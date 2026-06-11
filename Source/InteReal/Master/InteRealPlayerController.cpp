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
#include "Materials/MaterialInterface.h"

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
		}
	}

	// DefaultPawn 초기 설정
	if (APawn* P = GetPawn())
	{
		P->SetActorHiddenInGame(true);
	}

	ApplyCurrentControlMode();

	// View mode 기본 상태 동기화
	if (CachedViewModeManager)
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

		// 새 가구 배치 중 OR 기존 가구 이동 중일 때 툴팁 표시
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

	if (bIsDraggingGizmo && SelectedFurniture)
	{
		FVector WorldOrigin, WorldDir;
		DeprojectMousePositionToWorld(WorldOrigin, WorldDir);
		const FVector Center = SelectedFurniture->GetActorLocation();

		// 회전 
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

		//  X / Y 이동 (그리드 스냅 + 벽 충돌)
		if ((CurrentDraggingAxis == TEXT("MoveX") || CurrentDraggingAxis == TEXT("MoveY")) && PlacementManager)
		{
			FPlane GroundPlane(DragStartFurnitureLocation, FVector::UpVector);
			FVector CursorOnGround = FMath::LinePlaneIntersection(WorldOrigin, WorldOrigin + WorldDir * 100000.f, GroundPlane);
			PlacementManager->UpdateGizmoMoveLocation(CursorOnGround, SelectedFurniture, CurrentDraggingAxis);
			return;
		}

		// Z 이동 (수직) 
		if (CurrentDraggingAxis == TEXT("MoveZ"))
		{
			// 카메라 방향에 수직인 평면에 투영해 Z값만 추출
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

void AInteRealPlayerController::HandleFurnitureSpawn(FFurnitureDataRow FurnitureData)
{
	StartFurniturePlacement(FurnitureData);
}

void AInteRealPlayerController::HandleWallMaterialChanged(UMaterialInterface* NewMaterial)
{
	ApplyMaterialToSelectedWall(NewMaterial);
}

void AInteRealPlayerController::SelectWall(UDynamicMeshComponent* WallComponent)
{
	if (SelectedWallComponent == WallComponent)
	{
		return;
	}

	DeselectFurniture();
	SelectedWallComponent = WallComponent;
}

void AInteRealPlayerController::DeselectWall()
{
	SelectedWallComponent = nullptr;
}

void AInteRealPlayerController::ApplyMaterialToSelectedWall(UMaterialInterface* NewMaterial)
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	if (!SelectedWallComponent) return;
	if (!NewMaterial) return;

	SelectedWallComponent->SetMaterial(0, NewMaterial);
}

void AInteRealPlayerController::ApplyCurrentControlMode()
{
	UpdateMappingContexts();
	UpdateInputModeForCurrentControlMode();

	// View 모드 전환 시 선택/프리뷰 상태 정리
	if (CurrentControlMode != EInteRealControlMode::Edit)
	{
		DeselectFurniture();
		DeselectWall();
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

		// 항상 공통 입력 먼저 등록
		if (IMC_Common)
		{
			Subsystem->AddMappingContext(IMC_Common, 0);
		}

		// 모드별 입력 추가 등록
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
		// 💡 [픽셀 스트리밍 대응] LockOnCapture로 변경하여 브라우저 클릭 시 포커스 동기화
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockOnCapture);
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
		// 💡 [픽셀 스트리밍 대응] View 모드에서도 클릭 가능하도록 동일하게 설정
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
		// 드래그 중인 가구를 트레이스에서 제외 → 가구 콜리전을 건드리지 않고 바닥 좌표만 획득
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
		DeselectWall();
		return;
	}

	UPrimitiveComponent* HitComp = LastCursorHit.GetComponent();
	if (HitComp && SelectedFurniture)
	{
		for (const FName& Tag : HitComp->ComponentTags)
		{
			const FString TagStr = Tag.ToString();

			// ── 회전 ─────────────────────────────────────────────
			if (TagStr.StartsWith(TEXT("Rotate")) || TagStr == TEXT("RotationRing"))
			{
				CurrentDraggingAxis  = TagStr;
				bIsDraggingGizmo     = true;
				DragStartFurnitureRot = SelectedFurniture->GetActorRotation();

				FVector WorldOrigin, WorldDir;
				DeprojectMousePositionToWorld(WorldOrigin, WorldDir);

				// 축별 투영 평면 선택
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

			// ── 이동 ─────────────────────────────────────────────
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
				// 선택된 가구를 다시 클릭 → 이동 드래그 시작
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
	
	if (UDynamicMeshComponent* HitWallComp = Cast<UDynamicMeshComponent>(HitComp))
	{
		if (HitWallComp->ComponentHasTag(TEXT("EditableWall")))
		{
			DeselectFurniture();
			SelectWall(HitWallComp);
			return;
		}
	}

	DeselectFurniture();
	DeselectWall();
}

void AInteRealPlayerController::OnPlaceReleasedKey()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;

	bIsDraggingGizmo = false;

	// 화살표 이동 확정 (그리드 스냅)
	if ((CurrentDraggingAxis == TEXT("MoveX") || CurrentDraggingAxis == TEXT("MoveY")) && SelectedFurniture && PlacementManager)
		PlacementManager->FinalizeGizmoMove(SelectedFurniture);

	CurrentDraggingAxis = TEXT("");

	// 가구 바디 드래그 확정 — 실제로 움직인 경우에만 스냅, 그냥 클릭이면 원위치 복원
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
		// 커서가 선택된 가구 위에 있으면 삭제, 아니면 선택 해제
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
		FRotator Rot = SelectedFurniture->GetActorRotation();
		Rot.Yaw = FRotator::NormalizeAxis(Rot.Yaw + 15.0f);
		SelectedFurniture->SetActorRotation(Rot);
	}
}

void AInteRealPlayerController::SelectFurniture(AFurniture* Furniture)
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	if (SelectedFurniture == Furniture) return;

	DeselectWall();
	DeselectFurniture();
	SelectedFurniture = Furniture;

	if (SelectedFurniture)
	{
		SelectedFurniture->SetSelected(true);
	}
}

void AInteRealPlayerController::DeselectFurniture()
{
	if (SelectedFurniture)
	{
		SelectedFurniture->SetSelected(false);
		SelectedFurniture = nullptr;
	}
	bIsDraggingGizmo = false;
	bIsMovingFurniture = false;
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
		SetControlMode(EInteRealControlMode::View);
	}
	else
	{
		SetControlMode(EInteRealControlMode::Edit);
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
	UE_LOG(LogTemp, Log, TEXT("Undo requested"));
}

void AInteRealPlayerController::OnRedoKey()
{
	UE_LOG(LogTemp, Log, TEXT("Redo requested"));
}

void AInteRealPlayerController::OnCopyKey()
{
	UE_LOG(LogTemp, Log, TEXT("Copy requested"));
}

void AInteRealPlayerController::OnPasteKey()
{
	UE_LOG(LogTemp, Log, TEXT("Paste requested"));
}

void AInteRealPlayerController::OnDuplicateKey()
{
	UE_LOG(LogTemp, Log, TEXT("Duplicate requested"));
}

void AInteRealPlayerController::OnSaveKey()
{
	UE_LOG(LogTemp, Log, TEXT("Save requested"));
}

