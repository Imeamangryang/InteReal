#include "InteRealPlayerController.h"
#include "InteRealHUD.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "InteReal/EditMode/Gizmo/InteRealGizmoComponent.h"
#include "InteReal/EditMode/Furniture/LightFixture.h"
#include "InteReal/EditMode/2D/InteReal2DFloorPlanViewportWidget.h"
#include "InteReal/ViewMode/ViewModeManager.h"
#include "InteReal/Harness/Public/HarnessPipelineManager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "CollisionShape.h"
#include "Kismet/GameplayStatics.h"
#include "Components/Overlay.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "InteReal/SubSystems/InteRealUISubSystem.h"
#include "Engine/GameInstance.h"
#include "Components/InteRealFloorPlanPlacementSyncComponent.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "UI/SubWidgets/EditModeLayoutWidget.h"
#include "Widgets/SViewport.h"
#include "Engine/Texture2D.h"

static bool IsOpeningAssetRowForReplacement(const FFurnitureDataRow& Row)
{
	if (Row.AssetKind != EPlacementAssetKind::Generic)
	{
		return true;
	}

	const FString Label = Row.DisplayName.ToString().ToLower();
	return Label.Contains(TEXT("door")) ||
		Label.Contains(TEXT("window")) ||
		Label.Contains(TEXT("문")) ||
		Label.Contains(TEXT("창")) ||
		Label.Contains(TEXT("현관")) ||
		Label.Contains(TEXT("미닫이")) ||
		Label.Contains(TEXT("슬라이딩"));
}

static bool OpeningComponentAcceptsRow(const UPrimitiveComponent* Component, const FFurnitureDataRow& Row)
{
	if (!Component || !Component->ComponentHasTag(TEXT("EditableOpening")) || !IsOpeningAssetRowForReplacement(Row))
	{
		return false;
	}

	const FString Label = Row.DisplayName.ToString().ToLower();
	const bool bRowLooksWindow =
		Row.AssetKind == EPlacementAssetKind::Window ||
		Label.Contains(TEXT("window")) ||
		Label.Contains(TEXT("창"));
	const bool bComponentIsWindow = Component->ComponentHasTag(TEXT("WindowAsset"));
	const bool bComponentIsDoor = Component->ComponentHasTag(TEXT("DoorAsset"));

	return bRowLooksWindow ? bComponentIsWindow : bComponentIsDoor;
}

static bool TryGetComponentTagFloat(const UActorComponent* Component, const TCHAR* Prefix, float& OutValue)
{
	if (!Component || !Prefix)
	{
		return false;
	}

	const FString PrefixString(Prefix);
	for (const FName& TagName : Component->ComponentTags)
	{
		const FString Tag = TagName.ToString();
		if (Tag.StartsWith(PrefixString))
		{
			OutValue = FCString::Atof(*Tag.Mid(PrefixString.Len()));
			return true;
		}
	}
	return false;
}

static void ApplyOpeningMeshPreservingSlot(UStaticMeshComponent* OpeningComp, UStaticMesh* NewMesh)
{
	if (!OpeningComp || !NewMesh)
	{
		return;
	}

	float SlotWidth = 0.0f;
	float SlotHeight = 0.0f;
	float SlotDepth = 20.0f;
	TryGetComponentTagFloat(OpeningComp, TEXT("HarnessOpeningWidthCm="), SlotWidth);
	TryGetComponentTagFloat(OpeningComp, TEXT("HarnessOpeningHeightCm="), SlotHeight);
	TryGetComponentTagFloat(OpeningComp, TEXT("HarnessOpeningDepthCm="), SlotDepth);

	OpeningComp->SetStaticMesh(NewMesh);

	const FVector MeshSize = NewMesh->GetBounds().GetBox().GetSize();
	FVector NewScale = FVector::OneVector;
	if (SlotWidth > KINDA_SMALL_NUMBER && MeshSize.X > KINDA_SMALL_NUMBER)
	{
		NewScale.X = SlotWidth / MeshSize.X;
	}
	if (SlotDepth > KINDA_SMALL_NUMBER && MeshSize.Y > KINDA_SMALL_NUMBER)
	{
		NewScale.Y = FMath::Min(1.0f, SlotDepth * 0.85f / MeshSize.Y);
	}
	if (SlotHeight > KINDA_SMALL_NUMBER && MeshSize.Z > KINDA_SMALL_NUMBER)
	{
		NewScale.Z = SlotHeight / MeshSize.Z;
	}
	OpeningComp->SetRelativeScale3D(NewScale);
	OpeningComp->MarkRenderStateDirty();
}

static bool IsMouseOverInteractiveUI()
{
	if (!FSlateApplication::IsInitialized() || !GEngine || !GEngine->GameViewport)
	{
		return false;
	}

	FSlateApplication& SlateApplication = FSlateApplication::Get();
	const FWidgetPath WidgetPath = SlateApplication.LocateWindowUnderMouse(
		SlateApplication.GetCursorPos(),
		SlateApplication.GetInteractiveTopLevelWindows());
	const TSharedPtr<SViewport> GameViewportWidget = GEngine->GameViewport->GetGameViewportWidget();
	return WidgetPath.IsValid() &&
		GameViewportWidget.IsValid() &&
		WidgetPath.GetLastWidget() != GameViewportWidget.ToSharedRef();
}


static bool IsFirstPersonPawnLocationBlocked(UWorld* World, const APawn* Pawn, const FVector& Location)
{
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FirstPersonSafeLocation), false);
	if (Pawn)
	{
		QueryParams.AddIgnoredActor(Pawn);
	}

	const float PawnRadius = 34.0f;
	const float PawnHalfHeight = 88.0f;
	return World->OverlapBlockingTestByChannel(Location, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(PawnRadius, PawnHalfHeight), QueryParams);
}

static FVector ResolveFirstPersonSafeLocation(UWorld* World, const APawn* Pawn, const FVector& DesiredLocation, const FRotator& FacingRotation, float MaxSearchRadius = 125.0f)
{
	if (!IsFirstPersonPawnLocationBlocked(World, Pawn, DesiredLocation))
	{
		return DesiredLocation;
	}

	// 천장????? ?��? 공간?�서??XY�?먼�? 밀�?건물 �??�보가 ?�택?????�으므�?
	// 같�? ?��? 좌표?�서 Z�?먼�? ??�� 캡슐 ?�단??천장�?겹치지 ?�는 ?�이�?찾는??
	for (float LowerOffset = 10.0f; LowerOffset <= 80.0f; LowerOffset += 10.0f)
	{
		FVector LoweredLocation = DesiredLocation;
		LoweredLocation.Z = DesiredLocation.Z - LowerOffset;
		if (!IsFirstPersonPawnLocationBlocked(World, Pawn, LoweredLocation))
		{
			return LoweredLocation;
		}
	}

	const FVector Forward = FRotationMatrix(FRotator(0.0f, FacingRotation.Yaw, 0.0f)).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(FRotator(0.0f, FacingRotation.Yaw, 0.0f)).GetUnitAxis(EAxis::Y);
	TArray<FVector, TInlineAllocator<16>> SearchDirections;
	SearchDirections.Add(Forward);
	SearchDirections.Add(-Forward);
	SearchDirections.Add(Right);
	SearchDirections.Add(-Right);
	SearchDirections.Add((Forward + Right).GetSafeNormal());
	SearchDirections.Add((Forward - Right).GetSafeNormal());
	SearchDirections.Add((-Forward + Right).GetSafeNormal());
	SearchDirections.Add((-Forward - Right).GetSafeNormal());

	FVector BestLocation = DesiredLocation;
	float BestScore = TNumericLimits<float>::Max();
	for (float Radius = 25.0f; Radius <= MaxSearchRadius; Radius += 25.0f)
	{
		for (const FVector& Direction : SearchDirections)
		{
			for (float LowerOffset = 0.0f; LowerOffset <= 80.0f; LowerOffset += 20.0f)
			{
				FVector Candidate = DesiredLocation + Direction * Radius;
				Candidate.Z = DesiredLocation.Z - LowerOffset;
				if (!IsFirstPersonPawnLocationBlocked(World, Pawn, Candidate))
				{
					const float DistSq = FVector::DistSquared2D(DesiredLocation, Candidate);
					const float Score = DistSq + LowerOffset * LowerOffset * 0.25f;
					if (Score < BestScore)
					{
						BestScore = Score;
						BestLocation = Candidate;
					}
				}
			}
		}

		if (BestScore < TNumericLimits<float>::Max())
		{
			return BestLocation;
		}
	}

	return DesiredLocation;
}

AInteRealPlayerController::AInteRealPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	PrimaryActorTick.bCanEverTick = true;
	
	FloorPlanPlacementSyncComponent = CreateDefaultSubobject<UInteRealFloorPlanPlacementSyncComponent>(TEXT("FloorPlanPlacementSyncComponent"));
}

void AInteRealPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	FindViewModeManager();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UInteRealUISubSystem* UISubsystem = GI->GetSubsystem<UInteRealUISubSystem>())
		{
			UISubsystem->OnModeChanged.AddDynamic(this, &AInteRealPlayerController::HandleModeChanged);
			UISubsystem->OnFurnitureSpawn.AddDynamic(this, &AInteRealPlayerController::HandleFurnitureSpawn);
			UISubsystem->OnIconClicked.AddDynamic(this, &AInteRealPlayerController::HandleIconClicked);
			UISubsystem->OnWallMaterialDataChanged.RemoveDynamic(this, &AInteRealPlayerController::HandleWallMaterialDataChanged);
			UISubsystem->OnWallMaterialDataChanged.AddDynamic(this, &AInteRealPlayerController::HandleWallMaterialDataChanged);
		}
	}

	if (APawn* P = GetPawn())
	{
		P->SetActorHiddenInGame(true);
	}

	ApplyCurrentControlMode();
	
	if (FloorPlanPlacementSyncComponent)
	{
		FloorPlanPlacementSyncComponent->Initialize(this);
		FloorPlanPlacementSyncComponent->BindFloorPlan2DEvents();
	}
	
	if (UHarnessPipelineManager* Pipeline = GetWorld()->GetSubsystem<UHarnessPipelineManager>())
	{
		Pipeline->OnPipelineLoadFinished.RemoveDynamic(this, &AInteRealPlayerController::HandlePipelineLoadFinished);
		Pipeline->OnPipelineLoadFinished.AddDynamic(this, &AInteRealPlayerController::HandlePipelineLoadFinished);
	}

	if (CachedViewModeManager)
	{
		SetViewMode(EHarnessViewMode::Isometric);
	}

	// ??BeginPlay?�서 LayoutWidget 바인??+ 초기 ?�프???�용
	if (AInteRealHUD* HUD = GetInteRealHUD())
	{
		if (UEditModeLayoutWidget* LayoutWidget = HUD->EditModeLayoutWidgetInstance)
		{
			LayoutWidget->OnFloorPlanPanelOpenChanged.RemoveDynamic(
				this, &AInteRealPlayerController::HandleFloorPlanPanelOpenChanged);
			LayoutWidget->OnFloorPlanPanelOpenChanged.AddDynamic(
				this, &AInteRealPlayerController::HandleFloorPlanPanelOpenChanged);

			// 초기 ?�널 ?�태 즉시 ?�용
			HandleFloorPlanPanelOpenChanged(LayoutWidget->IsFloorPlanPanelOpen());
		}
	}
}

void AInteRealPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (!bFloorPlanOffsetInitialized)
	{
		if (AInteRealHUD* HUD = GetInteRealHUD())
		{
			if (UEditModeLayoutWidget* LayoutWidget = HUD->EditModeLayoutWidgetInstance)
			{
				LayoutWidget->OnFloorPlanPanelOpenChanged.RemoveDynamic(
					this, &AInteRealPlayerController::HandleFloorPlanPanelOpenChanged);
				LayoutWidget->OnFloorPlanPanelOpenChanged.AddDynamic(
					this, &AInteRealPlayerController::HandleFloorPlanPanelOpenChanged);

				HandleFloorPlanPanelOpenChanged(LayoutWidget->IsFloorPlanPanelOpen());
				bFloorPlanOffsetInitialized = true;
			}
		}
	}

	if (bIsFocusingFirstPerson)
	{
		if (APawn* P = GetPawn())
		{
			const FVector NewLoc = FMath::VInterpTo(P->GetActorLocation(), FirstPersonFocusTarget, DeltaTime, 8.0f);
			FHitResult SweepHit;
			P->SetActorLocation(NewLoc, true, &SweepHit);
			if (SweepHit.bBlockingHit || FVector::DistSquared2D(P->GetActorLocation(), FirstPersonFocusTarget) < 4.0f)
			{
				bIsFocusingFirstPerson = false;
			}
		}
	}

	if (CurrentControlMode != EInteRealControlMode::Edit)
	{
		if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
		{
			InteRealHUD->UpdatePlacementTooltip(
				false,
				false,
				EPlacementInvalidReason::None,
				FVector2D::ZeroVector
			);
			InteRealHUD->UpdateRotationGuide(false, 0.0f, FVector2D::ZeroVector);
		}

		return;
	}

	UpdateCursorHit();

	if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
	{
		UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
		const bool bHasPreview = PS && PS->HasActivePreview();
		const EPlacementInvalidReason InvalidReason = PS ? PS->InvalidReason : EPlacementInvalidReason::None;

		float MouseX = 0.f;
		float MouseY = 0.f;
		UWidgetLayoutLibrary::GetMousePositionScaledByDPI(this, MouseX, MouseY);

		// 가�배��OR 기존 가굴동 중일 �팁 �시
		InteRealHUD->UpdatePlacementTooltip(
			true,
			bHasPreview || bIsMovingFurniture,
			InvalidReason,
			FVector2D(MouseX, MouseY)
		);

		const bool bShowGuide = bHasPreview || bIsMovingFurniture;
		AFurniture* GuideFurniture = bIsMovingFurniture ? SelectedFurniture.Get() : (PS ? PS->GetPreviewFurniture() : nullptr);
		if (bShowGuide && GuideFurniture)
		{
			FVector2D GuideScreenPos;
			bool bFound = false;
			{
				const FBox BBox = GuideFurniture->GetComponentsBoundingBox(true);
				const FVector Corners[8] = {
					{BBox.Min.X, BBox.Min.Y, BBox.Min.Z}, {BBox.Max.X, BBox.Min.Y, BBox.Min.Z},
					{BBox.Min.X, BBox.Max.Y, BBox.Min.Z}, {BBox.Max.X, BBox.Max.Y, BBox.Min.Z},
					{BBox.Min.X, BBox.Min.Y, BBox.Max.Z}, {BBox.Max.X, BBox.Min.Y, BBox.Max.Z},
					{BBox.Min.X, BBox.Max.Y, BBox.Max.Z}, {BBox.Max.X, BBox.Max.Y, BBox.Max.Z},
				};
				float BestScore = -FLT_MAX;
				for (const FVector& Corner : Corners)
				{
					FVector2D SP;
					if (ProjectWorldLocationToScreen(Corner, SP))
					{
						const float Score = SP.X - SP.Y;
						if (Score > BestScore) { BestScore = Score; GuideScreenPos = SP; bFound = true; }
					}
				}
			}
			InteRealHUD->UpdateUserGuide(bFound, InvalidReason, GuideScreenPos);
		}
		else
		{
			InteRealHUD->UpdateUserGuide(false, InvalidReason, FVector2D::ZeroVector);
		}
	}

	if (UInteRealGizmoComponent* Gizmo = GetSelectedGizmoComponent())
	{
		if (PlayerCameraManager)
		{
			const bool bFirstPersonGizmo =
				CachedViewModeManager &&
				CachedViewModeManager->GetCurrentViewMode() == EHarnessViewMode::FirstPerson;

			Gizmo->UpdateConstantScreenSize(
				this,
				bFirstPersonGizmo ? FirstPersonGizmoScaleMultiplier : 1.0f);
		}

		if (!Gizmo->IsDragging())
		{
			Gizmo->UpdateHover(bIsHitting, LastCursorHit);
		}
	}

	if (UInteRealGizmoComponent* Gizmo = GetSelectedGizmoComponent(); Gizmo && Gizmo->IsDragging() && SelectedFurniture)
	{
		FVector WorldOrigin, WorldDir;
		DeprojectMousePositionToWorld(WorldOrigin, WorldDir);
		FVector2D MousePos;
		GetMousePosition(MousePos.X, MousePos.Y);
		const bool bChanged = Gizmo->UpdateDrag(WorldOrigin, WorldDir, GetPlacementSubsystem(), IsInputKeyDown(EKeys::LeftControl), MousePos);
		if (bChanged && FloorPlanPlacementSyncComponent)
		{
			FloorPlanPlacementSyncComponent->SyncFloorPlan2DFromFurniture(SelectedFurniture);
		}
		
		if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
		{
			if (bIsGizmoRotationWidgetActive)
			{
				InteRealHUD->UpdateRotationGuideForInput(
					Gizmo->GetCurrentRotationDeltaDegrees());
			}
		}
		return;
	}

	if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
	{
		InteRealHUD->UpdateRotationGuide(false, 0.0f, FVector2D::ZeroVector);
	}

	UInteriorPlacementSubsystem* PS2 = GetPlacementSubsystem();
	if (bIsMovingFurniture && SelectedFurniture && PS2)
	{
		PS2->UpdateGizmoMoveFree(CurrentCursorWorldLoc + MoveDragOffset, SelectedFurniture);
		if (UInteRealGizmoComponent* Gizmo = GetSelectedGizmoComponent())
		{
			Gizmo->UpdateAnchorFromOwner();
		}
		if (FloorPlanPlacementSyncComponent)
		{
			FloorPlanPlacementSyncComponent->SyncFloorPlan2DFromFurniture(SelectedFurniture);
		}
		return;
	}

	if (!PS2 || !PS2->HasActivePreview()) return;
	if (IsMouseOverInteractiveUI()) return;
	if (bPreviewHiddenUntilViewport)
	{
		PS2->SetPreviewHidden(false);
		bPreviewHiddenUntilViewport = false;
	}
	if (!bIsHitting) return;

	PS2->UpdatePreviewLocation(LastCursorHit);

	if (FloorPlanPlacementSyncComponent)
	{
		FloorPlanPlacementSyncComponent->SyncPreview2DFromActivePreview();
	}
}

void AInteRealPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC) return;

	// ===== Common bindings =====
	if (IA_ToggleMode)
	{
		EIC->BindAction(IA_ToggleMode, ETriggerEvent::Started, this, &AInteRealPlayerController::OnToggleModeKey);
	}

	if (IA_FocusSelection)
	{
		EIC->BindAction(IA_FocusSelection, ETriggerEvent::Started, this, &AInteRealPlayerController::OnFocusSelectionKey);
	}

	if (IA_Move)
	{
		EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AInteRealPlayerController::OnMoveKey);
	}

	if (IA_MoveVertical)
	{
		EIC->BindAction(IA_MoveVertical, ETriggerEvent::Triggered, this, &AInteRealPlayerController::OnMoveVerticalKey);
	}

	if (IA_Look)
	{
		EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AInteRealPlayerController::OnLookKey);
	}

	if (IA_Zoom)
	{
		EIC->BindAction(IA_Zoom, ETriggerEvent::Triggered, this, &AInteRealPlayerController::OnZoomKey);
	}

	// ===== Edit bindings =====
	if (IA_Place)
	{
		EIC->BindAction(IA_Place, ETriggerEvent::Started, this, &AInteRealPlayerController::OnPlaceKey);
		EIC->BindAction(IA_Place, ETriggerEvent::Completed, this, &AInteRealPlayerController::OnPlaceReleasedKey);
	}

	if (IA_Remove)
	{
		EIC->BindAction(IA_Remove, ETriggerEvent::Started, this, &AInteRealPlayerController::OnRemoveKey);
	}

	if (IA_Rotate)
	{
		EIC->BindAction(IA_Rotate, ETriggerEvent::Started, this, &AInteRealPlayerController::OnRotateKey);
	}

	if (IA_Rotate15)
	{
		EIC->BindAction(IA_Rotate15, ETriggerEvent::Started, this, &AInteRealPlayerController::OnRotate15Key);
	}

	if (IA_Continuous)
	{
		EIC->BindAction(IA_Continuous, ETriggerEvent::Started, this, &AInteRealPlayerController::OnContinuousPressed);
		EIC->BindAction(IA_Continuous, ETriggerEvent::Completed, this, &AInteRealPlayerController::OnContinuousReleased);
		EIC->BindAction(IA_Continuous, ETriggerEvent::Canceled, this, &AInteRealPlayerController::OnContinuousReleased);
	}

	InputComponent->BindKey(EKeys::G, IE_Pressed, this, &AInteRealPlayerController::ToggleGrid);

	// ===== View bindings =====
	if (IA_SwitchToTopDown)
	{
		EIC->BindAction(IA_SwitchToTopDown, ETriggerEvent::Started, this, &AInteRealPlayerController::OnTopDownKey);
	}

	if (IA_SwitchToIsometric)
	{
		EIC->BindAction(IA_SwitchToIsometric, ETriggerEvent::Started, this, &AInteRealPlayerController::OnIsometricKey);
	}

	if (IA_SwitchToFirstPerson)
	{
		EIC->BindAction(IA_SwitchToFirstPerson, ETriggerEvent::Started, this, &AInteRealPlayerController::OnFirstPersonKey);
	}
	
	if (IA_Undo)
	{
		EIC->BindAction(IA_Undo, ETriggerEvent::Started, this, &AInteRealPlayerController::OnUndoKey);
	}

	if (IA_Redo)
	{
		EIC->BindAction(IA_Redo, ETriggerEvent::Started, this, &AInteRealPlayerController::OnRedoKey);
	}

	if (IA_Copy)
	{
		EIC->BindAction(IA_Copy, ETriggerEvent::Started, this, &AInteRealPlayerController::OnCopyKey);
	}

	if (IA_Paste)
	{
		EIC->BindAction(IA_Paste, ETriggerEvent::Started, this, &AInteRealPlayerController::OnPasteKey);
	}

	if (IA_Duplicate)
	{
		EIC->BindAction(IA_Duplicate, ETriggerEvent::Started, this, &AInteRealPlayerController::OnDuplicateKey);
	}

	if (IA_Save)
	{
		EIC->BindAction(IA_Save, ETriggerEvent::Started, this, &AInteRealPlayerController::OnSaveKey);
	}
}

void AInteRealPlayerController::SetControlMode(EInteRealControlMode NewMode)
{
	if (CurrentControlMode == NewMode)
	{
		ApplyCurrentControlMode();
		return;
	}

	const EInteRealControlMode PreviousControlMode = CurrentControlMode;

	CurrentControlMode = NewMode;
	ApplyCurrentControlMode();

	if (PreviousControlMode == EInteRealControlMode::Edit && NewMode == EInteRealControlMode::View)
	{
		SetViewMode(EHarnessViewMode::FirstPerson);
	}
}

