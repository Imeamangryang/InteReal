#include "InteriorPlacementSubsystem.h"
#include "InteReal/EditMode/Visualization/PlacementVisualizerActor.h"
#include "InteReal/EditMode/Managers/GridSpaceManager.h"
#include "InteReal/EditMode/Placement/FloorPlacementHandler.h"
#include "InteReal/EditMode/Placement/WallPlacementHandler.h"
#include "InteReal/EditMode/Placement/CeilingPlacementHandler.h"
#include "InteReal/EditMode/Placement/SurfacePlacementHandler.h"
#include "InteReal/EditMode/History/PlacementHistoryHandler.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"

void UInteriorPlacementSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Surface → Wall → Ceiling → Floor 순 (구체적인 것부터 CanHandle)
	auto AddHandler = [&](auto* Handler)
	{
		Handler->Initialize(this);
		PlacementHandlers.Add(Handler);
	};

	AddHandler(NewObject<USurfacePlacementHandler>(this));
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

	// 바닥 Z 라인트레이스
	{
		FHitResult Hit;
		FCollisionQueryParams Params;
		if (GetWorld()->LineTraceSingleByChannel(Hit,
		                                         FVector(CenterX, CenterY, 100000.0f),
		                                         FVector(CenterX, CenterY, -100000.0f),
		                                         ECC_WorldStatic,
		                                         Params))
		{
			FloorZ = Hit.ImpactPoint.Z;
		}
		else if (!FloorData.faces.IsEmpty())
		{
			FloorZ = FloorData.faces[0].z_offset;
		}
	}

	const int32 Length  = FMath::CeilToInt((MaxX - MinX) / Cell);
	const int32 Breadth = FMath::CeilToInt((MaxY - MinY) / Cell);
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

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PreviewFurniture = GetWorld()->SpawnActor<AFurniture>(Visualizer->FurnitureClass, RayPosition, Rotation, Params);
	if (!PreviewFurniture)
	{
		return;
	}

	PreviewFurniture->ApplyFurnitureRow(InFurnitureRow);

	FHitResult InitHit;
	InitHit.Location     = RayPosition;
	InitHit.ImpactPoint  = RayPosition;
	InitHit.ImpactNormal = FVector::UpVector;
	UpdatePreviewLocation(InitHit);

	LineFillAnchor = PreviewGridAnchor;
}

