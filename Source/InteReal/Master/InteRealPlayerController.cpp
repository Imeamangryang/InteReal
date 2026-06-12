#include "InteRealPlayerController.h"
#include "InteRealHUD.h"
#include "InteReal/EditMode/Managers/InteriorPlacementManager.h"
#include "InteReal/ViewMode/ViewModeManager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/BoxComponent.h"
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

	FindPlacementManager();
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
		}

		return;
	}

	UpdateCursorHit();

	if (AInteRealHUD* InteRealHUD = GetInteRealHUD())
	{
		const bool bHasPreview = PlacementManager && PlacementManager->HasActivePreview();
		const EPlacementInvalidReason InvalidReason =
			PlacementManager ? PlacementManager->InvalidReason : EPlacementInvalidReason::None;

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
		AFurniture* GuideFurniture = bIsMovingFurniture ? SelectedFurniture.Get() : (PlacementManager ? PlacementManager->GetPreviewFurniture() : nullptr);
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

	if (!bIsDraggingGizmo)
	{
		UpdateGizmoHover();
	}

	if (bIsDraggingGizmo && SelectedFurniture)
	{
		FVector WorldOrigin, WorldDir;
		DeprojectMousePositionToWorld(WorldOrigin, WorldDir);
		const FVector Center = SelectedFurniture->GetActorLocation();

		// �전 
		if (CurrentDraggingAxis.StartsWith(TEXT("Rotate")) || CurrentDraggingAxis == TEXT("RotationRing"))
		{
			FVector PlaneNormal = FVector::UpVector;
			if (CurrentDraggingAxis == TEXT("RotatePitch")) PlaneNormal = FVector::RightVector;
			else if (CurrentDraggingAxis == TEXT("RotateRoll")) PlaneNormal = FVector::ForwardVector;

			const FVector Hit = FMath::LinePlaneIntersection(WorldOrigin, WorldOrigin + WorldDir * 100000.f,
				FPlane(Center, PlaneNormal));

			float CurrentAngle;
			if (CurrentDraggingAxis == TEXT("RotatePitch"))
				CurrentAngle = FMath::RadiansToDegrees(FMath::Atan2(Hit.Z - Center.Z, Hit.X - Center.X));
			else if (CurrentDraggingAxis == TEXT("RotateRoll"))
				CurrentAngle = FMath::RadiansToDegrees(FMath::Atan2(Hit.Z - Center.Z, Hit.Y - Center.Y));
			else
				CurrentAngle = FMath::RadiansToDegrees(FMath::Atan2(Hit.Y - Center.Y, Hit.X - Center.X));

			float DeltaAngle = FRotator::NormalizeAxis(CurrentAngle - DragStartAngleDeg) * GizmoRotationSensitivity;
			if (IsInputKeyDown(EKeys::LeftControl))
				DeltaAngle = FMath::GridSnap(DeltaAngle, 15.0f);

			FRotator NewRot = DragStartFurnitureRot;
			if      (CurrentDraggingAxis == TEXT("RotatePitch")) NewRot.Pitch = FRotator::NormalizeAxis(NewRot.Pitch + DeltaAngle);
			else if (CurrentDraggingAxis == TEXT("RotateRoll"))  NewRot.Roll  = FRotator::NormalizeAxis(NewRot.Roll  + DeltaAngle);
			else                                                  NewRot.Yaw   = FRotator::NormalizeAxis(NewRot.Yaw   + DeltaAngle);

			SelectedFurniture->SetActorRotation(NewRot);
			return;
		}

		//  X / Y �동 (그리�냅 + �충
		if ((CurrentDraggingAxis == TEXT("MoveX") || CurrentDraggingAxis == TEXT("MoveY")) && PlacementManager)
		{
			FPlane GroundPlane(DragStartFurnitureLocation, FVector::UpVector);
			FVector CursorOnGround = FMath::LinePlaneIntersection(WorldOrigin, WorldOrigin + WorldDir * 100000.f, GroundPlane);
			PlacementManager->UpdateGizmoMoveLocation(CursorOnGround, SelectedFurniture, CurrentDraggingAxis);
			return;
		}

		// Z �동 (�직) 
		if (CurrentDraggingAxis == TEXT("MoveZ"))
		{
			// 카메방향�직�면�영Z값만 추출
			FVector CamFwd = FVector(WorldDir.X, WorldDir.Y, 0.f).GetSafeNormal();
			FVector CursorOnPlane = FMath::LinePlaneIntersection(WorldOrigin, WorldOrigin + WorldDir * 100000.f,
				FPlane(DragStartFurnitureLocation, CamFwd));
			FVector NewLoc = DragStartFurnitureLocation;
			NewLoc.Z = CursorOnPlane.Z;
			SelectedFurniture->SetActorLocation(NewLoc);
			return;
		}
	}

	if (bIsMovingFurniture && SelectedFurniture && PlacementManager)
	{
		PlacementManager->UpdateGizmoMoveFree(CurrentCursorWorldLoc + MoveDragOffset, SelectedFurniture);
		return;
	}

	if (!PlacementManager || !bIsHitting) return;
	if (!PlacementManager->HasActivePreview()) return;

	PlacementManager->UpdatePreviewLocation(CurrentCursorWorldLoc);
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
		EIC->BindAction(IA_Rotate, ETriggerEvent::Started, this, &AInteRealPlayerController::OnRotatePreviewKey);
	}

	if (IA_Rotate15)
	{
		EIC->BindAction(IA_Rotate15, ETriggerEvent::Started, this, &AInteRealPlayerController::OnRotate15Key);
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

	if (PlacementManager)
	{
		PlacementManager->RecordUndoSnapshot();
	}

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
		if (PlacementManager && PlacementManager->HasActivePreview())
		{
			PlacementManager->CancelPreview();
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

void AInteRealPlayerController::FindPlacementManager()
{
	if (PlacementManager) return;

	for (TActorIterator<AInteriorPlacementManager> It(GetWorld()); It; ++It)
	{
		PlacementManager = *It;
		break;
	}
}

void AInteRealPlayerController::UpdateCursorHit()
{
	if (bIsMovingFurniture && SelectedFurniture)
	{
		// �래�중가구� �레�스�서 �외 가�콜리�건드리� �고 바닥 좌표맍득
		FVector WorldOrigin, WorldDir;
		DeprojectMousePositionToWorld(WorldOrigin, WorldDir);

		FCollisionQueryParams Params;
		Params.AddIgnoredActor(SelectedFurniture);

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

	bIsHitting = GetHitResultUnderCursorByChannel(
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		true,
		LastCursorHit
	);

	if (bIsHitting)
	{
		CurrentCursorWorldLoc = LastCursorHit.Location;
	}

	// 기즈몰선 �위 �정
	if (SpawnedGizmo)
	{
		FVector WorldOrigin, WorldDir;
		DeprojectMousePositionToWorld(WorldOrigin, WorldDir);

		TArray<FHitResult> Hits;
		FCollisionQueryParams Params(NAME_None, true);
		GetWorld()->LineTraceMultiByChannel(
			Hits,
			WorldOrigin,
			WorldOrigin + WorldDir * 100000.f,
			ECC_Visibility,
			Params
		);

		for (const FHitResult& Hit : Hits)
		{
			if (Hit.GetActor() == SpawnedGizmo)
			{
				LastCursorHit = Hit;
				bIsHitting = true;
				CurrentCursorWorldLoc = Hit.Location;
				break;
			}
		}
	}
}

void AInteRealPlayerController::ToggleGrid()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	if (!PlacementManager) return;

	bGridVisible = !bGridVisible;
	PlacementManager->SetGridVisible(bGridVisible);
}

void AInteRealPlayerController::OnPlaceKey()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	if (!PlacementManager) return;

	if (PlacementManager->HasActivePreview())
	{
		if (bIsHitting)
		{
			PlacementManager->ConfirmFurniture();
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
		for (const FName& Tag : HitComp->ComponentTags)
		{
			const FString TagStr = Tag.ToString();

			// �� �전 ���������������������������������������������
			if (TagStr.StartsWith(TEXT("Rotate")) || TagStr == TEXT("RotationRing"))
			{
				if (PlacementManager)
				{
					PlacementManager->RecordUndoSnapshot();
				}
				
				CurrentDraggingAxis  = TagStr;
				bIsDraggingGizmo     = true;
				DragStartFurnitureRot = SelectedFurniture->GetActorRotation();

				FVector WorldOrigin, WorldDir;
				DeprojectMousePositionToWorld(WorldOrigin, WorldDir);

				// 축별 �영 �면 �택
				FVector PlaneNormal = FVector::UpVector;
				if (TagStr == TEXT("RotatePitch")) PlaneNormal = FVector::RightVector;
				else if (TagStr == TEXT("RotateRoll")) PlaneNormal = FVector::ForwardVector;

				const FPlane Plane(SelectedFurniture->GetActorLocation(), PlaneNormal);
				const FVector Hit    = FMath::LinePlaneIntersection(WorldOrigin, WorldOrigin + WorldDir * 100000.f, Plane);
				const FVector Center = SelectedFurniture->GetActorLocation();

				if (TagStr == TEXT("RotatePitch"))
					DragStartAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Hit.Z - Center.Z, Hit.X - Center.X));
				else if (TagStr == TEXT("RotateRoll"))
					DragStartAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Hit.Z - Center.Z, Hit.Y - Center.Y));
				else
					DragStartAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Hit.Y - Center.Y, Hit.X - Center.X));
				return;
			}

			// �� �동 ���������������������������������������������
			if (TagStr.StartsWith(TEXT("Move")))
			{
				CurrentDraggingAxis        = TagStr;
				bIsDraggingGizmo           = true;
				DragStartFurnitureLocation = SelectedFurniture->GetActorLocation();

				if (PlacementManager && TagStr != TEXT("MoveZ"))
					PlacementManager->BeginGizmoMove(SelectedFurniture);
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

				if (PlacementManager)
				{
					PlacementManager->BeginGizmoMove(SelectedFurniture);
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

	bIsDraggingGizmo = false;

	// �살�동 �정 (그리�냅)
	if ((CurrentDraggingAxis == TEXT("MoveX") || CurrentDraggingAxis == TEXT("MoveY")) && SelectedFurniture && PlacementManager)
		PlacementManager->FinalizeGizmoMove(SelectedFurniture);

	if (!CurrentDraggingAxis.IsEmpty())
	{
		SetGizmoAxisOpacity(CurrentDraggingAxis, GizmoDefaultOpacity);
	}
	CurrentDraggingAxis = TEXT("");

	// 가�바�래귕정 �제례직인 경우�만 �냅, 그냥 �릭�면 �위�복
	if (bIsMovingFurniture && SelectedFurniture && PlacementManager)
	{
		bIsMovingFurniture = false;
		const float MoveDist = FVector::Dist2D(SelectedFurniture->GetActorLocation(), DragStartFurnitureLocation);
		if (MoveDist > 2.0f)
			PlacementManager->FinalizeGizmoMove(SelectedFurniture);
		else
			PlacementManager->AbortGizmoMove(SelectedFurniture);
	}
}

void AInteRealPlayerController::OnRemoveKey()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	if (!PlacementManager) return;

	if (PlacementManager->HasActivePreview())
	{
		PlacementManager->CancelPreview();
		return;
	}

	if (SelectedFurniture)
	{
		// 커서가 �택가굄에 �으멠�, �니멠택 �제
		if (bIsHitting && Cast<AFurniture>(LastCursorHit.GetActor()) == SelectedFurniture)
		{
			AFurniture* ToRemove = SelectedFurniture;
			DeselectFurniture();
			PlacementManager->RemoveFurniture(ToRemove);
		}
		else
		{
			DeselectFurniture();
		}
	}
}

