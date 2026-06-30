#include "InteriorPlacementSubsystem.h"
#include "InteReal/EditMode/Visualization/PlacementVisualizerActor.h"
#include "InteReal/EditMode/Furniture/LightFixture.h"
#include "InteReal/EditMode/Managers/GridSpaceManager.h"
#include "InteReal/EditMode/Placement/FloorPlacementHandler.h"
#include "InteReal/EditMode/Placement/DoorWindowPlacementHandler.h"
#include "InteReal/EditMode/Placement/WallPlacementHandler.h"
#include "InteReal/EditMode/Placement/CeilingPlacementHandler.h"
#include "InteReal/EditMode/Placement/SurfacePlacementHandler.h"
#include "InteReal/EditMode/History/PlacementHistoryHandler.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"

void UInteriorPlacementSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Surface → Door/Window → Wall → Ceiling → Floor 순 (구체적인 것부터 CanHandle)
	auto AddHandler = [&](auto* Handler)
	{
		Handler->Initialize(this);
		PlacementHandlers.Add(Handler);
	};

	AddHandler(NewObject<USurfacePlacementHandler>(this));
	AddHandler(NewObject<UDoorWindowPlacementHandler>(this));
	AddHandler(NewObject<UWallPlacementHandler>(this));
	AddHandler(NewObject<UCeilingPlacementHandler>(this));
	AddHandler(NewObject<UFloorPlacementHandler>(this));

	HistoryHandler = NewObject<UPlacementHistoryHandler>(this);
	HistoryHandler->Initialize(this);
}

void UInteriorPlacementSubsystem::Deinitialize()
{
	if (PreviewFurniture)
	{
		PreviewFurniture->Destroy();
		PreviewFurniture = nullptr;
	}
	Super::Deinitialize();
}

void UInteriorPlacementSubsystem::RegisterVisualizer(APlacementVisualizerActor* InVisualizer)
{
	Visualizer = InVisualizer;
	if (Visualizer)
	{
		GridCellSize  = Visualizer->GridCellSize;
		WallThickness = Visualizer->WallThickness;
	}
}

// ===== 초기화 =====

void UInteriorPlacementSubsystem::InitializeFromFloorData(const FHarnessFloorData& FloorData, float Cell)
{
	if (FloorData.vertices.IsEmpty())
	{
		return;
	}

	float MinX = TNumericLimits<float>::Max(), MaxX = TNumericLimits<float>::Lowest();
	float MinY = TNumericLimits<float>::Max(), MaxY = TNumericLimits<float>::Lowest();
	for (const FTopologyVertex& V : FloorData.vertices)
	{
		const FVector2D Point = FloorData.ToHarnessPoint(V);
		MinX = FMath::Min(MinX, Point.X);
		MaxX = FMath::Max(MaxX, Point.X);
		MinY = FMath::Min(MinY, Point.Y);
		MaxY = FMath::Max(MaxY, Point.Y);
	}

	const float CenterX = (MinX + MaxX) * 0.5f;
	const float CenterY = (MinY + MaxY) * 0.5f;

	// FloorData is authoritative. A top-down single trace hits the ceiling first.
	if (!FloorData.faces.IsEmpty())
	{
		FloorZ = FloorData.faces[0].z_offset;
		for (const FTopologyFace& Face : FloorData.faces)
		{
			FloorZ = FMath::Min(FloorZ, Face.z_offset);
		}
	}
	else
	{
		TArray<FHitResult> Hits;
		FCollisionQueryParams Params;
		GetWorld()->LineTraceMultiByChannel(
			Hits,
			FVector(CenterX, CenterY, 100000.0f),
			FVector(CenterX, CenterY, -100000.0f),
			ECC_WorldStatic,
			Params);
		for (const FHitResult& Hit : Hits)
		{
			const UPrimitiveComponent* Component = Hit.GetComponent();
			if (Component &&
				(Component->ComponentHasTag(TEXT("Floor")) || Component->ComponentHasTag(TEXT("EditableFloor"))))
			{
				FloorZ = Hit.ImpactPoint.Z;
				break;
			}
		}
	}

	Cell = FMath::Max(Cell, 1.0f);
	const int32 Length  = FMath::Max(1, FMath::CeilToInt((MaxX - MinX) / Cell));
	const int32 Breadth = FMath::Max(1, FMath::CeilToInt((MaxY - MinY) / Cell));
	InitializeGrid(Length, Breadth, Cell);

	if (Grid)
	{
		Grid->SetOrigin(FVector2D(CenterX, CenterY));
	}

	if (Visualizer)
	{
		Visualizer->SetActorLocation(FVector(CenterX, CenterY, FloorZ + 1.0f));
		const float TotalWidth  = MaxX - MinX;
		const float TotalHeight = MaxY - MinY;
		Visualizer->GridDecal->DecalSize = FVector(500.0f, TotalHeight * 0.5f + Cell, TotalWidth * 0.5f + Cell);
	}

	BuildFloorPolygon(FloorData);
	BuildWallSegments(FloorData);
	MarkOutOfBoundsTiles();

	if (Visualizer)
	{
		Visualizer->RebuildGridMesh(FloorRoomPolygons.IsEmpty() ? TArray<TArray<FVector2D>>{FloorPolygon} : FloorRoomPolygons,
		                            GridCellSize,
		                            FloorZ);
		SetGridVisible(true);
	}

	ApplyWallTraceCollision();
}