void AInteRealPlayerController::HandleModeChanged(bool bIsEditMode)
{
	if (bIsEditMode)
	{
		SetControlMode(EInteRealControlMode::Edit);

		// ??LayoutWidget ?�리게이??바인??
		if (AInteRealHUD* HUD = GetInteRealHUD())
		{
			if (UEditModeLayoutWidget* LayoutWidget = HUD->EditModeLayoutWidgetInstance)
			{
				LayoutWidget->OnFloorPlanPanelOpenChanged.RemoveDynamic(
					this, &AInteRealPlayerController::HandleFloorPlanPanelOpenChanged);
				LayoutWidget->OnFloorPlanPanelOpenChanged.AddDynamic(
					this, &AInteRealPlayerController::HandleFloorPlanPanelOpenChanged);

				// ?��? ?�려?�으�?즉시 ?�용
				HandleFloorPlanPanelOpenChanged(LayoutWidget->IsFloorPlanPanelOpen());
			}
		}
	}
	else
	{
		SetControlMode(EInteRealControlMode::View);

		// ??View 모드�??�아�??�도 ?�널 ?�태 반영
		if (AInteRealHUD* HUD = GetInteRealHUD())
		{
			if (UEditModeLayoutWidget* LayoutWidget = HUD->EditModeLayoutWidgetInstance)
			{
				HandleFloorPlanPanelOpenChanged(LayoutWidget->IsFloorPlanPanelOpen());
			}
		}
	}
}

void AInteRealPlayerController::HandleIconClicked(FName command)
{
	if (command == "Undo")
	{
		OnUndoKey();
	}
	else if (command == "Redo")
	{
		OnRedoKey();
	}
}

void AInteRealPlayerController::HandleFurnitureSpawn(FFurnitureDataRow FurnitureData)
{
	StartFurniturePlacement(FurnitureData);
}

void AInteRealPlayerController::HandleWallMaterialDataChanged(FMaterialDataRow MaterialData)
{
	ApplyMaterialDataToSelectedSurface(MaterialData);
}

void AInteRealPlayerController::HandlePipelineLoadFinished()
{
	if (FloorPlanPlacementSyncComponent)
	{
		FloorPlanPlacementSyncComponent->RebuildFloorPlan2DFromPlacedFurniture();
	}
}

void AInteRealPlayerController::SelectSurface(UMeshComponent* SurfaceComponent)
{
	if (SelectedSurfaceComponent == SurfaceComponent)
	{
		return;
	}

	DeselectFurniture();
	DeselectSurface();

	SelectedSurfaceComponent = SurfaceComponent;

	if (SelectedSurfaceComponent)
	{
		SelectedSurfaceComponent->SetRenderCustomDepth(true);
		SelectedSurfaceComponent->SetCustomDepthStencilValue(1);
	}

	const bool bCanEditMaterial =
		SelectedSurfaceComponent &&
		(
			SelectedSurfaceComponent->ComponentHasTag(TEXT("EditableWall")) ||
			SelectedSurfaceComponent->ComponentHasTag(TEXT("EditableFloor")) ||
			SelectedSurfaceComponent->ComponentHasTag(TEXT("Floor"))
		);

	if (AInteRealHUD* HUD = GetInteRealHUD())
	{
		HUD->ShowFurnitureSizePanel(nullptr);
		HUD->ShowLightAttributesPanel(nullptr);
		HUD->ShowMaterialAttributesPanel(bCanEditMaterial);

		if (bCanEditMaterial)
		{
			FMaterialDataRow SurfaceMaterialData;
			if (TryGetSurfaceMaterialData(SelectedSurfaceComponent, SurfaceMaterialData))
			{
				HUD->RefreshMaterialAttributesPanel(SurfaceMaterialData);
			}
			else
			{
				HUD->ResetMaterialAttributesPanel();
			}
		}
	}
}

void AInteRealPlayerController::DeselectSurface()
{
	if (SelectedSurfaceComponent)
	{
		SelectedSurfaceComponent->SetRenderCustomDepth(false);
		SelectedSurfaceComponent = nullptr;
	}

	if (AInteRealHUD* HUD = GetInteRealHUD())
	{
		HUD->ShowMaterialAttributesPanel(false);
	}
}

void AInteRealPlayerController::ApplyMaterialDataToSelectedSurface(const FMaterialDataRow& MaterialData)
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	if (!SelectedSurfaceComponent) return;
	if (!SurfaceMasterMaterial) return;

	FMaterialDataRow AppliedMaterialData = MaterialData;

	FMaterialDataRow ExistingMaterialData;
	if (!AppliedMaterialData.DisplayImage && TryGetSurfaceMaterialData(SelectedSurfaceComponent, ExistingMaterialData))
	{
		AppliedMaterialData.DisplayImage = ExistingMaterialData.DisplayImage;
	}

	if (UInteriorPlacementSubsystem* PS = GetPlacementSubsystem()) PS->RecordUndoSnapshot();

	UMaterialInstanceDynamic* SurfaceMID = Cast<UMaterialInstanceDynamic>(SelectedSurfaceComponent->GetMaterial(0));
	if (!SurfaceMID)
	{
		SurfaceMID = UMaterialInstanceDynamic::Create(SurfaceMasterMaterial, this);
		if (!SurfaceMID) return;

		SelectedSurfaceComponent->SetMaterial(0, SurfaceMID);
	}

	if (AppliedMaterialData.DisplayImage)
	{
		SurfaceMID->SetTextureParameterValue(TEXT("BaseColorTexture"), AppliedMaterialData.DisplayImage);
	}

	SurfaceMID->SetScalarParameterValue(TEXT("Metallic"), AppliedMaterialData.Metallic);
	SurfaceMID->SetScalarParameterValue(TEXT("Metalic"), AppliedMaterialData.Metallic);
	SurfaceMID->SetScalarParameterValue(TEXT("Specular"), AppliedMaterialData.Specular);
	SurfaceMID->SetScalarParameterValue(TEXT("Roughness"), AppliedMaterialData.Roughness);
	SurfaceMID->SetScalarParameterValue(TEXT("Emissive"), AppliedMaterialData.Emissive);

	StoreSurfaceMaterialData(SelectedSurfaceComponent, AppliedMaterialData);

	if (AInteRealHUD* HUD = GetInteRealHUD())
	{
		HUD->RefreshMaterialAttributesPanel(AppliedMaterialData);
	}
}

void AInteRealPlayerController::ApplyCurrentControlMode()
{
	UpdateMappingContexts();
	UpdateInputModeForCurrentControlMode();

	// View 모드 �환 �택/�리뷁태 �리
	if (CurrentControlMode != EInteRealControlMode::Edit)
	{
		DeselectFurniture();
		DeselectSurface();
		if (UInteriorPlacementSubsystem* PS = GetPlacementSubsystem())
		{
			if (PS->HasActivePreview()) PS->CancelPreview();
			PS->SetGridVisible(false);
			PS->SetLightFixtureIconsEditModeActive(false);
		}

		if (FloorPlanPlacementSyncComponent)
		{
			FloorPlanPlacementSyncComponent->CancelFloorPlan2DPlacement();
		}
	}
	else if (UInteriorPlacementSubsystem* PS = GetPlacementSubsystem())
	{
		PS->SetGridVisible(bGridVisible);
		PS->SetLightFixtureIconsEditModeActive(true);
	}

	if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
	{
		InteRealHUD->UpdateModeUIVisibility(CurrentControlMode);
	}
}

void AInteRealPlayerController::UpdateMappingContexts()
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (IMC_Common)
		{
			Subsystem->RemoveMappingContext(IMC_Common);
		}
		
		if (IMC_EditMode)
		{
			Subsystem->RemoveMappingContext(IMC_EditMode);
		}

		if (IMC_ViewMode)
		{
			Subsystem->RemoveMappingContext(IMC_ViewMode);
		}

		// �� 공통 �력 먼� �록
		if (IMC_Common)
		{
			Subsystem->AddMappingContext(IMC_Common, 0);
		}

		// 모드볅력 추� �록
		if (CurrentControlMode == EInteRealControlMode::Edit)
		{
			if (IMC_EditMode)
			{
				Subsystem->AddMappingContext(IMC_EditMode, 1);
			}
		}
		else
		{
			if (IMC_ViewMode)
			{
				Subsystem->AddMappingContext(IMC_ViewMode, 1);
			}
		}
	}
}

void AInteRealPlayerController::UpdateInputModeForCurrentControlMode()
{
	if (CurrentControlMode == EInteRealControlMode::Edit)
	{
		FInputModeGameAndUI InputMode;
		// �� [�� �트리밍 �LockOnCapture��경하브라�� �릭 �커�기		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockOnCapture);
		InputMode.SetHideCursorDuringCapture(false);

		if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
		{
			if (UUserWidget* PlacementTab = InteRealHUD->GetPlacementTabInstance())
			{
				InputMode.SetWidgetToFocus(PlacementTab->TakeWidget());
			}
		}

		SetInputMode(InputMode);
		bShowMouseCursor = true;
	}
	else
	{
		FInputModeGameAndUI InputMode;
		// �� [�� �트리밍 �View 모드�서�릭 가�하�록 �일�게 �정
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockOnCapture);
		InputMode.SetHideCursorDuringCapture(false);

		SetInputMode(InputMode);
		bShowMouseCursor = true;
	}
}

AInteRealHUD* AInteRealPlayerController::GetInteRealHUD() const
{
	return Cast<AInteRealHUD>(GetHUD());
}

UInteriorPlacementSubsystem* AInteRealPlayerController::GetPlacementSubsystem() const
{
	return GetWorld() ? GetWorld()->GetSubsystem<UInteriorPlacementSubsystem>() : nullptr;
}

// ?�레?�스 결과??거리?�이므�??�면??가???�에 ?�는 기즈�??�소�??�택?�다.
static bool IsRotateGizmoTag(const FString& Tag)
{
	return Tag.StartsWith(TEXT("Rotate")) || Tag == TEXT("RotationRing");
}

static void AddSelectedFurnitureNonGizmoIgnoredComponents(FCollisionQueryParams& Params,
                                                          const AFurniture* Furniture,
                                                          const UInteRealGizmoComponent* Gizmo)
{
	if (!Furniture)
	{
		return;
	}

	TArray<UPrimitiveComponent*> Components;
	Furniture->GetComponents<UPrimitiveComponent>(Components);
	for (UPrimitiveComponent* Component : Components)
	{
		if (Component && (!Gizmo || !Gizmo->OwnsGizmoComponent(Component)))
		{
			Params.AddIgnoredComponent(Component);
		}
	}
}

static bool SelectGizmoHit(const TArray<FHitResult>& Hits, const UInteRealGizmoComponent* Gizmo, FHitResult& OutHit)
{
	if (!Gizmo)
	{
		return false;
	}

	for (const FHitResult& Hit : Hits)
	{
		if (!Gizmo->GetAxisTagFromHit(Hit).IsEmpty())
		{
			OutHit = Hit;
			return true;
		}
	}
	return false;
}

