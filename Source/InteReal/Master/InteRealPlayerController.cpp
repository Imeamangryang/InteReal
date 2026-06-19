#include "InteRealPlayerController.h"
#include "InteRealHUD.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "InteReal/EditMode/Gizmo/InteRealGizmoActor.h"
#include "InteReal/EditMode/2D/InteReal2DFloorPlanViewportWidget.h"
#include "InteReal/ViewMode/ViewModeManager.h"
#include "InteReal/Harness/Public/HarnessPipelineManager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/BoxComponent.h"
#include "CollisionShape.h"
#include "Kismet/GameplayStatics.h"
#include "Components/Overlay.h"
#include "Public/HarnessCaptureMinimapWidget.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "InteReal/SubSystems/InteRealUISubSystem.h"
#include "Engine/GameInstance.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"

static FVector GetGizmoAnchorLocation(const AFurniture* Furniture);

AInteRealPlayerController::AInteRealPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	PrimaryActorTick.bCanEverTick = true;
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
			UISubsystem->OnWallMaterialChanged.AddDynamic(this, &AInteRealPlayerController::HandleWallMaterialChanged);
			UISubsystem->OnIconClicked.AddDynamic(this, &AInteRealPlayerController::HandleIconClicked);
		}
	}

	// DefaultPawn 초기 �정
	if (APawn* P = GetPawn())
	{
		P->SetActorHiddenInGame(true);
	}

	ApplyCurrentControlMode();

	// View mode 기본 �태 �기	if (CachedViewModeManager)
	{
		SetViewMode(EHarnessViewMode::Isometric);
	}
}

void AInteRealPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsFocusingFirstPerson)
	{
		if (APawn* P = GetPawn())
		{
			const FVector NewLoc = FMath::VInterpTo(P->GetActorLocation(), FirstPersonFocusTarget, DeltaTime, 8.0f);
			P->SetActorLocation(NewLoc);
			if (FVector::DistSquared2D(NewLoc, FirstPersonFocusTarget) < 4.0f)
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

	if (SpawnedGizmo)
	{
		if (PlayerCameraManager)
		{
			const bool bFirstPersonGizmo =
				CachedViewModeManager &&
				CachedViewModeManager->GetCurrentViewMode() == EHarnessViewMode::FirstPerson;

			if (SelectedFurniture)
			{
				SpawnedGizmo->SetActorLocation(GetGizmoAnchorLocation(SelectedFurniture));
			}

			SpawnedGizmo->UpdateConstantScreenSize(
				PlayerCameraManager->GetCameraLocation(),
				PlayerCameraManager->GetFOVAngle(),
				bFirstPersonGizmo ? FirstPersonGizmoScaleMultiplier : 1.0f);
		}

		if (!SpawnedGizmo->IsDragging())
		{
			SpawnedGizmo->UpdateHover(bIsHitting, LastCursorHit);
		}
	}

	if (SpawnedGizmo && SpawnedGizmo->IsDragging() && SelectedFurniture)
	{
		FVector WorldOrigin, WorldDir;
		DeprojectMousePositionToWorld(WorldOrigin, WorldDir);
		FVector2D MousePos;
		GetMousePosition(MousePos.X, MousePos.Y);
		SpawnedGizmo->UpdateDrag(SelectedFurniture, WorldOrigin, WorldDir, GetPlacementSubsystem(), IsInputKeyDown(EKeys::LeftControl), MousePos);
		SyncFloorPlan2DFromFurniture(SelectedFurniture);
		
		if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
		{
			const EGizmoTransformAxis DraggedAxis = SpawnedGizmo->GetCurrentAxis();
			const bool bRotating =
				DraggedAxis == EGizmoTransformAxis::RotateYaw ||
				DraggedAxis == EGizmoTransformAxis::RotatePitch ||
				DraggedAxis == EGizmoTransformAxis::RotateRoll;

			FVector2D GuideScreenPos = FVector2D::ZeroVector;
			const bool bHasGuideAnchor = ProjectWorldLocationToScreen(SpawnedGizmo->GetActorLocation(), GuideScreenPos);
			InteRealHUD->UpdateRotationGuide(
				bRotating && bHasGuideAnchor,
				SpawnedGizmo->GetCurrentRotationDeltaDegrees(),
				GuideScreenPos);
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
		SyncFloorPlan2DFromFurniture(SelectedFurniture);
		return;
	}

	if (!PS2 || !bIsHitting) return;
	if (!PS2->HasActivePreview()) return;

	PS2->UpdatePreviewLocation(LastCursorHit);

	if (AFurniture* PreviewFurniture = PS2->GetPreviewFurniture())
	{
		if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
		{
			if (UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = InteRealHUD->GetFloorPlan2DWidget())
			{
				const FVector PreviewLocation = PreviewFurniture->GetActorLocation();

				FloorPlan2DWidget->SetFurniturePreviewAtDocumentPosition(
					FVector2D(PreviewLocation.X, PreviewLocation.Y),
					PreviewFurniture->GetActorRotation().Yaw
				);
			}
		}
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

	CurrentControlMode = NewMode;
	ApplyCurrentControlMode();
}

void AInteRealPlayerController::HandleModeChanged(bool bIsEditMode)
{
	if (bIsEditMode)
	{
		SetControlMode(EInteRealControlMode::Edit);
	}
	else
	{
		SetControlMode(EInteRealControlMode::View);
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

void AInteRealPlayerController::HandleWallMaterialChanged(UMaterialInterface* NewMaterial)
{
	ApplyMaterialToSelectedSurface(NewMaterial);
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
}

void AInteRealPlayerController::DeselectSurface()
{
	if (SelectedSurfaceComponent)
	{
		SelectedSurfaceComponent->SetRenderCustomDepth(false);
		SelectedSurfaceComponent = nullptr;
	}
}

void AInteRealPlayerController::ApplyMaterialToSelectedWall(UMaterialInterface* NewMaterial)
{
	ApplyMaterialToSelectedSurface(NewMaterial);
}

void AInteRealPlayerController::ApplyMaterialToSelectedSurface(UMaterialInterface* NewMaterial)
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	if (!SelectedSurfaceComponent) return;
	if (!NewMaterial) return;

	if (UInteriorPlacementSubsystem* PS = GetPlacementSubsystem())
		PS->RecordUndoSnapshot();

	SelectedSurfaceComponent->SetMaterial(0, NewMaterial);
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
			if (PS->HasActivePreview()) PS->CancelPreview();

		if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
		{
			if (UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = InteRealHUD->GetFloorPlan2DWidget())
			{
				FloorPlan2DWidget->CancelFurniturePlacement();
			}
		}
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

// 기즈모 트레이스 결과 중 Move 축 콜리전을 우선 선택
static bool IsRotateGizmoTag(const FString& Tag)
{
	return Tag.StartsWith(TEXT("Rotate")) || Tag == TEXT("RotationRing");
}

static FVector GetGizmoAnchorLocation(const AFurniture* Furniture)
{
	if (!Furniture)
	{
		return FVector::ZeroVector;
	}

	const FBox VisualBounds = Furniture->GetVisualBounds();
	return VisualBounds.IsValid ? VisualBounds.GetCenter() : Furniture->GetActorLocation();
}

static bool SelectGizmoHit(const TArray<FHitResult>& Hits, const AActor* Gizmo, FHitResult& OutHit)
{
	const FHitResult* FirstGizmoHit = nullptr;

	for (const FHitResult& Hit : Hits)
	{
		if (Hit.GetActor() != Gizmo) continue;

		if (!FirstGizmoHit)
		{
			FirstGizmoHit = &Hit;
		}

		const UPrimitiveComponent* Comp = Hit.GetComponent();
		if (!Comp) continue;

		const FString AxisTag = AInteRealGizmoActor::GetAxisTagFromComponent(Comp);
		if (AxisTag.StartsWith(TEXT("Move")))
		{
			OutHit = Hit;
			return true;
		}
	}

	if (FirstGizmoHit)
	{
		OutHit = *FirstGizmoHit;
		return true;
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
		
		if (SpawnedGizmo)
		{
			DeprojectMousePositionToWorld(WorldOrigin, WorldDir);

			TArray<FHitResult> Hits;
			GetWorld()->LineTraceMultiByChannel(
				Hits,
				WorldOrigin,
				WorldOrigin + WorldDir * 100000.f,
				ECC_Visibility,
				Params
			);

			FHitResult GizmoHit;
			if (SelectGizmoHit(Hits, SpawnedGizmo, GizmoHit))
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
					Params
				);

				if (SelectGizmoHit(Hits, SpawnedGizmo, GizmoHit))
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

	// 기즈모가 선택된 가구 메시에 가려져도 우선적으로 인식되도록 별도 검사
	if (SpawnedGizmo)
	{
		FVector WorldOrigin, WorldDir;
		DeprojectMousePositionToWorld(WorldOrigin, WorldDir);

		FCollisionQueryParams GizmoParams(NAME_None, true);
		if (APawn* P = GetPawn())
		{
			GizmoParams.AddIgnoredActor(P);
		}
		if (SelectedFurniture)
		{
			GizmoParams.AddIgnoredActor(SelectedFurniture);
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
		if (SelectGizmoHit(Hits, SpawnedGizmo, GizmoHit))
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

			if (SelectGizmoHit(Hits, SpawnedGizmo, GizmoHit))
			{
				LastCursorHit = GizmoHit;
				bIsHitting = true;
				CurrentCursorWorldLoc = GizmoHit.Location;
				return;
			}
		}
	}

	// 바닥/가구 등 기본 트레이스
	FHitResult FloorHit;
	FHitResult WallHit;
	bool bHitFloor = false;
	bool bHitWall = false;

	{
		FVector WorldOrigin, WorldDir;
		DeprojectMousePositionToWorld(WorldOrigin, WorldDir);
		const FVector TraceEnd = WorldOrigin + WorldDir * 100000.f;
		
		FCollisionQueryParams Params(NAME_None, true);
		if (APawn* P = GetPawn())
		{
			Params.AddIgnoredActor(P);
		}

		// 배치 미리보기 중에는 이미 배치된 가구들이 커서 트레이스를 가로막지 않도록 무시 처리
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

		// 벽 전용 트레이스 — 바닥에 가려지지 않아 벽 전체 높이에서 잡힘
		bHitWall = GetWorld()->LineTraceSingleByChannel(WallHit, WorldOrigin, TraceEnd, WallTraceChannel, Params);
	}

	// 배치 중인 가구가 벽 배치를 지원하지 않으면, 화면상 더 가까운 벽이 잡히더라도 무시하고 바닥 히트를 쓴다.
	// (ISO 시점에서는 다른 방의 벽이 화면상 커서 아래 바닥보다 카메라에 더 가깝게 잡혀 Wall로 오판되는 경우가 있음)
	UInteriorPlacementSubsystem* PS3 = GetPlacementSubsystem();
	const AFurniture* PreviewFurniture = (PS3 && PS3->HasActivePreview()) ? PS3->GetPreviewFurniture() : nullptr;
	const bool bPreviewSupportsWall = !PreviewFurniture || PreviewFurniture->SupportsPlacementType(EPlacementSurfaceType::Wall);
	const bool bPreviewSupportsFloor = PreviewFurniture && PreviewFurniture->SupportsPlacementType(EPlacementSurfaceType::Floor);
	const bool bPreviewFloorOnly =
		PreviewFurniture &&
		bPreviewSupportsFloor &&
		!PreviewFurniture->SupportsPlacementType(EPlacementSurfaceType::Wall) &&
		!PreviewFurniture->SupportsPlacementType(EPlacementSurfaceType::Surface) &&
		!PreviewFurniture->SupportsPlacementType(EPlacementSurfaceType::Ceiling);

	// 화면상 같은 픽셀에서 둘 다 잡히면 카메라에 더 가까운(거리가 짧은) 쪽을 채택
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

	if (bIsHitting && bPreviewFloorOnly && PS3)
	{
		const UPrimitiveComponent* HitComp = LastCursorHit.GetComponent();
		const bool bHitLooksLikeWall =
			(HitComp && HitComp->ComponentHasTag(TEXT("EditableWall"))) ||
			LastCursorHit.Location.Z > PS3->GetFloorZ() + 5.0f;

		if (bHitLooksLikeWall)
		{
			FVector WorldOrigin;
			FVector WorldDir;
			DeprojectMousePositionToWorld(WorldOrigin, WorldDir);

			const FVector FloorPoint = FMath::LinePlaneIntersection(
				WorldOrigin,
				WorldOrigin + WorldDir * 100000.0f,
				FPlane(FVector(0.0f, 0.0f, PS3->GetFloorZ()), FVector::UpVector));

			LastCursorHit.Location = FloorPoint;
			LastCursorHit.ImpactPoint = FloorPoint;
			LastCursorHit.ImpactNormal = FVector::UpVector;
			LastCursorHit.Normal = FVector::UpVector;
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

void AInteRealPlayerController::OnPlaceKey()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS) return;

	if (PS->HasActivePreview())
	{
		if (bIsHitting)
		{
			TSet<TObjectKey<AFurniture>> PreviouslyPlacedFurnitureKeys;
			SnapshotPlacedFurnitureActors(PreviouslyPlacedFurnitureKeys);

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

			const bool bContinuePlacement = bContinuousModifierHeld;
			PS->ConfirmFurniture(bContinuePlacement);

			AFurniture* ConfirmedFurniture = ResolveConfirmedFurnitureActor(
				PreviewFurniture,
				PreviouslyPlacedFurnitureKeys
			);

			if (FurnitureRow)
			{
				if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
				{
					if (UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = InteRealHUD->GetFloorPlan2DWidget())
					{
						BindFloorPlan2DEvents();

						FloorPlan2DWidget->AddPlacedFurnitureAtDocumentPosition(
							*FurnitureRow,
							FVector2D(ConfirmedWorldLocation.X, ConfirmedWorldLocation.Y),
							ConfirmedYaw
						);

						RegisterLastAddedFloorPlan2DFurnitureActor(
							FloorPlan2DWidget,
							ConfirmedFurniture
						);

						if (bContinuePlacement)
						{
							FloorPlan2DWidget->StartFurniturePlacement(*FurnitureRow);
						}
						else
						{
							FloorPlan2DWidget->CancelFurniturePlacement();

							if (IsValid(ConfirmedFurniture))
							{
								SelectFurniture(ConfirmedFurniture);
							}
						}
					}
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

	if (HitComp && SelectedFurniture && SpawnedGizmo)
	{
		const FString AxisTag = AInteRealGizmoActor::GetAxisTagFromComponent(HitComp);
		if (!AxisTag.IsEmpty())
		{
			if (AxisTag.StartsWith(TEXT("Move")))
			{
				FVector WorldOrigin, WorldDir;
				DeprojectMousePositionToWorld(WorldOrigin, WorldDir);
				FVector2D MousePos;
				GetMousePosition(MousePos.X, MousePos.Y);
				SpawnedGizmo->BeginDrag(AxisTag, SelectedFurniture, WorldOrigin, WorldDir, MousePos);

				if (UInteriorPlacementSubsystem* PSGizmo = GetPlacementSubsystem())
					PSGizmo->BeginGizmoMove(SelectedFurniture);
				return;
			}

			if (IsRotateGizmoTag(AxisTag))
			{
				if (UInteriorPlacementSubsystem* PSSnap = GetPlacementSubsystem())
				{
					PSSnap->RecordUndoSnapshot();
				}

				FVector WorldOrigin, WorldDir;
				DeprojectMousePositionToWorld(WorldOrigin, WorldDir);
				FVector2D MousePos;
				GetMousePosition(MousePos.X, MousePos.Y);
				SpawnedGizmo->BeginDrag(AxisTag, SelectedFurniture, WorldOrigin, WorldDir, MousePos);
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
				// �택가구� �시 �릭 �동 �래규작
				bIsMovingFurniture = true;
				DragStartFurnitureLocation = SelectedFurniture->GetActorLocation();
				MoveDragOffset = DragStartFurnitureLocation - CurrentCursorWorldLoc;
				MoveDragOffset.Z = 0.0f;

				if (UInteriorPlacementSubsystem* PSMove = GetPlacementSubsystem())
					PSMove->BeginGizmoMove(SelectedFurniture);
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

	if (SpawnedGizmo)
	{
		const EGizmoTransformAxis DraggedAxis = SpawnedGizmo->GetCurrentAxis();

		// 축 이동 확정 (그리드 스냅)
		if ((DraggedAxis == EGizmoTransformAxis::MoveX ||
		     DraggedAxis == EGizmoTransformAxis::MoveY ||
		     DraggedAxis == EGizmoTransformAxis::MoveZ) && SelectedFurniture)
		{
			if (UInteriorPlacementSubsystem* PSFin = GetPlacementSubsystem())
				PSFin->FinalizeGizmoMove(SelectedFurniture);

			SelectedFurniture->SetSelected(true);
		}

		SpawnedGizmo->EndDrag();
	}

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
		SyncFloorPlan2DFromFurniture(SelectedFurniture);
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

		if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
		{
			if (UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = InteRealHUD->GetFloorPlan2DWidget())
			{
				FloorPlan2DWidget->CancelFurniturePlacement();
			}
		}
		return;
	}

	if (SelectedFurniture)
	{
		AFurniture* ToRemove = SelectedFurniture;

		bIsDeletingFurnitureFrom3D = true;
		RemoveFloorPlan2DForFurniture(ToRemove);
		bIsDeletingFurnitureFrom3D = false;

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
		return;
	}

	if (SelectedFurniture)
	{
		if (PS) PS->RecordUndoSnapshot();
		FRotator Rot = SelectedFurniture->GetActorRotation();
		Rot.Yaw = FRotator::NormalizeAxis(Rot.Yaw + AngleDeg);
		SelectedFurniture->SetActorRotation(Rot);
		if (SpawnedGizmo)
		{
			SpawnedGizmo->SetActorRotation(Rot);
		}
		SyncFloorPlan2DFromFurniture(SelectedFurniture);
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

	// 이미 배치된 가구를 선택한 상태에서 Shift를 누르면, 그 가구를 같은 종류의 새 프리뷰로
	// 다시 꺼내서 연속배치를 시작할 수 있게 한다.
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

void AInteRealPlayerController::SelectFurniture(AFurniture* Furniture)
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	if (SelectedFurniture == Furniture) return;

	DeselectSurface();
	DeselectFurniture();
	SelectedFurniture = Furniture;

	if (SelectedFurniture)
	{
		SelectedFurniture->SetSelected(true);

		// ---- Gizmo ----
		if (GizmoActorClass && GetWorld())
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			const FVector GizmoSpawnLoc = GetGizmoAnchorLocation(SelectedFurniture);

			SpawnedGizmo = GetWorld()->SpawnActor<AInteRealGizmoActor>(
				GizmoActorClass,
				GizmoSpawnLoc,
				SelectedFurniture->GetActorRotation(),
				Params);

			if (SpawnedGizmo)
			{
				FAttachmentTransformRules AttachRules(
					EAttachmentRule::KeepWorld,
					EAttachmentRule::KeepWorld,
					EAttachmentRule::KeepWorld,
					false
				);

				SpawnedGizmo->AttachToActor(SelectedFurniture, AttachRules);

				if (USceneComponent* GizmoRoot = SpawnedGizmo->GetRootComponent())
				{
					GizmoRoot->SetUsingAbsoluteRotation(true);
				}

				SpawnedGizmo->InitAxisMaterials();
			}
		}
		
		if (!bIsSyncingFurniture3DFrom2D)
		{
			FGuid InstanceGuid;
			if (FindFloorPlan2DGuidForFurniture(SelectedFurniture, InstanceGuid))
			{
				if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
				{
					if (UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = InteRealHUD->GetFloorPlan2DWidget())
					{
						bIsSyncingFloorPlan2DFrom3D = true;
						FloorPlan2DWidget->SelectPlacedFurnitureByGuid(InstanceGuid);
						bIsSyncingFloorPlan2DFrom3D = false;
					}
				}
			}
		}
	}
}

void AInteRealPlayerController::DeselectFurniture()
{
	if (SpawnedGizmo)
	{
		SpawnedGizmo->Destroy();
		SpawnedGizmo = nullptr;
	}
	
	if (SelectedFurniture)
	{
		SelectedFurniture->SetSelected(false);
		SelectedFurniture = nullptr;
	}
	bIsMovingFurniture = false;
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
						BindFloorPlan2DEvents();

						FloorPlan2DWidget->AddPlacedFurnitureAtDocumentPosition(
							*FurnitureRow,
							FVector2D(ConfirmedWorldLocation.X, ConfirmedWorldLocation.Y),
							ConfirmedYaw
						);

						RegisterLastAddedFloorPlan2DFurnitureActor(
							FloorPlan2DWidget,
							ConfirmedFurniture
						);

						FloorPlan2DWidget->CancelFurniturePlacement();

						if (IsValid(ConfirmedFurniture))
						{
							SelectFurniture(ConfirmedFurniture);
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

			if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
			{
				if (UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = InteRealHUD->GetFloorPlan2DWidget())
				{
					FloorPlan2DWidget->CancelFurniturePlacement();
				}
			}
		}
	}
	else if (Action == TEXT("LOAD"))
	{
		FString Payload;
		if (Root->TryGetStringField(TEXT("data"), Payload))
			PS->ImportPlacedFurnituresJson(Payload);
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
	DeselectFurniture();

	if (PS->HasActivePreview()) PS->CancelPreview();

	const FVector SpawnLoc = bIsHitting ? CurrentCursorWorldLoc : FVector::ZeroVector;
	PS->CreatePreviewFurnitureFromRow(SpawnLoc, FRotator::ZeroRotator, FurnitureRow);

	if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
	{
		if (UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = InteRealHUD->GetFloorPlan2DWidget())
		{
			BindFloorPlan2DEvents();

			FloorPlan2DWidget->StartFurniturePlacement(FurnitureRow);
		}
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

	// SetControlMode(EInteRealControlMode::View);
	CachedViewModeManager->SetViewMode(NewMode);

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UInteRealUISubSystem* UISubsystem =
			GI->GetSubsystem<UInteRealUISubSystem>())
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
		P->SetActorLocation(Center);

		if (CachedViewModeManager->IsCanvasRotated())
		{
			FRotator Rot(0.f, -90.f, 0.f);
			P->SetActorRotation(Rot);
			SetControlRotation(Rot);
		}
		else
		{
			FRotator Rot(0.f, 0.f, 0.f);
			P->SetActorRotation(Rot);
			SetControlRotation(Rot);
		}

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
			// 가구 반경만큼 떨어진 위치, Z는 고정 눈높이
			const float HalfExtent = FMath::Max(Bounds.GetExtent().X, Bounds.GetExtent().Y);
			const float ViewDistance = 150.0f;

			FVector Dir = (FurnitureCenter - P->GetActorLocation());
			Dir.Z = 0.f;
			Dir = Dir.GetSafeNormal();
			if (Dir.IsNearlyZero()) Dir = P->GetActorForwardVector();

			FVector Target = FurnitureCenter + (-Dir) * (HalfExtent + ViewDistance);
			Target.Z = 160.0f;

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
	PS->ConfirmFurniture();
}

void AInteRealPlayerController::OnDuplicateKey()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	if (!SelectedFurniture) return;
	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS) return;

	OnCopyKey();
	if (!bHasCopiedFurniture) return;

	FVector PasteLocation = bIsHitting
		? CurrentCursorWorldLoc
		: SelectedFurniture->GetActorLocation() + FVector(100.0f, 100.0f, 0.0f);

	if (PS->HasActivePreview()) PS->CancelPreview();

	PS->CreatePreviewFurnitureFromRow(PasteLocation, CopiedFurnitureRotation, CopiedFurnitureRow);
	PS->ConfirmFurniture();
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

void AInteRealPlayerController::HandleFloorPlan2DFurniturePlacementRequested(FVector2D DocumentPosition)
{
	if (CurrentControlMode != EInteRealControlMode::Edit)
	{
		return;
	}

	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS || !PS->HasActivePreview())
	{
		return;
	}

	AFurniture* PreviewFurniture = PS->GetPreviewFurniture();
	if (!PreviewFurniture)
	{
		return;
	}

	const int32 FurnitureID = PreviewFurniture->FurnitureID;
	const FFurnitureDataRow* FurnitureRow = PS->FindFurnitureRowByID(FurnitureID);
	if (!FurnitureRow)
	{
		return;
	}

	const float FloorZ = PreviewFurniture->GetActorLocation().Z;
	const FVector RequestedWorldLocation(
		DocumentPosition.X,
		DocumentPosition.Y,
		FloorZ
	);

	FHitResult FloorHit;
	FloorHit.bBlockingHit = true;
	FloorHit.Location = RequestedWorldLocation;
	FloorHit.ImpactPoint = RequestedWorldLocation;
	FloorHit.ImpactNormal = FVector::UpVector;
	FloorHit.Normal = FVector::UpVector;

	PS->UpdatePreviewLocation(FloorHit);

	PreviewFurniture = PS->GetPreviewFurniture();
	if (!PreviewFurniture)
	{
		return;
	}

	if (PS->InvalidReason != EPlacementInvalidReason::None)
	{
		return;
	}

	const FVector ConfirmedWorldLocation = PreviewFurniture->GetActorLocation();
	const float ConfirmedYaw = PreviewFurniture->GetActorRotation().Yaw;

	TSet<TObjectKey<AFurniture>> PreviouslyPlacedFurnitureKeys;
	SnapshotPlacedFurnitureActors(PreviouslyPlacedFurnitureKeys);

	AFurniture* PreviewFurnitureBeforeConfirm = PreviewFurniture;

	PS->ConfirmFurniture(false);

	AFurniture* ConfirmedFurniture = ResolveConfirmedFurnitureActor(
		PreviewFurnitureBeforeConfirm,
		PreviouslyPlacedFurnitureKeys
	);

	if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
	{
		if (UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = InteRealHUD->GetFloorPlan2DWidget())
		{
			BindFloorPlan2DEvents();

			FloorPlan2DWidget->AddPlacedFurnitureAtDocumentPosition(
				*FurnitureRow,
				FVector2D(ConfirmedWorldLocation.X, ConfirmedWorldLocation.Y),
				ConfirmedYaw
			);

			RegisterLastAddedFloorPlan2DFurnitureActor(
				FloorPlan2DWidget,
				ConfirmedFurniture
			);

			FloorPlan2DWidget->CancelFurniturePlacement();

			if (IsValid(ConfirmedFurniture))
			{
				SelectFurniture(ConfirmedFurniture);
			}
		}
	}
}

void AInteRealPlayerController::HandleFloorPlan2DFurniturePreviewMoved(FVector2D DocumentPosition)
{
	if (CurrentControlMode != EInteRealControlMode::Edit)
	{
		return;
	}

	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS || !PS->HasActivePreview())
	{
		return;
	}

	AFurniture* PreviewFurniture = PS->GetPreviewFurniture();
	if (!PreviewFurniture)
	{
		return;
	}

	const float FloorZ = PreviewFurniture->GetActorLocation().Z;
	const FVector RequestedWorldLocation(
		DocumentPosition.X,
		DocumentPosition.Y,
		FloorZ
	);

	FHitResult FloorHit;
	FloorHit.bBlockingHit = true;
	FloorHit.Location = RequestedWorldLocation;
	FloorHit.ImpactPoint = RequestedWorldLocation;
	FloorHit.ImpactNormal = FVector::UpVector;
	FloorHit.Normal = FVector::UpVector;

	PS->UpdatePreviewLocation(FloorHit);

	PreviewFurniture = PS->GetPreviewFurniture();
	if (!PreviewFurniture)
	{
		return;
	}

	const FVector SnappedWorldLocation = PreviewFurniture->GetActorLocation();
	const float SnappedYaw = PreviewFurniture->GetActorRotation().Yaw;

	if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
	{
		if (UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = InteRealHUD->GetFloorPlan2DWidget())
		{
			FloorPlan2DWidget->SetFurniturePreviewAtDocumentPosition(
				FVector2D(SnappedWorldLocation.X, SnappedWorldLocation.Y),
				SnappedYaw
			);
		}
	}
}

void AInteRealPlayerController::HandleFloorPlan2DPlacedFurnitureSelected(
	int32 FurnitureIndex,
	FInteReal2DPlacedFurniture Furniture
)
{
	if (CurrentControlMode != EInteRealControlMode::Edit || bIsSyncingFloorPlan2DFrom3D)
	{
		return;
	}

	TWeakObjectPtr<AFurniture>* FurnitureActorPtr =
		FloorPlan2DFurnitureActors.Find(Furniture.InstanceGuid);

	if (!FurnitureActorPtr || !FurnitureActorPtr->IsValid())
	{
		return;
	}

	bIsSyncingFurniture3DFrom2D = true;
	SelectFurniture(FurnitureActorPtr->Get());
	bIsSyncingFurniture3DFrom2D = false;
}

void AInteRealPlayerController::HandleFloorPlan2DPlacedFurnitureMoved(
	int32 FurnitureIndex,
	FInteReal2DPlacedFurniture Furniture
)
{
	if (CurrentControlMode != EInteRealControlMode::Edit)
	{
		return;
	}

	TWeakObjectPtr<AFurniture>* FurnitureActorPtr =
		FloorPlan2DFurnitureActors.Find(Furniture.InstanceGuid);

	if (!FurnitureActorPtr || !FurnitureActorPtr->IsValid())
	{
		return;
	}

	AFurniture* FurnitureActor = FurnitureActorPtr->Get();

	if (SelectedFurniture != FurnitureActor)
	{
		bIsSyncingFurniture3DFrom2D = true;
		SelectFurniture(FurnitureActor);
		bIsSyncingFurniture3DFrom2D = false;
	}

	if (!bIsMovingFurnitureFromFloorPlan2D)
	{
		bIsMovingFurnitureFromFloorPlan2D = true;

		if (UInteriorPlacementSubsystem* PS = GetPlacementSubsystem())
		{
			PS->BeginGizmoMove(FurnitureActor);
		}
	}

	SyncFurnitureActorFromFloorPlan2D(Furniture);
}

void AInteRealPlayerController::HandleFloorPlan2DPlacedFurnitureMoveEnded(
	int32 FurnitureIndex,
	FInteReal2DPlacedFurniture Furniture
)
{
	if (CurrentControlMode != EInteRealControlMode::Edit)
	{
		return;
	}

	TWeakObjectPtr<AFurniture>* FurnitureActorPtr =
		FloorPlan2DFurnitureActors.Find(Furniture.InstanceGuid);

	if (!FurnitureActorPtr || !FurnitureActorPtr->IsValid())
	{
		bIsMovingFurnitureFromFloorPlan2D = false;
		return;
	}

	AFurniture* FurnitureActor = FurnitureActorPtr->Get();

	SyncFurnitureActorFromFloorPlan2D(Furniture);

	if (bIsMovingFurnitureFromFloorPlan2D)
	{
		if (UInteriorPlacementSubsystem* PS = GetPlacementSubsystem())
		{
			PS->FinalizeGizmoMove(FurnitureActor);
		}
	}

	bIsMovingFurnitureFromFloorPlan2D = false;

	if (FurnitureActor)
	{
		FurnitureActor->SetSelected(true);
	}
}

void AInteRealPlayerController::BindFloorPlan2DEvents()
{
	AInteRealHUD* InteRealHUD = GetInteRealHUD();
	if (!InteRealHUD)
	{
		return;
	}

	UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = InteRealHUD->GetFloorPlan2DWidget();
	if (!FloorPlan2DWidget)
	{
		return;
	}

	FloorPlan2DWidget->OnFurniturePlacementRequested2D.AddUniqueDynamic(
		this,
		&AInteRealPlayerController::HandleFloorPlan2DFurniturePlacementRequested
	);

	FloorPlan2DWidget->OnFurniturePreviewMoved2D.AddUniqueDynamic(
		this,
		&AInteRealPlayerController::HandleFloorPlan2DFurniturePreviewMoved
	);

	FloorPlan2DWidget->OnPlacedFurnitureSelected2D.AddUniqueDynamic(
		this,
		&AInteRealPlayerController::HandleFloorPlan2DPlacedFurnitureSelected
	);

	FloorPlan2DWidget->OnPlacedFurnitureMoved2D.AddUniqueDynamic(
		this,
		&AInteRealPlayerController::HandleFloorPlan2DPlacedFurnitureMoved
	);

	FloorPlan2DWidget->OnPlacedFurnitureMoveEnded2D.AddUniqueDynamic(
		this,
		&AInteRealPlayerController::HandleFloorPlan2DPlacedFurnitureMoveEnded
	);

	FloorPlan2DWidget->OnPlacedFurnitureDeleted2D.AddUniqueDynamic(
		this,
		&AInteRealPlayerController::HandleFloorPlan2DPlacedFurnitureDeleted
	);

	FloorPlan2DWidget->OnPlacedFurnituresCleared2D.AddUniqueDynamic(
		this,
		&AInteRealPlayerController::HandleFloorPlan2DPlacedFurnituresCleared
	);
}

void AInteRealPlayerController::RegisterFloorPlan2DFurnitureActor(
	const FInteReal2DPlacedFurniture& Furniture2D,
	AFurniture* FurnitureActor
)
{
	if (!Furniture2D.InstanceGuid.IsValid() || !IsValid(FurnitureActor))
	{
		return;
	}

	FloorPlan2DFurnitureActors.Add(Furniture2D.InstanceGuid, FurnitureActor);
}

bool AInteRealPlayerController::FindFloorPlan2DGuidForFurniture(
	const AFurniture* FurnitureActor,
	FGuid& OutInstanceGuid
) const
{
	OutInstanceGuid.Invalidate();

	if (!IsValid(FurnitureActor))
	{
		return false;
	}

	for (const TPair<FGuid, TWeakObjectPtr<AFurniture>>& Pair : FloorPlan2DFurnitureActors)
	{
		if (Pair.Value.Get() == FurnitureActor)
		{
			OutInstanceGuid = Pair.Key;
			return true;
		}
	}

	return false;
}

void AInteRealPlayerController::SyncFloorPlan2DFromFurniture(AFurniture* FurnitureActor)
{
    if (!IsValid(FurnitureActor) || bIsSyncingFurniture3DFrom2D)
    {
        return;
    }

    FGuid InstanceGuid;
    if (!FindFloorPlan2DGuidForFurniture(FurnitureActor, InstanceGuid))
    {
        return;
    }

    AInteRealHUD* InteRealHUD = GetInteRealHUD();
    if (!InteRealHUD)
    {
        return;
    }

    UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = InteRealHUD->GetFloorPlan2DWidget();
    if (!FloorPlan2DWidget)
    {
        return;
    }

    const FVector WorldLocation = FurnitureActor->GetActorLocation();
    const float WorldYaw = FurnitureActor->GetActorRotation().Yaw;

    bIsSyncingFloorPlan2DFrom3D = true;

    FloorPlan2DWidget->UpdatePlacedFurnitureByGuid(
        InstanceGuid,
        FVector2D(WorldLocation.X, WorldLocation.Y),
        WorldYaw
    );

    bIsSyncingFloorPlan2DFrom3D = false;
}

void AInteRealPlayerController::SyncFurnitureActorFromFloorPlan2D(
    const FInteReal2DPlacedFurniture& Furniture2D
)
{
    if (bIsSyncingFloorPlan2DFrom3D || !Furniture2D.InstanceGuid.IsValid())
    {
        return;
    }

    TWeakObjectPtr<AFurniture>* FurnitureActorPtr =
        FloorPlan2DFurnitureActors.Find(Furniture2D.InstanceGuid);

    if (!FurnitureActorPtr || !FurnitureActorPtr->IsValid())
    {
        return;
    }

    AFurniture* FurnitureActor = FurnitureActorPtr->Get();
    if (!IsValid(FurnitureActor))
    {
        return;
    }

    bIsSyncingFurniture3DFrom2D = true;

    const FVector CurrentLocation = FurnitureActor->GetActorLocation();
    const FVector NewLocation(
        Furniture2D.CenterDocumentPosition.X,
        Furniture2D.CenterDocumentPosition.Y,
        CurrentLocation.Z
    );

    FRotator NewRotation = FurnitureActor->GetActorRotation();
    NewRotation.Yaw = Furniture2D.RotationDegrees;

    FurnitureActor->SetActorLocation(NewLocation);
    FurnitureActor->SetActorRotation(NewRotation);

    bIsSyncingFurniture3DFrom2D = false;
}

bool AInteRealPlayerController::RemoveFloorPlan2DForFurniture(AFurniture* FurnitureActor)
{
	if (!IsValid(FurnitureActor))
	{
		return false;
	}

	FGuid InstanceGuid;
	if (!FindFloorPlan2DGuidForFurniture(FurnitureActor, InstanceGuid))
	{
		return false;
	}

	if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
	{
		if (UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = InteRealHUD->GetFloorPlan2DWidget())
		{
			const bool bRemoved = FloorPlan2DWidget->RemovePlacedFurnitureByGuid(InstanceGuid);
			if (bRemoved)
			{
				return true;
			}
		}
	}

	FloorPlan2DFurnitureActors.Remove(InstanceGuid);
	return false;
}

void AInteRealPlayerController::HandleFloorPlan2DPlacedFurnitureDeleted(
    int32 FurnitureIndex,
    FGuid InstanceGuid
)
{
	if (bIsDeletingFurnitureFrom3D)
	{
		FloorPlan2DFurnitureActors.Remove(InstanceGuid);
		return;
	}

	TWeakObjectPtr<AFurniture>* FurnitureActorPtr = FloorPlan2DFurnitureActors.Find(InstanceGuid);
	if (!FurnitureActorPtr || !FurnitureActorPtr->IsValid())
	{
		FloorPlan2DFurnitureActors.Remove(InstanceGuid);
		return;
	}

	AFurniture* FurnitureActor = FurnitureActorPtr->Get();

	FloorPlan2DFurnitureActors.Remove(InstanceGuid);

	bIsDeletingFurnitureFromFloorPlan2D = true;
	DeleteFurnitureActor(FurnitureActor);
	bIsDeletingFurnitureFromFloorPlan2D = false;
}

void AInteRealPlayerController::HandleFloorPlan2DPlacedFurnituresCleared()
{
	if (bIsDeletingFurnitureFrom3D)
	{
		FloorPlan2DFurnitureActors.Empty();
		return;
	}

	TArray<TWeakObjectPtr<AFurniture>> FurnitureActors;
	FloorPlan2DFurnitureActors.GenerateValueArray(FurnitureActors);
	FloorPlan2DFurnitureActors.Empty();

	bIsDeletingFurnitureFromFloorPlan2D = true;

	for (const TWeakObjectPtr<AFurniture>& FurnitureActorPtr : FurnitureActors)
	{
		if (FurnitureActorPtr.IsValid())
		{
			DeleteFurnitureActor(FurnitureActorPtr.Get());
		}
	}

	bIsDeletingFurnitureFromFloorPlan2D = false;
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

bool AInteRealPlayerController::RegisterLastAddedFloorPlan2DFurnitureActor(
	UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget,
	AFurniture* FurnitureActor
)
{
	if (!FloorPlan2DWidget || !IsValid(FurnitureActor))
	{
		return false;
	}

	const int32 FurnitureIndex = FloorPlan2DWidget->SelectedFurnitureIndex;
	if (!FloorPlan2DWidget->PlacedFurnitures2D.IsValidIndex(FurnitureIndex))
	{
		return false;
	}

	RegisterFloorPlan2DFurnitureActor(
		FloorPlan2DWidget->PlacedFurnitures2D[FurnitureIndex],
		FurnitureActor
	);

	return true;
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