void UInteriorPlacementSubsystem::InitializeGrid(int32 Length, int32 Breadth, float Cell)
{
	GridCellSize = Cell;
	if (Visualizer)
	{
		Visualizer->GridCellSize = Cell;
	}

	if (Grid)
	{
		Grid->Destroy();
		Grid = nullptr;
	}
	Grid = GetWorld()->SpawnActor<AGridSpaceManager>(AGridSpaceManager::StaticClass());
	Grid->Initialize(Length, Breadth, Cell);
}

// ===== 그리드 =====

void UInteriorPlacementSubsystem::SetGridVisible(bool bVisible)
{
	if (Visualizer)
	{
		Visualizer->SetGridVisible(bVisible);
	}
}

void UInteriorPlacementSubsystem::SetLightFixtureIconsVisible(bool bVisible)
{
	bLightFixtureIconsPreferred = bVisible;
	ApplyLightFixtureIconsEffectiveState();
}

void UInteriorPlacementSubsystem::SetLightFixtureIconsEditModeActive(bool bActive)
{
	bLightFixtureIconsEditModeActive = bActive;
	ApplyLightFixtureIconsEffectiveState();
}

void UInteriorPlacementSubsystem::ApplyLightFixtureIconsEffectiveState()
{
	const bool bShow = AreLightFixtureIconsCurrentlyVisible();
	for (AFurniture* Furniture : PlacedFurnitures)
	{
		if (ALightFixture* Light = Cast<ALightFixture>(Furniture))
		{
			Light->SetIconForcedHidden(!bShow);
		}
	}
}

void UInteriorPlacementSubsystem::SetFreePlacementMode(bool bEnable)
{
	bFreePlacementMode = bEnable;
}

// ===== 프리뷰 =====