void AInteRealPlayerController::UpdateCursorHit()
{
	if (bIsMovingFurniture && SelectedFurniture)
	{
		FVector WorldOrigin, WorldDir;
		DeprojectMousePositionToWorld(WorldOrigin, WorldDir);

		FCollisionQueryParams Params(NAME_None, true);
		
		APawn* P = GetPawn();
		Params.AddIgnoredActor(P);
		Params.AddIgnoredActor(SelectedFurniture);
		
		if (UInteRealGizmoComponent* Gizmo = GetSelectedGizmoComponent())
		{
			DeprojectMousePositionToWorld(WorldOrigin, WorldDir);

			FCollisionQueryParams GizmoParams(NAME_None, true);
			if (P)
			{
				GizmoParams.AddIgnoredActor(P);
			}
			AddSelectedFurnitureNonGizmoIgnoredComponents(GizmoParams, SelectedFurniture, Gizmo);

			TArray<FHitResult> Hits;
			GetWorld()->LineTraceMultiByChannel(
				Hits,
				WorldOrigin,
				WorldOrigin + WorldDir * 100000.f,
				ECC_Visibility,
				GizmoParams
			);

			FHitResult GizmoHit;
			if (SelectGizmoHit(Hits, Gizmo, GizmoHit))
			{
				LastCursorHit = GizmoHit;
				bIsHitting = true;
				CurrentCursorWorldLoc = GizmoHit.Location;
				return;
			}

			if (GizmoTraceRadius > 0.0f)
			{
				Hits.Reset();
				GetWorld()->SweepMultiByChannel(
					Hits,
					WorldOrigin,
					WorldOrigin + WorldDir * 100000.f,
					FQuat::Identity,
					ECC_Visibility,
					FCollisionShape::MakeSphere(GizmoTraceRadius),
					GizmoParams
				);

				if (SelectGizmoHit(Hits, Gizmo, GizmoHit))
				{
					LastCursorHit = GizmoHit;
					bIsHitting = true;
					CurrentCursorWorldLoc = GizmoHit.Location;
					return;
				}
			}
		}

		bIsHitting = GetWorld()->LineTraceSingleByChannel(
			LastCursorHit,
			WorldOrigin,
			WorldOrigin + WorldDir * 100000.f,
			ECC_Visibility,
			Params
		);
		
		if (bIsHitting)
		{
			CurrentCursorWorldLoc = LastCursorHit.Location;
		}
		return;
	}

	// 기즈모�? ?�택??가�?메시??가?�져???�선?�으�??�식?�도�?별도 검??
	if (UInteRealGizmoComponent* Gizmo = GetSelectedGizmoComponent())
	{
		FVector WorldOrigin, WorldDir;
		if (!DeprojectMousePositionToWorld(WorldOrigin, WorldDir) || WorldDir.SizeSquared() < KINDA_SMALL_NUMBER)
		{
			return;
		}

		FCollisionQueryParams GizmoParams(NAME_None, true);
		if (APawn* P = GetPawn())
		{
			GizmoParams.AddIgnoredActor(P);
		}
		if (SelectedFurniture)
		{
			AddSelectedFurnitureNonGizmoIgnoredComponents(GizmoParams, SelectedFurniture, Gizmo);
		}

		TArray<FHitResult> Hits;
		GetWorld()->LineTraceMultiByChannel(
			Hits,
			WorldOrigin,
			WorldOrigin + WorldDir * 100000.f,
			ECC_Visibility,
			GizmoParams
		);

		FHitResult GizmoHit;
		if (SelectGizmoHit(Hits, Gizmo, GizmoHit))
		{
			LastCursorHit = GizmoHit;
			bIsHitting = true;
			CurrentCursorWorldLoc = GizmoHit.Location;
			return;
		}

		if (GizmoTraceRadius > 0.0f)
		{
			Hits.Reset();
			GetWorld()->SweepMultiByChannel(
				Hits,
				WorldOrigin,
				WorldOrigin + WorldDir * 100000.f,
				FQuat::Identity,
				ECC_Visibility,
				FCollisionShape::MakeSphere(GizmoTraceRadius),
				GizmoParams
			);

			if (SelectGizmoHit(Hits, Gizmo, GizmoHit))
			{
				LastCursorHit = GizmoHit;
				bIsHitting = true;
				CurrentCursorWorldLoc = GizmoHit.Location;
				return;
			}
		}
	}

	// 바닥/가�???기본 ?�레?�스
	FHitResult FloorHit;
	FHitResult WallHit;
	bool bHitFloor = false;
	bool bHitWall = false;

	{
		FVector WorldOrigin, WorldDir;
		if (!DeprojectMousePositionToWorld(WorldOrigin, WorldDir) || WorldDir.SizeSquared() < KINDA_SMALL_NUMBER)
		{
			return;
		}
		const FVector TraceEnd = WorldOrigin + WorldDir * 100000.f;
		
		FCollisionQueryParams Params(NAME_None, true);
		if (APawn* P = GetPawn())
		{
			Params.AddIgnoredActor(P);
		}

		// 배치 미리보기 중에???��? 배치??가구들??커서 ?�레?�스�?가로막지 ?�도�?무시 처리
		if (UInteriorPlacementSubsystem* PS = GetPlacementSubsystem())
		{
			if (PS->HasActivePreview())
			{
				const AFurniture* Preview = PS->GetPreviewFurniture();
				const bool bNeedsFurnitureSurfaceHit = Preview && Preview->SupportsPlacementType(EPlacementSurfaceType::Surface);
				if (!bNeedsFurnitureSurfaceHit)
				{
					for (AFurniture* Placed : PS->GetPlacedFurnitures())
					{
						if (IsValid(Placed))
						{
							Params.AddIgnoredActor(Placed);
						}
					}
				}
			}
		}

		bHitFloor = GetWorld()->LineTraceSingleByChannel(FloorHit, WorldOrigin, TraceEnd, ECC_Visibility, Params);

		// �??�용 ?�레?�스 ??바닥??가?��?지 ?�아 �??�체 ?�이?�서 ?�힘
		bHitWall = GetWorld()->LineTraceSingleByChannel(WallHit, WorldOrigin, TraceEnd, WallTraceChannel, Params);
	}

	// 배치 중인 가구�? �?배치�?지?�하지 ?�으�? ?�면????가까운 벽이 ?�히?�라??무시?�고 바닥 ?�트�??�다.
	// (ISO ?�점?�서???�른 방의 벽이 ?�면??커서 ?�래 바닥보다 카메?�에 ??가깝게 ?��? Wall�??�판?�는 경우가 ?�음)
	UInteriorPlacementSubsystem* PS3 = GetPlacementSubsystem();
	const AFurniture* PreviewFurniture = (PS3 && PS3->HasActivePreview()) ? PS3->GetPreviewFurniture() : nullptr;
	const bool bPreviewSupportsWall = !PreviewFurniture || PreviewFurniture->SupportsPlacementType(EPlacementSurfaceType::Wall);
	const bool bPreviewSupportsFloor = PreviewFurniture && PreviewFurniture->SupportsPlacementType(EPlacementSurfaceType::Floor);
	const bool bPreviewNeedsFloorFallback =
		PreviewFurniture &&
		bPreviewSupportsFloor &&
		!PreviewFurniture->SupportsPlacementType(EPlacementSurfaceType::Wall) &&
		!PreviewFurniture->SupportsPlacementType(EPlacementSurfaceType::Ceiling);

	// ?�면??같�? ?��??�서 ?????�히�?카메?�에 ??가까운(거리가 짧�?) 쪽을 채택
	if (bHitWall && bPreviewSupportsWall && (!bHitFloor || WallHit.Distance < FloorHit.Distance))
	{
		LastCursorHit = WallHit;
		bIsHitting = true;
	}
	else
	{
		LastCursorHit = FloorHit;
		bIsHitting = bHitFloor;
	}

	if (bIsHitting && bPreviewNeedsFloorFallback && PS3)
	{
		const UPrimitiveComponent* HitComp = LastCursorHit.GetComponent();
		const AFurniture* HitFurniture = Cast<AFurniture>(LastCursorHit.GetActor());
		const bool bIsSupportedFurnitureSurface =
			HitFurniture &&
			HitFurniture->GetPlacementState() == EPlacementState::Placed &&
			PreviewFurniture->SupportsPlacementType(EPlacementSurfaceType::Surface);
		const bool bHitLooksLikeWall =
			(HitComp && HitComp->ComponentHasTag(TEXT("EditableWall"))) ||
			LastCursorHit.Location.Z > PS3->GetFloorZ() + 5.0f;

		if (bHitLooksLikeWall && !bIsSupportedFurnitureSurface)
		{
			FVector WorldOrigin;
			FVector WorldDir;
			DeprojectMousePositionToWorld(WorldOrigin, WorldDir);

			const FVector FloorPoint = FMath::LinePlaneIntersection(
				WorldOrigin,
				WorldOrigin + WorldDir * 100000.0f,
				FPlane(FVector(0.0f, 0.0f, PS3->GetFloorZ()), FVector::UpVector));

			FHitResult ProjectedFloorHit;
			ProjectedFloorHit.bBlockingHit = true;
			ProjectedFloorHit.Location = FloorPoint;
			ProjectedFloorHit.ImpactPoint = FloorPoint;
			ProjectedFloorHit.ImpactNormal = FVector::UpVector;
			ProjectedFloorHit.Normal = FVector::UpVector;
			LastCursorHit = ProjectedFloorHit;
		}
	}

	if (bIsHitting)
	{
		CurrentCursorWorldLoc = LastCursorHit.Location;
	}
}

void AInteRealPlayerController::ToggleGrid()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS) return;

	bGridVisible = !bGridVisible;
	PS->SetGridVisible(bGridVisible);
}

void AInteRealPlayerController::SetGridVisible(bool bShow)
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS) return;
	
	bGridVisible = bShow;
	PS->SetGridVisible(bGridVisible);
}

void AInteRealPlayerController::ToggleFreePlacementMode()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	if (UInteriorPlacementSubsystem* PS = GetPlacementSubsystem())
	{
		PS->ToggleFreePlacementMode();
	}
}

void AInteRealPlayerController::ToggleGizmoDisplayMode()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;

	const EInteRealGizmoDisplayMode CurrentMode = bGizmoShowMove && bGizmoShowRotate
		? EInteRealGizmoDisplayMode::All
		: bGizmoShowMove
			? EInteRealGizmoDisplayMode::Move
			: bGizmoShowRotate
				? EInteRealGizmoDisplayMode::Rotation
				: EInteRealGizmoDisplayMode::None;

	switch (CurrentMode)
	{
	case EInteRealGizmoDisplayMode::All:
		ApplyGizmoDisplayFlags(true, false);
		break;
	case EInteRealGizmoDisplayMode::Move:
		ApplyGizmoDisplayFlags(false, true);
		break;
	case EInteRealGizmoDisplayMode::Rotation:
	default:
		ApplyGizmoDisplayFlags(true, true);
		break;
	}
}

void AInteRealPlayerController::SetGizmoShowMove(bool bShow)
{
	ApplyGizmoDisplayFlags(bShow, IsGizmoShowingRotate());
}

void AInteRealPlayerController::SetGizmoShowRotate(bool bShow)
{
	ApplyGizmoDisplayFlags(IsGizmoShowingMove(), bShow);
}

bool AInteRealPlayerController::IsGizmoShowingMove() const
{
	return bGizmoShowMove;
}

bool AInteRealPlayerController::IsGizmoShowingRotate() const
{
	return bGizmoShowRotate;
}