void AInteRealPlayerController::OnRotatePreviewKey()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;

	if (PlacementManager && PlacementManager->HasActivePreview())
	{
		PlacementManager->RotatePreview(90.0f);
		return;
	}

	if (SelectedFurniture)
	{
		if (PlacementManager)
		{
			PlacementManager->RecordUndoSnapshot();
		}
		
		FRotator Rot = SelectedFurniture->GetActorRotation();
		Rot.Yaw = FRotator::NormalizeAxis(Rot.Yaw + 90.0f);
		SelectedFurniture->SetActorRotation(Rot);
	}
}

void AInteRealPlayerController::OnRotate15Key()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;

	if (PlacementManager && PlacementManager->HasActivePreview())
	{
		PlacementManager->RotatePreview(15.0f);
		return;
	}

	if (SelectedFurniture)
	{
		if (PlacementManager)
		{
			PlacementManager->RecordUndoSnapshot();
		}
		
		FRotator Rot = SelectedFurniture->GetActorRotation();
		Rot.Yaw = FRotator::NormalizeAxis(Rot.Yaw + 15.0f);
		SelectedFurniture->SetActorRotation(Rot);
	}
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

			// 바운박스 중심기즈�배치 (가�pivot X)
			FVector GizmoSpawnLocation = SelectedFurniture->GetActorLocation();
			GizmoSpawnLocation.Z = SelectedFurniture->GetComponentsBoundingBox(true).GetCenter().Z;

			SpawnedGizmo = GetWorld()->SpawnActor<AActor>(
				GizmoActorClass,
				SelectedFurniture->GetActorLocation(),
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

				InitGizmoAxisMaterials();
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
	bIsDraggingGizmo = false;
	bIsMovingFurniture = false;

	GizmoAxisMaterials.Empty();
	HoveredGizmoAxis.Empty();
}