void UInteriorPlacementSubsystem::CreatePreviewFurnitureFromRow(FVector RayPosition,
                                                                FRotator Rotation,
                                                                const FFurnitureDataRow& InFurnitureRow)
{
	if (!Visualizer || !Visualizer->FurnitureClass)
	{
		return;
	}

	if (PreviewFurniture)
	{
		PreviewFurniture->Destroy();
		PreviewFurniture = nullptr;
	}

	CurrentDimensions    = FVector2D(InFurnitureRow.Dimensions.X, InFurnitureRow.Dimensions.Y);
	PreviewRotation      = Rotation;
	CurrentFurnitureRow  = InFurnitureRow;
	ActivePlacementHandler = nullptr;
	RebuildCurrentDimensionsFromPreviewRotation();

	const auto SupportsType = [&InFurnitureRow](EPlacementSurfaceType Type)
	{
		return (InFurnitureRow.AllowedPlacementTypes & static_cast<uint8>(Type)) != 0;
	};
	const bool bNeedsInitialFloorFallback =
		SupportsType(EPlacementSurfaceType::Floor);
	if (bNeedsInitialFloorFallback)
	{
		// The spawn API only receives a position, so a stale ceiling/wall hit has no
		// component or normal metadata. Use the floor until the next real cursor hit.
		RayPosition.Z = FloorZ;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const TSubclassOf<AFurniture> SpawnClass = AFurniture::ResolveSpawnClass(InFurnitureRow, Visualizer->FurnitureClass, Visualizer->LightFixtureClass);
	PreviewFurniture = GetWorld()->SpawnActor<AFurniture>(SpawnClass, RayPosition, Rotation, Params);
	if (!PreviewFurniture)
	{
		return;
	}

	PreviewFurniture->ApplyFurnitureRow(InFurnitureRow);
	RebuildCurrentDimensionsFromPreviewRotation();

	FHitResult InitHit;
	InitHit.Location     = RayPosition;
	InitHit.ImpactPoint  = RayPosition;
	InitHit.ImpactNormal = FVector::UpVector;
	UpdatePreviewLocation(InitHit);

	LineFillAnchor = PreviewGridAnchor;
}

void UInteriorPlacementSubsystem::SetPreviewHidden(bool bHidden)
{
	if (!PreviewFurniture)
	{
		return;
	}
	PreviewFurniture->SetActorHiddenInGame(bHidden);
	PreviewFurniture->SetActorEnableCollision(!bHidden);
}

void UInteriorPlacementSubsystem::UpdatePreviewLocation(const FHitResult& CursorHit)
{
	if (!PreviewFurniture || !Grid)
	{
		return;
	}

	LastRayPosition = CursorHit.ImpactPoint;
	LastPlacementHit = CursorHit;

	IPlacementHandler* Handler = FindHandlerForHit(CursorHit);
	if (!Handler)
	{
		InvalidReason = EPlacementInvalidReason::UnsupportedSurface;
		PreviewFurniture->SetPlacementState(EPlacementState::Invalid);
		if (Visualizer)
		{
			Visualizer->ClearPlacementCellViz();
		}
		return;
	}

	const EPlacementSurfaceType HitType = DetermineHitSurfaceType(CursorHit);
	CurrentPreviewSurfaceType = HitType;

	// 핸들러가 먼저 위치를 잡음 — 타입 미지원이어도 프리뷰는 표면을 따라가야 함
	ActivePlacementHandler = nullptr;
	for (UObject* HandlerObj : PlacementHandlers)
	{
		if (Cast<IPlacementHandler>(HandlerObj) == Handler)
		{
			ActivePlacementHandler = HandlerObj;
			break;
		}
	}
	Handler->UpdatePreview(PreviewFurniture, CursorHit);

	// 위치 결정 후 타입 지원 여부 확인 — 미지원이면 상태만 덮어씀 (배치 확정 불가)
	if (!PreviewFurniture->SupportsPlacementType(HitType))
	{
		InvalidReason = EPlacementInvalidReason::UnsupportedSurface;
		PreviewFurniture->SetPlacementState(EPlacementState::Invalid);
		if (Visualizer)
		{
			Visualizer->ClearPlacementCellViz();
		}
	}
}

void UInteriorPlacementSubsystem::RotatePreview(float AngleDeg)
{
	if (!PreviewFurniture)
	{
		return;
	}

	PreviewRotation.Yaw = FRotator::NormalizeAxis(PreviewRotation.Yaw + AngleDeg);
	PreviewFurniture->SetActorRotation(PreviewRotation);
	RebuildCurrentDimensionsFromPreviewRotation();

	if (IPlacementHandler* Handler = GetActiveHandler())
	{
		Handler->UpdatePreview(PreviewFurniture, LastPlacementHit);
	}
}

void UInteriorPlacementSubsystem::CancelPreview()
{
	if (PreviewFurniture)
	{
		PreviewFurniture->Destroy();
		PreviewFurniture = nullptr;
	}
	ActivePlacementHandler = nullptr;
	InvalidReason = EPlacementInvalidReason::None;
	if (Visualizer)
	{
		Visualizer->ClearPlacementCellViz();
	}
}

// ===== 배치 확정/제거 =====

void UInteriorPlacementSubsystem::ConfirmFurniture(bool bContinuePlacement)
{
	if (!PreviewFurniture || !Grid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Placement] Confirm rejected: preview=%s grid=%s"),
			PreviewFurniture ? TEXT("valid") : TEXT("null"), Grid ? TEXT("valid") : TEXT("null"));
		return;
	}
	if (PreviewFurniture->GetPlacementState() == EPlacementState::Invalid)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Placement] Confirm rejected: id=%d state=Invalid reason=%s surface=%d dims=%s bounds=%s anchor=%s"),
			PreviewFurniture->FurnitureID, *UEnum::GetValueAsString(InvalidReason),
			static_cast<int32>(CurrentPreviewSurfaceType), *CurrentDimensions.ToString(),
			*PreviewFurniture->GetCollisionBounds().GetSize().ToCompactString(), *PreviewGridAnchor.ToString());
		return;
	}
	if (InvalidReason != EPlacementInvalidReason::None)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Placement] Confirm rejected: id=%d reason=%s"),
			PreviewFurniture->FurnitureID, *UEnum::GetValueAsString(InvalidReason));
		return;
	}

	const bool bIsFloor = (CurrentPreviewSurfaceType == EPlacementSurfaceType::Floor);
	if (bIsFloor && !IsPreviewLotEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Placement] Confirm rejected: grid occupancy changed id=%d dims=%s anchor=%s"),
			PreviewFurniture->FurnitureID, *CurrentDimensions.ToString(), *PreviewGridAnchor.ToString());
		return;
	}

	RecordUndoSnapshot();

	IPlacementHandler* Handler = GetActiveHandler();
	if (!Handler)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Placement] Confirm rejected: no active handler id=%d surface=%d"),
			PreviewFurniture->FurnitureID, static_cast<int32>(CurrentPreviewSurfaceType));
		return;
	}

	// 라인 채우기 (바닥 전용)
	if (bIsFloor && bContinuePlacement && LineFillAnchor != PreviewGridAnchor)
	{
		const int32 L = (int32)CurrentDimensions.X;
		const int32 B = (int32)CurrentDimensions.Y;
		const FVector2D Delta  = PreviewGridAnchor - LineFillAnchor;
		const bool bAlongX     = FMath::Abs(Delta.X) >= FMath::Abs(Delta.Y);
		const int32 Step       = bAlongX ? FMath::Max(L, 1) : FMath::Max(B, 1);
		const int32 AxisDelta  = bAlongX ? (int32)Delta.X : (int32)Delta.Y;
		const int32 Dir        = AxisDelta > 0 ? 1 : -1;
		const int32 Count      = FMath::Max(0, (FMath::Abs(AxisDelta) / Step) - 1);

		for (int32 i = 1; i <= Count; i++)
		{
			FVector2D Anchor = LineFillAnchor;
			if (bAlongX)
			{
				Anchor.X += Dir * Step * i;
			}
			else
			{
				Anchor.Y += Dir * Step * i;
			}
			if (Anchor != PreviewGridAnchor)
			{
				PlaceFurnitureCopyAtGridAnchor(Anchor, CurrentDimensions, PreviewRotation, CurrentFurnitureRow);
			}
		}
	}

	Handler->OnConfirm(PreviewFurniture);

	PreviewFurniture->Tags.Add(TEXT("InteriorFurniture"));
	PreviewFurniture->Tags.Add(FName(FString::Printf(TEXT("ID_%d"), PreviewFurniture->FurnitureID)));
	PreviewFurniture->SetPlacementState(EPlacementState::Placed);
	PlacedFurnitures.Add(PreviewFurniture);
	PreviewFurniture = nullptr;

	if (Visualizer)
	{
		Visualizer->ClearPlacementCellViz();
	}

	if (bContinuePlacement)
	{
		const FHitResult ContinueHit = LastPlacementHit;
		CreatePreviewFurnitureFromRow(LastRayPosition, PreviewRotation, CurrentFurnitureRow);
		UpdatePreviewLocation(ContinueHit);
	}
}