void AInteRealPlayerController::ApplyGizmoDisplayFlags(bool bShowMove, bool bShowRotate)
{
	bGizmoShowMove = bShowMove;
	bGizmoShowRotate = bShowRotate;

	if (UInteRealGizmoComponent* Gizmo = GetSelectedGizmoComponent())
	{
		if (bShowMove && bShowRotate)
		{
			Gizmo->SetDisplayMode(EInteRealGizmoDisplayMode::All);
		}
		else if (bShowMove)
		{
			Gizmo->SetDisplayMode(EInteRealGizmoDisplayMode::Move);
		}
		else if (bShowRotate)
		{
			Gizmo->SetDisplayMode(EInteRealGizmoDisplayMode::Rotation);
		}
		else
		{
			Gizmo->SetDisplayMode(EInteRealGizmoDisplayMode::None);
		}
	}
}


void AInteRealPlayerController::OnPlaceKey()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS) return;

	if (PS->HasActivePreview())
	{
		if (bIsHitting)
		{
			AFurniture* PreviewFurniture = PS->GetPreviewFurniture();
			const int32 FurnitureID = PreviewFurniture ? PreviewFurniture->FurnitureID : 0;
			const FFurnitureDataRow* FurnitureRow = PS->FindFurnitureRowByID(FurnitureID);
			const bool bContinuePlacement = bContinuousModifierHeld;

			AFurniture* ConfirmedFurniture = ConfirmActivePreviewFurnitureForFloorPlanSync(bContinuePlacement);

			// ?�속 배치 중이 ?�니?�면, �?배치??조명??바로 ?�택 ?�태�?만들??
			// 기즈모�? ?�께 ?�성 ?�널??즉시 ?�도�??�다 (?�반 가구는 기존 ?�작 ?��?).
			if (!bContinuePlacement && Cast<ALightFixture>(ConfirmedFurniture))
			{
				SelectFurniture(ConfirmedFurniture);
			}

			if (FloorPlanPlacementSyncComponent)
			{
				if (bContinuePlacement && FurnitureRow)
				{
					FloorPlanPlacementSyncComponent->StartFloorPlan2DPlacement(*FurnitureRow);
				}
				else
				{
					FloorPlanPlacementSyncComponent->CancelFloorPlan2DPlacement();
					FloorPlanPlacementSyncComponent->ClearFloorPlan2DSelection();
				}
			}
		}

		return;
	}

	if (!bIsHitting)
	{
		DeselectFurniture();
		DeselectSurface();
		return;
	}

	UPrimitiveComponent* HitComp = LastCursorHit.GetComponent();

	if (HitComp && SelectedFurniture)
	{
		UInteRealGizmoComponent* Gizmo = GetSelectedGizmoComponent();
		const FString AxisTag = Gizmo ? Gizmo->GetAxisTagFromHit(LastCursorHit) : FString();
		if (!AxisTag.IsEmpty())
		{
			if (AxisTag.StartsWith(TEXT("Move")))
			{
				FVector WorldOrigin, WorldDir;
				DeprojectMousePositionToWorld(WorldOrigin, WorldDir);
				FVector2D MousePos;
				GetMousePosition(MousePos.X, MousePos.Y);
				Gizmo->BeginDrag(AxisTag, WorldOrigin, WorldDir, MousePos);

				if (UInteriorPlacementSubsystem* PSGizmo = GetPlacementSubsystem())
					PSGizmo->BeginGizmoMove(SelectedFurniture);
				return;
			}

			if (IsRotateGizmoTag(AxisTag))
			{
				if (UInteriorPlacementSubsystem* PSSnap = GetPlacementSubsystem())
					PSSnap->BeginGizmoMove(SelectedFurniture);

				FVector WorldOrigin, WorldDir;
				DeprojectMousePositionToWorld(WorldOrigin, WorldDir);
				FVector2D MousePos;
				GetMousePosition(MousePos.X, MousePos.Y);
				Gizmo->BeginDrag(AxisTag, WorldOrigin, WorldDir, MousePos);

				const EGizmoTransformAxis RotationAxis = Gizmo->GetCurrentAxis();
				const FRotator InitialRotation = SelectedFurniture->GetActorRotation();
				const float InitialAngle = RotationAxis == EGizmoTransformAxis::RotatePitch
					? InitialRotation.Pitch
					: RotationAxis == EGizmoTransformAxis::RotateRoll
						? InitialRotation.Roll
						: InitialRotation.Yaw;

				FVector2D GizmoScreenPosition = MousePos;
				ProjectWorldLocationToScreen(Gizmo->GetAnchorLocation(), GizmoScreenPosition);
				if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
				{
					InteRealHUD->ShowRotationGuideForInput(InitialAngle, GizmoScreenPosition);
				}

				bIsGizmoRotationWidgetActive = true;
				Gizmo->SetGizmoHidden(true);
				return;
			}
		}
	}

	if (AFurniture* HitFurniture = Cast<AFurniture>(LastCursorHit.GetActor()))
	{
		if (HitFurniture->GetPlacementState() == EPlacementState::Placed)
		{
			if (SelectedFurniture == HitFurniture)
			{
				if (FloorPlanPlacementSyncComponent && !FloorPlanPlacementSyncComponent->IsSyncingFurniture3DFrom2D())
				{
					FloorPlanPlacementSyncComponent->SelectFloorPlan2DForFurniture(SelectedFurniture);
				}

				bIsMovingFurniture = true;
				DragStartFurnitureLocation = SelectedFurniture->GetActorLocation();
				MoveDragOffset = DragStartFurnitureLocation - CurrentCursorWorldLoc;
				MoveDragOffset.Z = 0.0f;

				if (UInteriorPlacementSubsystem* PSMove = GetPlacementSubsystem())
				{
					PSMove->BeginGizmoMove(SelectedFurniture);
				}
				return;
			}

			SelectFurniture(HitFurniture);
			return;
		}
	}
	
	if (UMeshComponent* HitMeshComp = Cast<UMeshComponent>(HitComp))
	{
		const bool bEditableSurface =
			HitMeshComp->ComponentHasTag(TEXT("EditableWall")) ||
			HitMeshComp->ComponentHasTag(TEXT("EditableOpening")) ||
			HitMeshComp->ComponentHasTag(TEXT("EditableFloor")) ||
			HitMeshComp->ComponentHasTag(TEXT("Floor"));

		if (bEditableSurface)
		{
			SelectSurface(HitMeshComp);
			return;
		}
	}

	DeselectFurniture();
	DeselectSurface();
}

void AInteRealPlayerController::OnPlaceReleasedKey()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;

	if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
	{
		InteRealHUD->UpdateRotationGuide(false, 0.0f, FVector2D::ZeroVector);
	}

	if (UInteRealGizmoComponent* Gizmo = GetSelectedGizmoComponent())
	{
		Gizmo->SetGizmoHidden(false);
		const EGizmoTransformAxis DraggedAxis = Gizmo->GetCurrentAxis();

		const bool bTransforming =
			DraggedAxis == EGizmoTransformAxis::MoveX ||
			DraggedAxis == EGizmoTransformAxis::MoveY ||
			DraggedAxis == EGizmoTransformAxis::MoveZ ||
			DraggedAxis == EGizmoTransformAxis::RotateYaw ||
			DraggedAxis == EGizmoTransformAxis::RotatePitch ||
			DraggedAxis == EGizmoTransformAxis::RotateRoll;
		const bool bRotating =
			DraggedAxis == EGizmoTransformAxis::RotateYaw ||
			DraggedAxis == EGizmoTransformAxis::RotatePitch ||
			DraggedAxis == EGizmoTransformAxis::RotateRoll;
		if (bTransforming && SelectedFurniture)
		{
			if (UInteriorPlacementSubsystem* PSFin = GetPlacementSubsystem())
			{
				if (bRotating)
				{
					PSFin->UpdateGizmoRotation(SelectedFurniture);
				}
				PSFin->FinalizeGizmoMove(SelectedFurniture);
			}

			SelectedFurniture->SetSelected(true);
		}

		Gizmo->EndDrag();
	}
	bIsGizmoRotationWidgetActive = false;

	if (bIsMovingFurniture && SelectedFurniture)
	{
		bIsMovingFurniture = false;
		const float MoveDist = FVector::Dist2D(SelectedFurniture->GetActorLocation(), DragStartFurnitureLocation);
		if (UInteriorPlacementSubsystem* PSEnd = GetPlacementSubsystem())
		{
			if (MoveDist > 2.0f)
				PSEnd->FinalizeGizmoMove(SelectedFurniture);
			else
				PSEnd->AbortGizmoMove(SelectedFurniture);
		}
		if (FloorPlanPlacementSyncComponent)
		{
			FloorPlanPlacementSyncComponent->SyncFloorPlan2DFromFurniture(SelectedFurniture);
		}
		SelectedFurniture->SetSelected(true);
	}
}

void AInteRealPlayerController::OnRemoveKey()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS) return;

	if (PS->HasActivePreview())
	{
		PS->CancelPreview();

		if (FloorPlanPlacementSyncComponent)
		{
			FloorPlanPlacementSyncComponent->CancelFloorPlan2DPlacement();
		}
		return;
	}

	if (SelectedFurniture)
	{
		AFurniture* ToRemove = SelectedFurniture;

		if (FloorPlanPlacementSyncComponent)
		{
			FloorPlanPlacementSyncComponent->SetDeletingFrom3D(true);
			FloorPlanPlacementSyncComponent->RemoveFloorPlan2DForFurniture(ToRemove);
			FloorPlanPlacementSyncComponent->SetDeletingFrom3D(false);
		}

		DeleteFurnitureActor(ToRemove);
	}
}

void AInteRealPlayerController::RotateEditFurniture(float AngleDeg)
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();

	if (PS && PS->HasActivePreview())
	{
		PS->RotatePreview(AngleDeg);

		if (FloorPlanPlacementSyncComponent)
		{
			FloorPlanPlacementSyncComponent->SyncPreview2DFromActivePreview();
		}

		return;
	}

	if (SelectedFurniture)
	{
		FRotator Rot = SelectedFurniture->GetActorRotation();
		Rot.Yaw = FRotator::NormalizeAxis(Rot.Yaw + AngleDeg);

		if (PS && SelectedFurniture->GetPlacedSurfaceType() == EPlacementSurfaceType::Floor)
		{
			const FFurnitureDataRow* Row = PS->FindFurnitureRowByID(SelectedFurniture->FurnitureID);
			if (!Row)
			{
				return;
			}

			PS->BeginGizmoMove(SelectedFurniture);
			const float Radians = FMath::DegreesToRadians(Rot.Yaw);
			const float AbsCos = FMath::Abs(FMath::Cos(Radians));
			const float AbsSin = FMath::Abs(FMath::Sin(Radians));
			SelectedFurniture->PlacedDimensions = FVector2D(
				FMath::Max(1, FMath::CeilToInt(Row->Dimensions.X * AbsCos + Row->Dimensions.Y * AbsSin)),
				FMath::Max(1, FMath::CeilToInt(Row->Dimensions.X * AbsSin + Row->Dimensions.Y * AbsCos)));
			SelectedFurniture->SetRotationPreservingPlacement(Rot);
			PS->UpdateGizmoMoveLocation(
				SelectedFurniture->GetMeshBounds().GetCenter(), SelectedFurniture, EGizmoTransformAxis::None);

			if (PS->InvalidReason == EPlacementInvalidReason::None)
			{
				PS->FinalizeGizmoMove(SelectedFurniture);
			}
			else
			{
				PS->AbortGizmoMove(SelectedFurniture);
			}
		}
		else
		{
			if (PS) PS->RecordUndoSnapshot();
			SelectedFurniture->SetRotationPreservingPlacement(Rot);
		}
		if (UInteRealGizmoComponent* Gizmo = GetSelectedGizmoComponent())
		{
			Gizmo->UpdateAnchorFromOwner();
		}
		if (FloorPlanPlacementSyncComponent)
		{
			FloorPlanPlacementSyncComponent->SyncFloorPlan2DFromFurniture(SelectedFurniture);
		}
	}
}