void AInteRealPlayerController::InitGizmoAxisMaterials()
{
	GizmoAxisMaterials.Empty();
	HoveredGizmoAxis.Empty();

	if (!SpawnedGizmo) return;

	TArray<UMeshComponent*> Meshes;
	SpawnedGizmo->GetComponents<UMeshComponent>(Meshes);

	for (UMeshComponent* Mesh : Meshes)
	{
		FString AxisTag;
		for (const FName& Tag : Mesh->ComponentTags)
		{
			const FString TagStr = Tag.ToString();
			if (TagStr.StartsWith(TEXT("Move")) || TagStr.StartsWith(TEXT("Rotate")) || TagStr == TEXT("RotationRing"))
			{
				AxisTag = TagStr;
				break;
			}
		}
		if (AxisTag.IsEmpty()) continue;

		TArray<TObjectPtr<UMaterialInstanceDynamic>>& DMIs = GizmoAxisMaterials.FindOrAdd(AxisTag);
		for (int32 i = 0; i < Mesh->GetNumMaterials(); i++)
		{
			if (UMaterialInstanceDynamic* DMI = Mesh->CreateAndSetMaterialInstanceDynamic(i))
			{
				DMI->SetScalarParameterValue(GizmoOpacityParamName, GizmoDefaultOpacity);
				DMIs.Add(DMI);
			}
		}
	}
}