void UInteriorPlacementSubsystem::RemoveFurniture(AFurniture* Target)
{
	if (!Target)
	{
		return;
	}
	RecordUndoSnapshot();
	DestroyFurnitureRecursive(Target);
}

void UInteriorPlacementSubsystem::DestroyFurnitureRecursive(AFurniture* Target)
{
	// 자식 가구 먼저 재귀 제거
	TArray<AFurniture*> Children;
	for (AFurniture* F : PlacedFurnitures)
	{
		if (IsValid(F) && F->ParentFurniture == Target)
		{
			Children.Add(F);
		}
	}
	for (AFurniture* Child : Children)
	{
		DestroyFurnitureRecursive(Child);
	}

	if (IPlacementHandler* Handler = FindHandlerForFurniture(Target))
	{
		Handler->OnRemove(Target);
	}

	PlacedFurnitures.Remove(Target);
	Target->Destroy();
}

// ===== 쿼리 =====

bool UInteriorPlacementSubsystem::IsPreviewLotEmpty() const
{
	if (!PreviewFurniture || !Grid)
	{
		return false;
	}

	TArray<FIntPoint> OccupiedCells;
	PreviewFurniture->GetOccupiedGridCells(Grid, PreviewGridAnchor, CurrentDimensions, OccupiedCells);
	for (const FIntPoint& Cell : OccupiedCells)
	{
		const FVector2D GridCell(Cell.X, Cell.Y);
		if (Grid->GetTileState(GridCell) == EGridTileState::None)
		{
			return false;
		}
		AActor* Existing = Grid->GetFurniture(GridCell);
		if (Existing && Existing != PreviewFurniture)
		{
			return false;
		}
	}
	return true;
}

const FFurnitureDataRow* UInteriorPlacementSubsystem::FindFurnitureRowByID(int32 TargetID) const
{
	if (!Visualizer || !Visualizer->FurnitureDataTable)
	{
		return nullptr;
	}

	static const FString ContextString(TEXT("FindFurnitureRowByID"));
	TArray<FFurnitureDataRow*> AllRows;
	Visualizer->FurnitureDataTable->GetAllRows<FFurnitureDataRow>(ContextString, AllRows);
	for (const FFurnitureDataRow* Row : AllRows)
	{
		if (Row && Row->ID == TargetID)
		{
			return Row;
		}
	}
	return nullptr;
}

bool UInteriorPlacementSubsystem::IsOverlappingPlacedFurniture(const AFurniture* Target,
                                                               const AFurniture* IgnoredFurniture,
                                                               const AFurniture* RequiredParent) const
{
	if (!Target)
	{
		return false;
	}

	const FBox TargetBounds = Target->GetCollisionBounds().ExpandBy(-1.0f);
	for (const AFurniture* Placed : PlacedFurnitures)
	{
		if (!IsValid(Placed) || Placed == Target || Placed == IgnoredFurniture)
		{
			continue;
		}
		if (RequiredParent && Placed->ParentFurniture != RequiredParent)
		{
			continue;
		}
		if (TargetBounds.Intersect(Placed->GetCollisionBounds()))
		{
			return true;
		}
	}
	return false;
}

// ===== 기즈모 =====

void UInteriorPlacementSubsystem::BeginGizmoMove(AFurniture* Target)
{
	if (!Target)
	{
		return;
	}
	HistoryHandler->BeginGizmoSnapshot();
	if (IPlacementHandler* Handler = FindHandlerForFurniture(Target))
	{
		Handler->BeginGizmoMove(Target);
	}
}

void UInteriorPlacementSubsystem::UpdateGizmoMoveLocation(FVector CursorOnGround, AFurniture* Target, EGizmoTransformAxis Axis)
{
	if (!Target)
	{
		return;
	}
	if (IPlacementHandler* Handler = FindHandlerForFurniture(Target))
	{
		Handler->UpdateGizmoMove(Target, CursorOnGround, Axis);
	}
}