void AInteRealPlayerController::SetSelectedFurnitureLocationCm(FVector NewLocationCm)
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	if (!SelectedFurniture) return;
	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS) return;

	if (SelectedFurniture->GetPlacedSurfaceType() == EPlacementSurfaceType::Floor)
	{
		PS->BeginGizmoMove(SelectedFurniture);
		PS->UpdateGizmoMoveFree(NewLocationCm, SelectedFurniture);

		if (PS->InvalidReason == EPlacementInvalidReason::None)
		{
			PS->FinalizeGizmoMove(SelectedFurniture);
		}
		else
		{
			PS->AbortGizmoMove(SelectedFurniture);
		}
	}
	else
	{
		PS->RecordUndoSnapshot();
		SelectedFurniture->SetActorLocation(NewLocationCm);
	}

	if (UInteRealGizmoComponent* Gizmo = GetSelectedGizmoComponent())
	{
		Gizmo->UpdateAnchorFromOwner();
	}
	if (FloorPlanPlacementSyncComponent)
	{
		FloorPlanPlacementSyncComponent->SyncFloorPlan2DFromFurniture(SelectedFurniture);
	}
}

void AInteRealPlayerController::SetSelectedFurnitureRotationYaw(float AbsoluteYawDeg)
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	if (!SelectedFurniture) return;
	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();

	FRotator Rot = SelectedFurniture->GetActorRotation();
	Rot.Yaw = FRotator::NormalizeAxis(AbsoluteYawDeg);

	if (PS && SelectedFurniture->GetPlacedSurfaceType() == EPlacementSurfaceType::Floor)
	{
		const FFurnitureDataRow* Row = PS->FindFurnitureRowByID(SelectedFurniture->FurnitureID);
		if (!Row)
		{
			return;
		}

		PS->BeginGizmoMove(SelectedFurniture);
		const float Radians = FMath::DegreesToRadians(Rot.Yaw);
		const float AbsCos = FMath::Abs(FMath::Cos(Radians));
		const float AbsSin = FMath::Abs(FMath::Sin(Radians));
		SelectedFurniture->PlacedDimensions = FVector2D(
			FMath::Max(1, FMath::CeilToInt(Row->Dimensions.X * AbsCos + Row->Dimensions.Y * AbsSin)),
			FMath::Max(1, FMath::CeilToInt(Row->Dimensions.X * AbsSin + Row->Dimensions.Y * AbsCos)));
		SelectedFurniture->SetRotationPreservingPlacement(Rot);
		PS->UpdateGizmoMoveLocation(
			SelectedFurniture->GetMeshBounds().GetCenter(), SelectedFurniture, EGizmoTransformAxis::None);

		if (PS->InvalidReason == EPlacementInvalidReason::None)
		{
			PS->FinalizeGizmoMove(SelectedFurniture);
		}
		else
		{
			PS->AbortGizmoMove(SelectedFurniture);
		}
	}
	else
	{
		if (PS) PS->RecordUndoSnapshot();
		SelectedFurniture->SetRotationPreservingPlacement(Rot);
	}

	if (UInteRealGizmoComponent* Gizmo = GetSelectedGizmoComponent())
	{
		Gizmo->UpdateAnchorFromOwner();
	}
	if (FloorPlanPlacementSyncComponent)
	{
		FloorPlanPlacementSyncComponent->SyncFloorPlan2DFromFurniture(SelectedFurniture);
	}
}

void AInteRealPlayerController::DeleteSelectedFurniture()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	if (!SelectedFurniture) return;

	AFurniture* ToRemove = SelectedFurniture;

	if (FloorPlanPlacementSyncComponent)
	{
		FloorPlanPlacementSyncComponent->SetDeletingFrom3D(true);
		FloorPlanPlacementSyncComponent->RemoveFloorPlan2DForFurniture(ToRemove);
		FloorPlanPlacementSyncComponent->SetDeletingFrom3D(false);
	}

	DeleteFurnitureActor(ToRemove);
}

void AInteRealPlayerController::DuplicateSelectedFurniture()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	if (!SelectedFurniture) return;
	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS) return;

	const FFurnitureDataRow* Row = PS->FindFurnitureRowByID(SelectedFurniture->FurnitureID);
	if (!Row) return;

	const FRotator Rotation = SelectedFurniture->GetActorRotation();
	const FVector SpawnLoc = SelectedFurniture->GetActorLocation() + FVector(50.0f, 50.0f, 0.0f);

	if (PS->HasActivePreview())
	{
		PS->CancelPreview();
	}
	DeselectFurniture();
	DeselectSurface();

	PS->CreatePreviewFurnitureFromRow(SpawnLoc, Rotation, *Row);

	FHitResult Hit;
	Hit.Location = SpawnLoc;
	Hit.ImpactPoint = SpawnLoc;
	Hit.ImpactNormal = FVector::UpVector;
	PS->UpdatePreviewLocation(Hit);

	if (AFurniture* NewFurniture = ConfirmActivePreviewFurnitureForFloorPlanSync(false))
	{
		SelectFurniture(NewFurniture);
	}
}

void AInteRealPlayerController::OnRotateKey()
{
	const bool bReverse = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
	RotateEditFurniture(bReverse ? -90.0f : 90.0f);
}

void AInteRealPlayerController::OnRotate15Key()
{
	const bool bReverse = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
	RotateEditFurniture(bReverse ? -15.0f : 15.0f);
}

void AInteRealPlayerController::OnContinuousPressed()
{
	bContinuousModifierHeld = true;

	// ?��? 배치??가구�? ?�택???�태?�서 Shift�??�르�? �?가구�? 같�? 종류?????�리뷰로
	// ?�시 꺼내???�속배치�??�작?????�게 ?�다.
	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (PS && !PS->HasActivePreview() && SelectedFurniture)
	{
		const FFurnitureDataRow* Row = PS->FindFurnitureRowByID(SelectedFurniture->FurnitureID);
		if (Row)
		{
			const FRotator Rotation = SelectedFurniture->GetActorRotation();
			const FVector SpawnLoc = bIsHitting ? CurrentCursorWorldLoc : SelectedFurniture->GetActorLocation();

			DeselectFurniture();
			PS->CreatePreviewFurnitureFromRow(SpawnLoc, Rotation, *Row);
		}
	}
}

void AInteRealPlayerController::OnContinuousReleased()
{
	bContinuousModifierHeld = false;
}

void AInteRealPlayerController::SelectFurnitureForFloorPlanSync(AFurniture* Furniture)
{
	SelectFurniture(Furniture);
}

void AInteRealPlayerController::SelectFurniture(AFurniture* Furniture)
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;

	if (!IsValid(Furniture))
	{
		DeselectFurniture();
		return;
	}

	if (SelectedFurniture == Furniture)
	{
		if (FloorPlanPlacementSyncComponent && !FloorPlanPlacementSyncComponent->IsSyncingFurniture3DFrom2D())
		{
			FloorPlanPlacementSyncComponent->SelectFloorPlan2DForFurniture(SelectedFurniture);
		}
		return;
	}

	DeselectSurface();
	DeselectFurniture();
	SelectedFurniture = Furniture;

	if (SelectedFurniture)
	{
		SelectedFurniture->SetSelected(true);
		if (UInteRealGizmoComponent* Gizmo = SelectedFurniture->GetGizmoComponent())
		{
			Gizmo->SetSelectedActive(true);
			ApplyGizmoDisplayFlags(bGizmoShowMove, bGizmoShowRotate);
			Gizmo->UpdateAnchorFromOwner();
		}
		
		if (FloorPlanPlacementSyncComponent && !FloorPlanPlacementSyncComponent->IsSyncingFurniture3DFrom2D())
		{
			FloorPlanPlacementSyncComponent->SelectFloorPlan2DForFurniture(SelectedFurniture);
		}

		if (AInteRealHUD* HUD = GetInteRealHUD())
		{
			if (ALightFixture* LightFixture = Cast<ALightFixture>(SelectedFurniture))
			{
				HUD->ShowLightAttributesPanel(LightFixture);
			}
			else
			{
				HUD->ShowFurnitureSizePanel(SelectedFurniture);
			}
		}
	}
}

void AInteRealPlayerController::DeselectFurniture()
{
	ClearFurnitureSelectionInternal(true);
}

void AInteRealPlayerController::ReceiveWebCommand(const FString& JsonString)
{
	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS) return;

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[EditMode] Invalid web command JSON: %s"), *JsonString);
		return;
	}

	FString Action;
	if (!Root->TryGetStringField(TEXT("action"), Action)) return;

	if (Action == TEXT("SELECT_KIND"))
	{
		int32 ID = 0;
		if (Root->TryGetNumberField(TEXT("furnitureId"), ID))
		{
			const FFurnitureDataRow* Row = PS->FindFurnitureRowByID(ID);
			if (Row)
			{
				SetControlMode(EInteRealControlMode::Edit);
				StartFurniturePlacement(*Row);
			}
		}
	}
	else if (Action == TEXT("ROTATE"))
	{
		if (PS->HasActivePreview())
		{
			SetControlMode(EInteRealControlMode::Edit);
			PS->RotatePreview(90.0f);
		}
	}
	else if (Action == TEXT("CONFIRM"))
	{
		if (PS->HasActivePreview())
		{
			SetControlMode(EInteRealControlMode::Edit);

			AFurniture* PreviewFurniture = PS->GetPreviewFurniture();
			const int32 FurnitureID = PreviewFurniture ? PreviewFurniture->FurnitureID : 0;
			const FFurnitureDataRow* FurnitureRow = PS->FindFurnitureRowByID(FurnitureID);

			FVector ConfirmedWorldLocation = FVector::ZeroVector;
			float ConfirmedYaw = 0.0f;

			if (PreviewFurniture)
			{
				ConfirmedWorldLocation = PreviewFurniture->GetActorLocation();
				ConfirmedYaw = PreviewFurniture->GetActorRotation().Yaw;
			}

			TSet<TObjectKey<AFurniture>> PreviouslyPlacedFurnitureKeys;
			SnapshotPlacedFurnitureActors(PreviouslyPlacedFurnitureKeys);

			AFurniture* PreviewFurnitureBeforeConfirm = PreviewFurniture;

			PS->ConfirmFurniture();

			AFurniture* ConfirmedFurniture = ResolveConfirmedFurnitureActor(
				PreviewFurnitureBeforeConfirm,
				PreviouslyPlacedFurnitureKeys
			);

			if (FurnitureRow)
			{
				if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
				{
					if (UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = InteRealHUD->GetFloorPlan2DWidget())
					{
						if (FloorPlanPlacementSyncComponent)
						{
							FloorPlanPlacementSyncComponent->RegisterConfirmedFurnitureToFloorPlan(*FurnitureRow, ConfirmedWorldLocation, ConfirmedYaw, ConfirmedFurniture);
						}

						if (FloorPlanPlacementSyncComponent)
						{
							FloorPlanPlacementSyncComponent->CancelFloorPlan2DPlacement();
							FloorPlanPlacementSyncComponent->ClearFloorPlan2DSelection();
						}
					}
				}
			}
		}
	}
	else if (Action == TEXT("CANCEL"))
	{
		if (PS->HasActivePreview())
		{
			PS->CancelPreview();

			if (FloorPlanPlacementSyncComponent)
			{
				FloorPlanPlacementSyncComponent->CancelFloorPlan2DPlacement();
			}
		}
	}
	else if (Action == TEXT("LOAD"))
	{
		FString Payload;
		if (Root->TryGetStringField(TEXT("data"), Payload))
		{
			PS->ImportPlacedFurnituresJson(Payload);

			if (FloorPlanPlacementSyncComponent)
			{
				FloorPlanPlacementSyncComponent->RequestRebuildFloorPlan2DFromPlacedFurniture();
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[EditMode] Unknown web action: %s"), *Action);
	}
}

void AInteRealPlayerController::StartFurniturePlacement(const FFurnitureDataRow& FurnitureRow)
{
	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS) return;

	SetControlMode(EInteRealControlMode::Edit);

	if (OpeningComponentAcceptsRow(SelectedSurfaceComponent.Get(), FurnitureRow))
	{
		if (UStaticMeshComponent* OpeningComp = Cast<UStaticMeshComponent>(SelectedSurfaceComponent.Get()))
		{
			if (FurnitureRow.FurnitureMesh)
			{
				PS->RecordUndoSnapshot();
				ApplyOpeningMeshPreservingSlot(OpeningComp, FurnitureRow.FurnitureMesh);
				SelectSurface(OpeningComp);
				return;
			}
		}
	}

	DeselectFurniture();

	if (PS->HasActivePreview()) PS->CancelPreview();

	const FVector SpawnLoc = bIsHitting ? CurrentCursorWorldLoc : FVector::ZeroVector;
	PS->CreatePreviewFurnitureFromRow(SpawnLoc, FRotator::ZeroRotator, FurnitureRow);
	bPreviewHiddenUntilViewport = IsMouseOverInteractiveUI();
	PS->SetPreviewHidden(bPreviewHiddenUntilViewport);

	if (FloorPlanPlacementSyncComponent)
	{
		FloorPlanPlacementSyncComponent->StartFloorPlan2DPlacement(FurnitureRow);
	}
}