void AInteRealPlayerController::SetGizmoAxisOpacity(const FString& Axis, float Opacity)
{
	if (const TArray<TObjectPtr<UMaterialInstanceDynamic>>* DMIs = GizmoAxisMaterials.Find(Axis))
	{
		for (UMaterialInstanceDynamic* DMI : *DMIs)
		{
			if (DMI)
			{
				DMI->SetScalarParameterValue(GizmoOpacityParamName, Opacity);
			}
		}
	}
}

void AInteRealPlayerController::UpdateGizmoHover()
{
	if (!SpawnedGizmo || GizmoAxisMaterials.Num() == 0) return;

	FString NewHoveredAxis;
	if (bIsHitting)
	{
		if (UPrimitiveComponent* HitComp = LastCursorHit.GetComponent())
		{
			if (HitComp->GetOwner() == SpawnedGizmo)
			{
				for (const FName& Tag : HitComp->ComponentTags)
				{
					const FString TagStr = Tag.ToString();
					if (GizmoAxisMaterials.Contains(TagStr))
					{
						NewHoveredAxis = TagStr;
						break;
					}
				}
			}
		}
	}

	if (NewHoveredAxis == HoveredGizmoAxis) return;

	if (!HoveredGizmoAxis.IsEmpty())
	{
		SetGizmoAxisOpacity(HoveredGizmoAxis, GizmoDefaultOpacity);
	}
	if (!NewHoveredAxis.IsEmpty())
	{
		SetGizmoAxisOpacity(NewHoveredAxis, GizmoHighlightOpacity);
	}
	HoveredGizmoAxis = NewHoveredAxis;
}