void UInteriorPlacementSubsystem::UpdateGizmoMoveFree(FVector TargetWorldLocation, AFurniture* Target)
{
	if (!Target)
	{
		return;
	}
	if (IPlacementHandler* Handler = FindHandlerForFurniture(Target))
	{
		Handler->UpdateGizmoMoveFree(Target, TargetWorldLocation);
	}
}

void UInteriorPlacementSubsystem::FinalizeGizmoMove(AFurniture* Target)
{
	if (!Target)
	{
		return;
	}
	HistoryHandler->CommitGizmoSnapshot();
	if (IPlacementHandler* Handler = FindHandlerForFurniture(Target))
	{
		Handler->FinalizeGizmoMove(Target);
	}
}

void UInteriorPlacementSubsystem::AbortGizmoMove(AFurniture* Target)
{
	if (!Target)
	{
		return;
	}
	HistoryHandler->DiscardGizmoSnapshot();
	if (IPlacementHandler* Handler = FindHandlerForFurniture(Target))
	{
		Handler->AbortGizmoMove(Target);
	}
}

// ===== Undo/Redo =====

void UInteriorPlacementSubsystem::RecordUndoSnapshot()
{
	if (HistoryHandler)
	{
		HistoryHandler->RecordSnapshot();
	}
}

void UInteriorPlacementSubsystem::Undo()
{
	if (HistoryHandler)
	{
		HistoryHandler->Undo();
	}
}

void UInteriorPlacementSubsystem::Redo()
{
	if (HistoryHandler)
	{
		HistoryHandler->Redo();
	}
}

bool UInteriorPlacementSubsystem::CanUndo() const
{
	return HistoryHandler && HistoryHandler->CanUndo();
}

bool UInteriorPlacementSubsystem::CanRedo() const
{
	return HistoryHandler && HistoryHandler->CanRedo();
}

// ===== JSON =====

FString UInteriorPlacementSubsystem::ExportPlacedFurnituresJson() const
{
	return HistoryHandler ? HistoryHandler->ExportPlacedFurnitureJson() : FString();
}

void UInteriorPlacementSubsystem::ImportPlacedFurnituresJson(const FString& Json)
{
	if (HistoryHandler)
	{
		HistoryHandler->ImportPlacedFurnitureJson(Json);
	}
}

FString UInteriorPlacementSubsystem::ExportEditStateJson() const
{
	return HistoryHandler ? HistoryHandler->ExportEditStateJson() : FString();
}

void UInteriorPlacementSubsystem::ImportEditStateJson(const FString& Json)
{
	if (HistoryHandler)
	{
		HistoryHandler->ImportEditStateJson(Json);
	}
}

void UInteriorPlacementSubsystem::ReceiveWebCommand(const FString& JsonString)
{
	if (HistoryHandler)
	{
		HistoryHandler->ReceiveWebCommand(JsonString);
	}
}

// ===== 내부 헬퍼 =====

EPlacementSurfaceType UInteriorPlacementSubsystem::DetermineHitSurfaceType(const FHitResult& Hit) const
{
	const UPrimitiveComponent* HitComp = Hit.GetComponent();

	if (HitComp && HitComp->ComponentHasTag(TEXT("Ceiling")))
	{
		return EPlacementSurfaceType::Ceiling;
	}

	// 법선이 아래 방향 = 천장 표면 (태그 없어도 감지)
	if (Hit.ImpactNormal.Z < -0.5f
		&& PreviewFurniture
		&& PreviewFurniture->SupportsPlacementType(EPlacementSurfaceType::Ceiling))
	{
		return EPlacementSurfaceType::Ceiling;
	}

	if (const AFurniture* HitFurniture = Cast<AFurniture>(Hit.GetActor()))
	{
		if (HitFurniture->GetPlacementState() == EPlacementState::Placed)
		{
			return EPlacementSurfaceType::Surface;
		}
	}

	if (HitComp && HitComp->ComponentHasTag(TEXT("EditableWall")))
	{
		return EPlacementSurfaceType::Wall;
	}

	return EPlacementSurfaceType::Floor;
}

IPlacementHandler* UInteriorPlacementSubsystem::FindHandlerForHit(const FHitResult& Hit) const
{
	for (UObject* HandlerObj : PlacementHandlers)
	{
		if (IPlacementHandler* Handler = Cast<IPlacementHandler>(HandlerObj))
		{
			if (Handler->CanHandle(Hit))
			{
				return Handler;
			}
		}
	}
	return nullptr;
}

IPlacementHandler* UInteriorPlacementSubsystem::FindHandlerForFurniture(const AFurniture* Furniture) const
{
	for (UObject* HandlerObj : PlacementHandlers)
	{
		if (IPlacementHandler* Handler = Cast<IPlacementHandler>(HandlerObj))
		{
			if (Handler->OwnsFurniture(Furniture))
			{
				return Handler;
			}
		}
	}
	return nullptr;
}