void UInteriorPlacementSubsystem::UpdatePreviewLocation(const FHitResult& CursorHit)
{
	if (!PreviewFurniture || !Grid)
	{
		return;
	}

	LastRayPosition = CursorHit.ImpactPoint;

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

	FHitResult RotHit;
	RotHit.Location     = LastRayPosition;
	RotHit.ImpactPoint  = LastRayPosition;
	RotHit.ImpactNormal = FVector::UpVector;

	if (IPlacementHandler* Handler = GetActiveHandler())
	{
		Handler->UpdatePreview(PreviewFurniture, RotHit);
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
		return;
	}
	if (PreviewFurniture->GetPlacementState() == EPlacementState::Invalid)
	{
		return;
	}
	if (InvalidReason != EPlacementInvalidReason::None)
	{
		return;
	}

	const bool bIsFloor = (CurrentPreviewSurfaceType == EPlacementSurfaceType::Floor);
	if (bIsFloor && !IsPreviewLotEmpty())
	{
		return;
	}

	RecordUndoSnapshot();

	IPlacementHandler* Handler = GetActiveHandler();
	if (!Handler)
	{
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
		CreatePreviewFurnitureFromRow(LastRayPosition, PreviewRotation, CurrentFurnitureRow);
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

	const int32 L = (int32)CurrentDimensions.X;
	const int32 B = (int32)CurrentDimensions.Y;

	for (int32 i = 0; i < L; i++)
	{
		for (int32 j = 0; j < B; j++)
		{
			FVector2D Cell(PreviewGridAnchor.X + i, PreviewGridAnchor.Y + j);
			if (Grid->GetTileState(Cell) == EGridTileState::None)
			{
				return false;
			}
			AActor* Existing = Grid->GetFurniture(Cell);
			if (Existing && Existing != PreviewFurniture)
			{
				return false;
			}
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

	const float FloorThresholdZ = FloorZ + 5.0f;

	// 천장 지원 + Wall 미지원 가구: Z 높이로 천장 판단 (Floor 지원 여부 무관)
	if (PreviewFurniture
		&& PreviewFurniture->SupportsPlacementType(EPlacementSurfaceType::Ceiling)
		&& !PreviewFurniture->SupportsPlacementType(EPlacementSurfaceType::Wall)
		&& Hit.Location.Z > FloorThresholdZ)
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

	if (Hit.Location.Z > FloorThresholdZ)
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

	for (int32 i = 0; i < L; i++)
	{
		for (int32 j = 0; j < B; j++)
		{
			FVector2D Cell(GridAnchor.X + i, GridAnchor.Y + j);
			if (Grid->GetTileState(Cell) == EGridTileState::None)
			{
				return;
			}
			if (Grid->GetFurniture(Cell))
			{
				return;
			}
		}
	}

	FVector World = Grid->ToWorldPosition(FVector2D(
		(float)GridAnchor.X + ((float)L / 2.0f) - 0.5f,
		(float)GridAnchor.Y + ((float)B / 2.0f) - 0.5f));
	World.Z = FloorZ;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFurniture* NewFurniture = GetWorld()->SpawnActor<AFurniture>(Visualizer->FurnitureClass, World, Rotation, Params);
	if (!NewFurniture)
	{
		return;
	}

	NewFurniture->ApplyFurnitureRow(Row);

	// 도면 외부/벽 관통 체크는 FloorPlacementHandler의 유효성 검사 함수 재활용
	UFloorPlacementHandler* FloorHandler = Cast<UFloorPlacementHandler>(PlacementHandlers.Last());
	if (FloorHandler && (!FloorHandler->IsCornersInsideFloor(NewFurniture) || FloorHandler->IntersectsWalls(NewFurniture)))
	{
		NewFurniture->Destroy();
		return;
	}

	const FBox NewBounds = NewFurniture->GetCollisionBounds().ExpandBy(-1.0f);
	for (AFurniture* Placed : PlacedFurnitures)
	{
		if (!IsValid(Placed))
		{
			continue;
		}
		if (NewBounds.Intersect(Placed->GetCollisionBounds()))
		{
			NewFurniture->Destroy();
			return;
		}
	}

	for (int32 i = 0; i < L; i++)
	{
		for (int32 j = 0; j < B; j++)
		{
			Grid->SetFurniture(FVector2D(GridAnchor.X + i, GridAnchor.Y + j), NewFurniture);
		}
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
			else
			{
				Comp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
			}
		}
	}
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
	TSet<FString> OpeningEdgeIds;
	for (const FTopologyOpening& Opening : FloorData.openings)
	{
		OpeningEdgeIds.Add(Opening.target_edge_id);
		OpeningEdgeIds.Add(Opening.target_edge_id + TEXT("_twin"));
	}

	TMap<FString, FVector2D> VMap;
	for (const FTopologyVertex& V : FloorData.vertices)
	{
		VMap.Add(V.id, FloorData.ToHarnessPoint(V));
	}

	TSet<FString> ProcessedTwinIds;
	for (const FTopologyHalfEdge& Edge : FloorData.half_edges)
	{
		if (Edge.type != TEXT("WallInner") && Edge.type != TEXT("WallOuter"))
		{
			continue;
		}
		if (ProcessedTwinIds.Contains(Edge.id) || OpeningEdgeIds.Contains(Edge.id))
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
		WallSegments.Add(TPair<FVector2D, FVector2D>(*S, *E));
	}
}