void AInteRealPlayerController::FindViewModeManager()
{
	if (CachedViewModeManager) return;

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AViewModeManager::StaticClass(), FoundActors);

	if (FoundActors.Num() > 0)
	{
		CachedViewModeManager = Cast<AViewModeManager>(FoundActors[0]);
	}
}

void AInteRealPlayerController::SetViewMode(EHarnessViewMode NewMode)
{
	FindViewModeManager();
	if (!CachedViewModeManager) return;

	CachedViewModeManager->SetViewMode(NewMode);

	if (NewMode != EHarnessViewMode::FirstPerson)
	{
		if (CurrentControlMode == EInteRealControlMode::Edit)
		{
			if (AInteRealHUD* HUD = GetInteRealHUD())
			{
				if (UEditModeLayoutWidget* LayoutWidget = HUD->EditModeLayoutWidgetInstance)
				{
					CachedViewModeManager->SetFloorPlanPanelOffset(LayoutWidget->IsFloorPlanPanelOpen());
				}
				else
				{
					CachedViewModeManager->SetFloorPlanPanelOffset(false);
				}
			}
			else
			{
				CachedViewModeManager->SetFloorPlanPanelOffset(false);
			}
		}
		else
		{
			CachedViewModeManager->SetFloorPlanPanelOffset(false);
		}
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UInteRealUISubSystem* UISubsystem = GI->GetSubsystem<UInteRealUISubSystem>())
		{
			UISubsystem->NotifyViewModeChanged(NewMode);
		}
	}
	
	APawn* P = GetPawn();
	if (!P) return;

	if (NewMode == EHarnessViewMode::FirstPerson)
	{
		CachedViewModeManager->FocusOnBuilding();
		FVector Center = CachedViewModeManager->GetCameraTargetLocation();
		Center.Z = 160.0f;
		
		const FRotator FirstPersonRotation = CachedViewModeManager->IsCanvasRotated()
			? FRotator(0.f, -90.f, 0.f)
			: FRotator(0.f, 0.f, 0.f);
		const FVector SafeCenter = ResolveFirstPersonSafeLocation(GetWorld(), P, Center, FirstPersonRotation, 125.0f);

		P->SetActorLocation(SafeCenter);
		P->SetActorRotation(FirstPersonRotation);
		SetControlRotation(FirstPersonRotation);

		Possess(P);
		SetViewTarget(P);
		P->SetActorHiddenInGame(true);
	}
	else
	{
		SetViewTarget(CachedViewModeManager);
	}

	if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
	{
		InteRealHUD->UpdateMinimapIconVisibility(NewMode);
	}
}

void AInteRealPlayerController::SetupMinimapHUD(
	UHarnessMinimapCaptureComponent* InCaptureComp,
	UTextureRenderTarget2D* InRT,
	TSubclassOf<UInteRealMinimap> InWidgetClass)
{
	if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
	{
		EHarnessViewMode CurrentMode = EHarnessViewMode::Isometric;
		if (CachedViewModeManager)
		{
			CurrentMode = CachedViewModeManager->GetCurrentViewMode();
		}

		InteRealHUD->SetupMinimapHUD(
			InCaptureComp,
			InRT,
			InWidgetClass,
			CurrentMode,
			CurrentControlMode
		);
	}
}

void AInteRealPlayerController::ShowMinimap()
{
	if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
	{
		InteRealHUD->ShowMinimap(CurrentControlMode);
	}
}

void AInteRealPlayerController::OnTopDownKey()
{
	SetViewMode(EHarnessViewMode::TopDown);
}

void AInteRealPlayerController::OnIsometricKey()
{
	SetViewMode(EHarnessViewMode::Isometric);
}

void AInteRealPlayerController::OnFirstPersonKey()
{
	SetViewMode(EHarnessViewMode::FirstPerson);
}

void AInteRealPlayerController::OnToggleModeKey()
{
	if (CurrentControlMode == EInteRealControlMode::Edit)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UInteRealUISubSystem* UISubsystem = GI->GetSubsystem<UInteRealUISubSystem>())
			{
				UISubsystem->OnModeChanged.Broadcast(false);
			}
		}
	}
	else
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UInteRealUISubSystem* UISubsystem = GI->GetSubsystem<UInteRealUISubSystem>())
			{
				UISubsystem->OnModeChanged.Broadcast(true);
			}
		}
	}
}

void AInteRealPlayerController::OnFocusSelectionKey()
{
	FindViewModeManager();
	if (!CachedViewModeManager) return;

	if (!SelectedFurniture)
	{
		bIsFocusingFirstPerson = false;
		CachedViewModeManager->FocusOnBuilding();
		return;
	}

	const FBox Bounds = SelectedFurniture->GetComponentsBoundingBox(true);
	const FVector FurnitureCenter = Bounds.GetCenter();

	if (CachedViewModeManager->GetCurrentViewMode() == EHarnessViewMode::FirstPerson)
	{
		if (APawn* P = GetPawn())
		{
			// 가�?반경만큼 ?�어�??�치, Z??고정 ?�높??
			const float HalfExtent = FMath::Max(Bounds.GetExtent().X, Bounds.GetExtent().Y);
			const float ViewDistance = 150.0f;

			FVector Dir = (FurnitureCenter - P->GetActorLocation());
			Dir.Z = 0.f;
			Dir = Dir.GetSafeNormal();
			if (Dir.IsNearlyZero()) Dir = P->GetActorForwardVector();

			FVector Target = FurnitureCenter + (-Dir) * (HalfExtent + ViewDistance);
			Target.Z = 160.0f;
			Target = ResolveFirstPersonSafeLocation(GetWorld(), P, Target, GetControlRotation(), 175.0f);

			FirstPersonFocusTarget = Target;
			bIsFocusingFirstPerson = true;
		}
	}
	else
	{
		CachedViewModeManager->FocusOnLocation(FurnitureCenter);
	}
}

void AInteRealPlayerController::OnMoveKey(const FInputActionValue& Value)
{
	bIsFocusingFirstPerson = false;
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (GetViewTarget() == GetPawn())
	{
		APawn* P = GetPawn();
		if (P)
		{
			const FRotator Rotation = GetControlRotation();
			const FRotator YawRotation(0, Rotation.Yaw, 0);
			const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
			const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

			P->AddMovementInput(ForwardDirection, MovementVector.X);
			P->AddMovementInput(RightDirection, MovementVector.Y);
		}
	}
	else if (CachedViewModeManager)
	{
		CachedViewModeManager->AddMovementInput(FVector::ForwardVector, MovementVector.X);
		CachedViewModeManager->AddMovementInput(FVector::RightVector, MovementVector.Y);
	}
}

void AInteRealPlayerController::OnMoveVerticalKey(const FInputActionValue& Value)
{
	const float AxisValue = Value.Get<float>();
	if (FMath::IsNearlyZero(AxisValue)) return;

	if (GetViewTarget() == GetPawn())
	{
		if (APawn* P = GetPawn())
		{
			P->AddMovementInput(FVector::UpVector, AxisValue);
		}
	}
	else if (CachedViewModeManager)
	{
		CachedViewModeManager->AddMovementInput(FVector::UpVector, AxisValue);
	}
}

void AInteRealPlayerController::OnLookKey(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	bool bIsRightClick = IsInputKeyDown(EKeys::RightMouseButton);
	bool bIsMiddleClick = IsInputKeyDown(EKeys::MiddleMouseButton);

	if (GetViewTarget() == GetPawn())
	{
		if (bIsRightClick)
		{
			AddYawInput(LookAxisVector.X);
			AddPitchInput(-LookAxisVector.Y);
		}
	}
	else if (CachedViewModeManager)
	{
		EHarnessViewMode CurrentMode = CachedViewModeManager->GetCurrentViewMode();

		if (CurrentMode == EHarnessViewMode::TopDown)
		{
			if (bIsRightClick || bIsMiddleClick)
			{
				CachedViewModeManager->AddPanInput(LookAxisVector.X, -LookAxisVector.Y);
			}
		}
		else if (CurrentMode == EHarnessViewMode::Isometric)
		{
			if (bIsRightClick)
			{
				CachedViewModeManager->AddRotationInput(LookAxisVector.X, -LookAxisVector.Y);
			}
			else if (bIsMiddleClick)
			{
				CachedViewModeManager->AddPanInput(LookAxisVector.X, -LookAxisVector.Y);
			}
		}
	}
}

void AInteRealPlayerController::OnZoomKey(const FInputActionValue& Value)
{
	if (CachedViewModeManager && GetViewTarget() != GetPawn())
	{
		CachedViewModeManager->AddZoomInput(Value.Get<float>());
	}
}

void AInteRealPlayerController::OnUndoKey()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS) return;

	DeselectFurniture();
	DeselectSurface();
	if (PS->HasActivePreview()) PS->CancelPreview();
	PS->Undo();
	if (FloorPlanPlacementSyncComponent)
	{
		FloorPlanPlacementSyncComponent->RebuildFloorPlan2DFromPlacedFurniture();
	}
}

void AInteRealPlayerController::OnRedoKey()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS) return;

	DeselectFurniture();
	DeselectSurface();
	if (PS->HasActivePreview()) PS->CancelPreview();
	PS->Redo();
	if (FloorPlanPlacementSyncComponent)
	{
		FloorPlanPlacementSyncComponent->RebuildFloorPlan2DFromPlacedFurniture();
	}
}