IPlacementHandler* UInteriorPlacementSubsystem::GetActiveHandler() const
{
	if (ActivePlacementHandler)
	{
		if (IPlacementHandler* Handler = Cast<IPlacementHandler>(ActivePlacementHandler))
		{
			return Handler;
		}
	}

	if (PlacementHandlers.IsEmpty())
	{
		return nullptr;
	}

	FHitResult FakeHit;
	FakeHit.Location     = LastRayPosition;
	FakeHit.ImpactPoint  = LastRayPosition;
	FakeHit.ImpactNormal = FVector::UpVector;

	for (UObject* HandlerObj : PlacementHandlers)
	{
		if (IPlacementHandler* Handler = Cast<IPlacementHandler>(HandlerObj))
		{
			if (Handler->CanHandle(FakeHit))
			{
				return Handler;
			}
		}
	}
	return nullptr;
}

void UInteriorPlacementSubsystem::RebuildCurrentDimensionsFromPreviewRotation()
{
	if (PreviewFurniture && GridCellSize > KINDA_SMALL_NUMBER)
	{
		const FVector Size = PreviewFurniture->GetCollisionBounds().GetSize();
		CurrentDimensions = FVector2D(
			FMath::Max(1, FMath::CeilToInt(Size.X / GridCellSize)),
			FMath::Max(1, FMath::CeilToInt(Size.Y / GridCellSize)));
		return;
	}

	CurrentDimensions = FVector2D(CurrentFurnitureRow.Dimensions.X, CurrentFurnitureRow.Dimensions.Y);

	const float NormalizedYaw = FRotator::NormalizeAxis(PreviewRotation.Yaw);
	const float AbsYaw = FMath::Abs(NormalizedYaw);
	const bool bQuarterTurn =
		FMath::IsNearlyEqual(AbsYaw, 90.0f, 1.0f) ||
		FMath::IsNearlyEqual(AbsYaw, 270.0f, 1.0f);

	if (bQuarterTurn)
	{
		Swap(CurrentDimensions.X, CurrentDimensions.Y);
	}
}

void UInteriorPlacementSubsystem::PlaceFurnitureCopyAtGridAnchor(FVector2D GridAnchor,
                                                                 FVector2D Dims,
                                                                 FRotator Rotation,
                                                                 const FFurnitureDataRow& Row)
{
	if (!Grid || !Visualizer || !Visualizer->FurnitureClass)
	{
		return;
	}

	const int32 L = (int32)Dims.X;
	const int32 B = (int32)Dims.Y;

	FVector World = Grid->ToWorldPosition(FVector2D(
		(float)GridAnchor.X + ((float)L / 2.0f) - 0.5f,
		(float)GridAnchor.Y + ((float)B / 2.0f) - 0.5f));
	World.Z = FloorZ;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const TSubclassOf<AFurniture> SpawnClass = AFurniture::ResolveSpawnClass(Row, Visualizer->FurnitureClass, Visualizer->LightFixtureClass);
	AFurniture* NewFurniture = GetWorld()->SpawnActor<AFurniture>(SpawnClass, World, Rotation, Params);
	if (!NewFurniture)
	{
		return;
	}

	NewFurniture->ApplyFurnitureRow(Row);
	NewFurniture->AlignPlacementBottomCenterTo(World, FloorZ);
	NewFurniture->GetOccupiedGridCells(Grid, GridAnchor, Dims, NewFurniture->PlacedOccupiedCells);
	for (const FIntPoint& Cell : NewFurniture->PlacedOccupiedCells)
	{
		const FVector2D GridCell(Cell.X, Cell.Y);
		if (Grid->GetTileState(GridCell) == EGridTileState::None || Grid->GetFurniture(GridCell))
		{
			NewFurniture->Destroy();
			return;
		}
	}

	// 도면 외부/벽 관통 체크는 FloorPlacementHandler의 유효성 검사 함수 재활용
	UFloorPlacementHandler* FloorHandler = Cast<UFloorPlacementHandler>(PlacementHandlers.Last());
	if (FloorHandler && (!FloorHandler->IsCornersInsideFloor(NewFurniture) || FloorHandler->IntersectsWalls(NewFurniture)))
	{
		NewFurniture->Destroy();
		return;
	}

	for (const FIntPoint& Cell : NewFurniture->PlacedOccupiedCells)
	{
		Grid->SetFurniture(FVector2D(Cell.X, Cell.Y), NewFurniture);
	}

	NewFurniture->PlacedGridAnchor = GridAnchor;
	NewFurniture->PlacedDimensions = Dims;
	NewFurniture->SetPlacedSurfaceType(EPlacementSurfaceType::Floor);
	NewFurniture->Tags.Add(TEXT("InteriorFurniture"));
	NewFurniture->Tags.Add(FName(FString::Printf(TEXT("ID_%d"), NewFurniture->FurnitureID)));
	NewFurniture->SetPlacementState(EPlacementState::Placed);
	PlacedFurnitures.Add(NewFurniture);
}

void UInteriorPlacementSubsystem::ApplyWallTraceCollision()
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TArray<UPrimitiveComponent*> Components;
		It->GetComponents<UPrimitiveComponent>(Components);
		for (UPrimitiveComponent* Comp : Components)
		{
			if (Comp->ComponentHasTag(TEXT("EditableWall")))
			{
				Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				Comp->SetCollisionObjectType(ECC_WorldStatic);
				Comp->SetCollisionResponseToAllChannels(ECR_Block);
				Comp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
				Comp->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
				Comp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
				Comp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			}
		}
	}
}