void AInteRealPlayerController::ReceiveWebCommand(const FString& JsonString)
{
	FindPlacementManager();
	if (!PlacementManager) return;

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[EditMode] Invalid web command JSON: %s"), *JsonString);
		return;
	}

	FString Action;
	if (!Root->TryGetStringField(TEXT("action"), Action))
	{
		return;
	}

	if (Action == TEXT("SELECT_KIND"))
	{
		int32 ID = 0;
		if (Root->TryGetNumberField(TEXT("furnitureId"), ID))
		{
			const FFurnitureDataRow* Row = PlacementManager->FindFurnitureRowByID(ID);
			if (Row)
			{
				SetControlMode(EInteRealControlMode::Edit);
				StartFurniturePlacement(*Row);
			}
		}
	}
	else if (Action == TEXT("ROTATE"))
	{
		if (PlacementManager->HasActivePreview())
		{
			SetControlMode(EInteRealControlMode::Edit);
			PlacementManager->RotatePreview(90.0f);
		}
	}
	else if (Action == TEXT("CONFIRM"))
	{
		if (PlacementManager->HasActivePreview())
		{
			SetControlMode(EInteRealControlMode::Edit);
			PlacementManager->ConfirmFurniture();
		}
	}
	else if (Action == TEXT("CANCEL"))
	{
		if (PlacementManager->HasActivePreview())
		{
			PlacementManager->CancelPreview();
		}
	}
	else if (Action == TEXT("LOAD"))
	{
		FString Payload;
		if (Root->TryGetStringField(TEXT("data"), Payload))
		{
			PlacementManager->ImportPlacedFurnituresJson(Payload);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[EditMode] Unknown web action: %s"), *Action);
	}
}

void AInteRealPlayerController::StartFurniturePlacement(const FFurnitureDataRow& FurnitureRow)
{
	FindPlacementManager();
	if (!PlacementManager) return;

	SetControlMode(EInteRealControlMode::Edit);
	DeselectFurniture();

	if (PlacementManager->HasActivePreview())
	{
		PlacementManager->CancelPreview();
	}

	const FVector SpawnLoc = bIsHitting ? CurrentCursorWorldLoc : FVector::ZeroVector;
	PlacementManager->CreatePreviewFurnitureFromRow(SpawnLoc, FRotator::ZeroRotator, FurnitureRow);
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
	TSubclassOf<UHarnessCaptureMinimapWidget> InWidgetClass)
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
	UE_LOG(LogTemp, Log, TEXT("FocusSelection requested"));
}

void AInteRealPlayerController::OnMoveKey(const FInputActionValue& Value)
{
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
	if (!PlacementManager) return;

	DeselectFurniture();
	DeselectSurface();

	if (PlacementManager->HasActivePreview())
	{
		PlacementManager->CancelPreview();
	}

	PlacementManager->Undo();
}

void AInteRealPlayerController::OnRedoKey()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	if (!PlacementManager) return;

	DeselectFurniture();
	DeselectSurface();

	if (PlacementManager->HasActivePreview())
	{
		PlacementManager->CancelPreview();
	}

	PlacementManager->Redo();
}

void AInteRealPlayerController::OnCopyKey()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	if (!PlacementManager) return;
	if (!SelectedFurniture) return;

	const FFurnitureDataRow* Row = PlacementManager->FindFurnitureRowByID(SelectedFurniture->FurnitureID);
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
	if (!PlacementManager) return;
	if (!bHasCopiedFurniture) return;
	if (!bIsHitting) return;

	if (PlacementManager->HasActivePreview())
	{
		PlacementManager->CancelPreview();
	}

	DeselectFurniture();
	DeselectSurface();

	PlacementManager->CreatePreviewFurnitureFromRow(
		CurrentCursorWorldLoc,
		CopiedFurnitureRotation,
		CopiedFurnitureRow
	);

	PlacementManager->ConfirmFurniture();
}

void AInteRealPlayerController::OnDuplicateKey()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	if (!PlacementManager) return;
	if (!SelectedFurniture) return;

	OnCopyKey();

	if (!bHasCopiedFurniture)
	{
		return;
	}

	FVector PasteLocation = bIsHitting
		? CurrentCursorWorldLoc
		: SelectedFurniture->GetActorLocation() + FVector(100.0f, 100.0f, 0.0f);

	if (PlacementManager->HasActivePreview())
	{
		PlacementManager->CancelPreview();
	}

	PlacementManager->CreatePreviewFurnitureFromRow(
		PasteLocation,
		CopiedFurnitureRotation,
		CopiedFurnitureRow
	);

	PlacementManager->ConfirmFurniture();
}

void AInteRealPlayerController::OnSaveKey()
{
	UE_LOG(LogTemp, Log, TEXT("Save requested"));
}