void AInteRealPlayerController::OnCopyKey()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	if (!SelectedFurniture) return;
	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS) return;

	const FFurnitureDataRow* Row = PS->FindFurnitureRowByID(SelectedFurniture->FurnitureID);
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("Copy failed: furniture row not found. ID=%d"), SelectedFurniture->FurnitureID);
		return;
	}

	CopiedFurnitureRow = *Row;
	CopiedFurnitureRotation = SelectedFurniture->GetActorRotation();
	bHasCopiedFurniture = true;

	UE_LOG(LogTemp, Log, TEXT("Furniture copied. ID=%d"), SelectedFurniture->FurnitureID);
}

void AInteRealPlayerController::OnPasteKey()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	if (!bHasCopiedFurniture) return;
	if (!bIsHitting) return;
	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS) return;

	if (PS->HasActivePreview()) PS->CancelPreview();

	DeselectFurniture();
	DeselectSurface();

	PS->CreatePreviewFurnitureFromRow(CurrentCursorWorldLoc, CopiedFurnitureRotation, CopiedFurnitureRow);
	PS->UpdatePreviewLocation(LastCursorHit);

	if (FloorPlanPlacementSyncComponent)
	{
		FloorPlanPlacementSyncComponent->StartFloorPlan2DPlacement(CopiedFurnitureRow);
		FloorPlanPlacementSyncComponent->SyncPreview2DFromActivePreview();
	}
}

void AInteRealPlayerController::OnDuplicateKey()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	if (!bHasCopiedFurniture) return;
	if (!bIsHitting) return;

	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS) return;

	if (PS->HasActivePreview())
	{
		PS->CancelPreview();
	}

	DeselectFurniture();
	DeselectSurface();

	PS->CreatePreviewFurnitureFromRow(CurrentCursorWorldLoc, CopiedFurnitureRotation, CopiedFurnitureRow);
	PS->UpdatePreviewLocation(LastCursorHit);

	if (FloorPlanPlacementSyncComponent)
	{
		FloorPlanPlacementSyncComponent->StartFloorPlan2DPlacement(CopiedFurnitureRow);
		FloorPlanPlacementSyncComponent->SyncPreview2DFromActivePreview();
	}
}

void AInteRealPlayerController::OnSaveKey()
{
	if (!GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("[InteReal] Save requested but World is invalid."));
		return;
	}

	if (UHarnessPipelineManager* Pipeline = GetWorld()->GetSubsystem<UHarnessPipelineManager>())
	{
		Pipeline->SaveCurrentProject();
		UE_LOG(LogTemp, Log, TEXT("[InteReal] Save requested from player controller."));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[InteReal] Save requested but HarnessPipelineManager was not found."));
}

void AInteRealPlayerController::HandleFloorPlanPanelOpenChanged(bool bOpen)
{
	if (!CachedViewModeManager) return;

	if (CachedViewModeManager->GetCurrentViewMode() == EHarnessViewMode::FirstPerson)
	{
		return;
	}

	if (CurrentControlMode == EInteRealControlMode::Edit)
	{
		CachedViewModeManager->SetFloorPlanPanelOffset(bOpen);
	}
	else
	{
		CachedViewModeManager->SetFloorPlanPanelOffset(false);
	}
}

void AInteRealPlayerController::SnapshotPlacedFurnitureActorsForFloorPlanSync(TSet<TObjectKey<AFurniture>>& OutPlacedFurnitureKeys) const
{
	SnapshotPlacedFurnitureActors(OutPlacedFurnitureKeys);
}

void AInteRealPlayerController::SnapshotPlacedFurnitureActors(
	TSet<TObjectKey<AFurniture>>& OutPlacedFurnitureKeys
) const
{
	OutPlacedFurnitureKeys.Reset();

	const UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS)
	{
		return;
	}

	for (AFurniture* PlacedFurniture : PS->GetPlacedFurnitures())
	{
		if (IsValid(PlacedFurniture))
		{
			OutPlacedFurnitureKeys.Add(TObjectKey<AFurniture>(PlacedFurniture));
		}
	}
}

AFurniture* AInteRealPlayerController::ResolveConfirmedFurnitureActorForFloorPlanSync(AFurniture* PreviousPreviewFurniture, const TSet<TObjectKey<AFurniture>>& PreviouslyPlacedFurnitureKeys) const
{
	return ResolveConfirmedFurnitureActor(PreviousPreviewFurniture, PreviouslyPlacedFurnitureKeys);
}

AFurniture* AInteRealPlayerController::ConfirmActivePreviewFurnitureForFloorPlanSync(bool bContinuePlacement)
{
	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS || !PS->HasActivePreview())
	{
		return nullptr;
	}

	AFurniture* PreviewFurniture = PS->GetPreviewFurniture();
	if (!PreviewFurniture)
	{
		return nullptr;
	}

	const int32 FurnitureID = PreviewFurniture->FurnitureID;
	const FFurnitureDataRow* FurnitureRow = PS->FindFurnitureRowByID(FurnitureID);
	if (!FurnitureRow)
	{
		return nullptr;
	}

	const FVector ConfirmedWorldLocation = PreviewFurniture->GetActorLocation();
	const float ConfirmedYaw = PreviewFurniture->GetActorRotation().Yaw;

	TSet<TObjectKey<AFurniture>> PreviouslyPlacedFurnitureKeys;
	SnapshotPlacedFurnitureActorsForFloorPlanSync(PreviouslyPlacedFurnitureKeys);

	AFurniture* PreviewFurnitureBeforeConfirm = PreviewFurniture;

	PS->ConfirmFurniture(bContinuePlacement);

	AFurniture* ConfirmedFurniture = ResolveConfirmedFurnitureActorForFloorPlanSync(PreviewFurnitureBeforeConfirm, PreviouslyPlacedFurnitureKeys);

	if (FloorPlanPlacementSyncComponent)
	{
		FloorPlanPlacementSyncComponent->RegisterConfirmedFurnitureToFloorPlan(*FurnitureRow, ConfirmedWorldLocation, ConfirmedYaw, ConfirmedFurniture);
	}

	return ConfirmedFurniture;
}

AFurniture* AInteRealPlayerController::ResolveConfirmedFurnitureActor(
	AFurniture* PreviousPreviewFurniture,
	const TSet<TObjectKey<AFurniture>>& PreviouslyPlacedFurnitureKeys
) const
{
	if (IsValid(PreviousPreviewFurniture) &&
		PreviousPreviewFurniture->GetPlacementState() == EPlacementState::Placed)
	{
		return PreviousPreviewFurniture;
	}

	const UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS)
	{
		return IsValid(PreviousPreviewFurniture) ? PreviousPreviewFurniture : nullptr;
	}

	for (AFurniture* PlacedFurniture : PS->GetPlacedFurnitures())
	{
		if (!IsValid(PlacedFurniture))
		{
			continue;
		}

		if (!PreviouslyPlacedFurnitureKeys.Contains(TObjectKey<AFurniture>(PlacedFurniture)))
		{
			return PlacedFurniture;
		}
	}

	return IsValid(PreviousPreviewFurniture) ? PreviousPreviewFurniture : nullptr;
}

void AInteRealPlayerController::DeleteFurnitureForFloorPlanSync(AFurniture* Furniture)
{
	DeleteFurnitureActor(Furniture);
}

void AInteRealPlayerController::ClearFurnitureSelectionForFloorPlanSync()
{
	ClearFurnitureSelectionInternal(false);
}

void AInteRealPlayerController::SyncFurnitureSizeChangeToFloorPlan2D(AFurniture* Furniture)
{
	if (!IsValid(Furniture))
	{
		return;
	}

	if (FloorPlanPlacementSyncComponent)
	{
		FloorPlanPlacementSyncComponent->SyncFloorPlan2DFromFurniture(Furniture);
	}

	if (SelectedFurniture.Get() == Furniture)
	{
		if (UInteRealGizmoComponent* Gizmo = GetSelectedGizmoComponent())
		{
			Gizmo->UpdateAnchorFromOwner();
		}
	}
}

void AInteRealPlayerController::DeleteFurnitureActor(AFurniture* FurnitureActor)
{
	if (!IsValid(FurnitureActor))
	{
		return;
	}

	if (SelectedFurniture == FurnitureActor)
	{
		DeselectFurniture();
	}

	if (UInteriorPlacementSubsystem* PS = GetPlacementSubsystem())
	{
		PS->RemoveFurniture(FurnitureActor);
	}

	if (IsValid(FurnitureActor) && !FurnitureActor->IsActorBeingDestroyed())
	{
		FurnitureActor->Destroy();
	}
}

void AInteRealPlayerController::ClearFurnitureSelectionInternal(bool bSyncFloorPlan2D)
{
	if (SelectedFurniture)
	{
		if (UInteRealGizmoComponent* Gizmo = SelectedFurniture->GetGizmoComponent())
		{
			Gizmo->EndDrag();
			Gizmo->SetSelectedActive(false);
		}
		SelectedFurniture->SetSelected(false);
		SelectedFurniture = nullptr;
	}

	if (AInteRealHUD* HUD = GetInteRealHUD())
	{
		HUD->ShowFurnitureSizePanel(nullptr);
		HUD->ShowLightAttributesPanel(nullptr);
		HUD->ShowMaterialAttributesPanel(false);
	}

	bIsMovingFurniture = false;
	bIsGizmoRotationWidgetActive = false;
	DragStartFurnitureLocation = FVector::ZeroVector;
	MoveDragOffset = FVector::ZeroVector;

	if (bSyncFloorPlan2D && FloorPlanPlacementSyncComponent && !FloorPlanPlacementSyncComponent->IsSyncingFurniture3DFrom2D())
	{
		FloorPlanPlacementSyncComponent->ClearFloorPlan2DSelection();
	}
}

UInteRealGizmoComponent* AInteRealPlayerController::GetSelectedGizmoComponent() const
{
	return SelectedFurniture ? SelectedFurniture->GetGizmoComponent() : nullptr;
}

bool AInteRealPlayerController::TryGetSurfaceMaterialData(UMeshComponent* SurfaceComponent, FMaterialDataRow& OutMaterialData) const
{
	if (!SurfaceComponent) return false;

	const FMaterialDataRow* FoundData = SurfaceMaterialDataMap.Find(TObjectKey<UMeshComponent>(SurfaceComponent));
	if (!FoundData) return false;

	OutMaterialData = *FoundData;
	return true;
}

void AInteRealPlayerController::StoreSurfaceMaterialData(UMeshComponent* SurfaceComponent, const FMaterialDataRow& MaterialData)
{
	if (!SurfaceComponent) return;

	SurfaceMaterialDataMap.Add(TObjectKey<UMeshComponent>(SurfaceComponent), MaterialData);
}

void AInteRealPlayerController::OnGizmoRotationChanged(float NewYawDegrees)
{
	if (!SelectedFurniture) return;

	FRotator NewRot = SelectedFurniture->GetActorRotation();
	NewRot.Yaw = NewYawDegrees;
	SelectedFurniture->SetActorRotation(NewRot);

	if (UInteRealGizmoComponent* Gizmo = GetSelectedGizmoComponent())
	{
		Gizmo->UpdateAnchorFromOwner();
	}

	if (UInteriorPlacementSubsystem* PS = GetPlacementSubsystem())
	{
		PS->UpdateGizmoRotation(SelectedFurniture);
	}

	if (FloorPlanPlacementSyncComponent)
	{
		FloorPlanPlacementSyncComponent->SyncFloorPlan2DFromFurniture(SelectedFurniture);
	}
}