void UInteriorPlacementSubsystem::UpdateGizmoRotation(AFurniture* Target)
{
	if (!Target || Target->GetPlacedSurfaceType() != EPlacementSurfaceType::Floor)
	{
		return;
	}

	IPlacementHandler* Handler = FindHandlerForFurniture(Target);
	if (!Handler || GridCellSize <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector Size = Target->GetCollisionBounds().GetSize();
	Target->PlacedDimensions = FVector2D(
		FMath::Max(1, FMath::CeilToInt(Size.X / GridCellSize)),
		FMath::Max(1, FMath::CeilToInt(Size.Y / GridCellSize)));
	Handler->UpdateGizmoMove(Target, Target->GetMeshBounds().GetCenter(), EGizmoTransformAxis::None);
}

void UInteriorPlacementSubsystem::ClearAllFurniture()
{
	CancelPreview();
	for (AFurniture* Furniture : PlacedFurnitures)
	{
		if (IsValid(Furniture))
		{
			Furniture->Destroy();
		}
	}
	PlacedFurnitures.Empty();
	if (Grid)
	{
		Grid->ClearFurnitureOccupancy();
	}
	InvalidReason = EPlacementInvalidReason::None;
}

void UInteriorPlacementSubsystem::MarkOutOfBoundsTiles()
{
	if (!Grid || (FloorPolygon.Num() < 3 && FloorRoomPolygons.IsEmpty()))
	{
		return;
	}
	for (int32 x = 0; x < Grid->GetLength(); x++)
	{
		for (int32 y = 0; y < Grid->GetBreadth(); y++)
		{
			FVector WorldPos = Grid->ToWorldPosition(FVector2D(x, y));
			Grid->SetTileState(FVector2D(x, y),
			                   IsPointInsideFloor(FVector2D(WorldPos.X, WorldPos.Y))
				                   ? EGridTileState::Walkable
				                   : EGridTileState::None);
		}
	}
}

bool UInteriorPlacementSubsystem::IsPointInPolygon(FVector2D Point, const TArray<FVector2D>& Polygon)
{
	if (Polygon.Num() < 3)
	{
		return true;
	}
	bool bInside = false;
	const int32 N = Polygon.Num();
	for (int32 i = 0, j = N - 1; i < N; j = i++)
	{
		if (((Polygon[i].Y > Point.Y) != (Polygon[j].Y > Point.Y)) &&
			(Point.X < (Polygon[j].X - Polygon[i].X) * (Point.Y - Polygon[i].Y) /
				(Polygon[j].Y - Polygon[i].Y) + Polygon[i].X))
		{
			bInside = !bInside;
		}
	}
	return bInside;
}

bool UInteriorPlacementSubsystem::IsPointInsideFloor(FVector2D Point) const
{
	if (!FloorRoomPolygons.IsEmpty())
	{
		for (const TArray<FVector2D>& RoomPolygon : FloorRoomPolygons)
		{
			if (IsPointInPolygon(Point, RoomPolygon))
			{
				return true;
			}
		}
		return false;
	}

	return IsPointInPolygon(Point, FloorPolygon);
}

void UInteriorPlacementSubsystem::BuildFloorPolygon(const FHarnessFloorData& FloorData)
{
	FloorPolygon.Empty();
	FloorRoomPolygons.Empty();

	TMap<FString, FVector2D> VMap;
	for (const FTopologyVertex& V : FloorData.vertices)
	{
		VMap.Add(V.id, FloorData.ToHarnessPoint(V));
	}

	for (const FTopologyFace& Face : FloorData.faces)
	{
		TArray<FVector2D> RoomPolygon;
		for (const FString& VertexId : Face.contour_vertex_ids)
		{
			if (const FVector2D* Pos = VMap.Find(VertexId))
			{
				RoomPolygon.Add(*Pos);
			}
		}
		if (RoomPolygon.Num() >= 3)
		{
			FloorRoomPolygons.Add(RoomPolygon);
		}
	}

	TSet<FString> OuterIds;
	for (const FTopologyHalfEdge& Edge : FloorData.half_edges)
	{
		if (Edge.type == TEXT("WallOuter"))
		{
			OuterIds.Add(Edge.vertex_start);
			OuterIds.Add(Edge.vertex_end);
		}
	}
	if (OuterIds.IsEmpty())
	{
		for (const FTopologyVertex& V : FloorData.vertices)
		{
			OuterIds.Add(V.id);
		}
	}

	TArray<FVector2D> Points;
	for (const FString& Id : OuterIds)
	{
		if (const FVector2D* Pos = VMap.Find(Id))
		{
			Points.Add(*Pos);
		}
	}

	if (Points.IsEmpty())
	{
		return;
	}

	FVector2D Centroid(0, 0);
	for (const FVector2D& P : Points)
	{
		Centroid += P;
	}
	Centroid /= (float)Points.Num();

	Points.Sort([&Centroid](const FVector2D& A, const FVector2D& B)
	{
		return FMath::Atan2(A.Y - Centroid.Y, A.X - Centroid.X) <
			FMath::Atan2(B.Y - Centroid.Y, B.X - Centroid.X);
	});
	FloorPolygon = Points;
}

void UInteriorPlacementSubsystem::BuildWallSegments(const FHarnessFloorData& FloorData)
{
	WallSegments.Empty();

	TMap<FString, FVector2D> VMap;
	for (const FTopologyVertex& V : FloorData.vertices)
	{
		VMap.Add(V.id, FloorData.ToHarnessPoint(V));
	}

	// 1. Gather raw wall segments from half edges (deduplicating twins)
	struct FRawSeg
	{
		FVector2D Start;
		FVector2D End;
		FVector2D Dir;
		bool bMerged = false;
	};
	TArray<FRawSeg> RawSegs;

	TSet<FString> ProcessedTwinIds;
	for (const FTopologyHalfEdge& Edge : FloorData.half_edges)
	{
		if (Edge.type != TEXT("WallInner") && Edge.type != TEXT("WallOuter"))
		{
			continue;
		}
		if (ProcessedTwinIds.Contains(Edge.id))
		{
			continue;
		}
		ProcessedTwinIds.Add(Edge.twin_id);

		const FVector2D* S = VMap.Find(Edge.vertex_start);
		const FVector2D* E = VMap.Find(Edge.vertex_end);
		if (!S || !E || FVector2D::DistSquared(*S, *E) < 1.0f)
		{
			continue;
		}

		FRawSeg Seg;
		Seg.Start = *S;
		Seg.End = *E;
		Seg.Dir = (*E - *S).GetSafeNormal();
		RawSegs.Add(Seg);
	}

	// 2. Perform collinear and adjacent segment merging
	bool bMergedAny = true;
	const float CollinearTolerance = 0.02f; // cross product tolerance for parallel lines
	const float DistTolerance = 5.0f;       // vertical line distance tolerance in cm (5cm)
	const float MaxGapToBridge = 300.0f;    // max door/window span to bridge in cm (3m)

	while (bMergedAny)
	{
		bMergedAny = false;
		for (int32 i = 0; i < RawSegs.Num(); ++i)
		{
			if (RawSegs[i].bMerged) continue;

			for (int32 j = i + 1; j < RawSegs.Num(); ++j)
			{
				if (RawSegs[j].bMerged) continue;

				// A. Check direction collinearity
				float Cross = FMath::Abs(RawSegs[i].Dir.X * RawSegs[j].Dir.Y - RawSegs[i].Dir.Y * RawSegs[j].Dir.X);
				if (Cross > CollinearTolerance) continue;

				// B. Check perpendicular distance from raw line i to segment j endpoints
				FVector2D Normal(-RawSegs[i].Dir.Y, RawSegs[i].Dir.X);
				float DistS = FMath::Abs(FVector2D::DotProduct(RawSegs[j].Start - RawSegs[i].Start, Normal));
				float DistE = FMath::Abs(FVector2D::DotProduct(RawSegs[j].End - RawSegs[i].Start, Normal));
				if (DistS > DistTolerance || DistE > DistTolerance) continue;

				// C. Project segment j onto raw line i to check gap/overlap
				float MinI = 0.0f;
				float MaxI = FVector2D::Distance(RawSegs[i].Start, RawSegs[i].End);

				float ProjectS = FVector2D::DotProduct(RawSegs[j].Start - RawSegs[i].Start, RawSegs[i].Dir);
				float ProjectE = FVector2D::DotProduct(RawSegs[j].End - RawSegs[i].Start, RawSegs[i].Dir);

				float MinJ = FMath::Min(ProjectS, ProjectE);
				float MaxJ = FMath::Max(ProjectS, ProjectE);

				// Calculate gap between segments
				float Gap = 0.0f;
				if (MinJ > MaxI)
				{
					Gap = MinJ - MaxI;
				}
				else if (MinI > MaxJ)
				{
					Gap = MinI - MaxJ;
				}

				// If the gap is too wide (e.g. they belong to completely different wall sections), skip merging
				if (Gap > MaxGapToBridge)
				{
					continue;
				}

				// Merge the segments by spanning the outermost endpoints
				float NewMin = FMath::Min(MinI, MinJ);
				float NewMax = FMath::Max(MaxI, MaxJ);

				RawSegs[i].Start = RawSegs[i].Start + RawSegs[i].Dir * NewMin;
				RawSegs[i].End = RawSegs[i].Start + RawSegs[i].Dir * (NewMax - NewMin);
				RawSegs[i].Dir = (RawSegs[i].End - RawSegs[i].Start).GetSafeNormal();

				RawSegs[j].bMerged = true;
				bMergedAny = true;
			}
		}
	}

	// 3. Register final merged segments
	for (const FRawSeg& Seg : RawSegs)
	{
		if (!Seg.bMerged)
		{
			WallSegments.Add(TPair<FVector2D, FVector2D>(Seg.Start, Seg.End));
		}
	}
}
