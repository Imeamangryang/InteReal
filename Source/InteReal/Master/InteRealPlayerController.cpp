#include "InteRealPlayerController.h"
#include "InteReal/EditMode/Managers/InteriorPlacementManager.h"
#include "InteReal/EditMode/Gizmo/FurnitureGizmoComponent.h"
#include "InteReal/ViewMode/ViewModeManager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Components/Overlay.h"
#include "Public/HarnessCaptureMinimapWidget.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "InteReal/SubSystems/InteRealUISubSystem.h"
#include "Engine/GameInstance.h"

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
		}
	}

	// ===== Edit UI =====
	if (PlacementTabWidget)
	{
		PlacementTabInstance = CreateWidget<UUserWidget>(this, PlacementTabWidget);
		if (PlacementTabInstance)
		{
			PlacementTabInstance->AddToViewport();
		}
	}

	if (TooltipWidgetClass)
	{
		TooltipInstance = CreateWidget<UPlacementTooltipWidget>(this, TooltipWidgetClass);
		if (TooltipInstance)
		{
			TooltipInstance->AddToViewport();
			TooltipInstance->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (RotationGuideWidgetClass)
	{
		RotationGuideInstance = CreateWidget<URotationGuideWidget>(this, RotationGuideWidgetClass);
		if (RotationGuideInstance)
		{
			RotationGuideInstance->AddToViewport();
			RotationGuideInstance->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	// ===== View UI =====
	if (ViewModeWidgetClass)
	{
		ViewModeWidgetInstance = CreateWidget<UUserWidget>(this, ViewModeWidgetClass);
		if (ViewModeWidgetInstance)
		{
			ViewModeWidgetInstance->AddToViewport();
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
		SetViewMode(EHarnessViewMode::TopDown);
	}
}

void AInteRealPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentControlMode != EInteRealControlMode::Edit)
	{
		return;
	}

	UpdateCursorHit();
	UpdateTooltip();

	if (bIsDraggingGizmo && SelectedFurniture)
	{
		FVector WorldOrigin, WorldDir;
		DeprojectMousePositionToWorld(WorldOrigin, WorldDir);

		const FPlane Plane(SelectedFurniture->GetActorLocation(), FVector::UpVector);
		const FVector Hit = FMath::LinePlaneIntersection(
			WorldOrigin,
			WorldOrigin + WorldDir * 100000.f,
			Plane
		);

		const FVector Center = SelectedFurniture->GetActorLocation();

		float CurrentAngle = FMath::RadiansToDegrees(
			FMath::Atan2(Hit.Y - Center.Y, Hit.X - Center.X)
		);

		float DeltaAngle = (CurrentAngle - DragStartAngleDeg) * GizmoRotationSensitivity;

		FRotator NewRot = DragStartFurnitureRot;
		NewRot.Yaw = FRotator::NormalizeAxis(NewRot.Yaw + DeltaAngle);

		if (IsInputKeyDown(EKeys::LeftControl))
		{
			NewRot.Yaw = FMath::GridSnap(NewRot.Yaw, 15.0f);
		}

		SelectedFurniture->SetActorRotation(NewRot);

		if (UFurnitureGizmoComponent* GizmoComp = SelectedFurniture->FindComponentByClass<UFurnitureGizmoComponent>())
		{
			float BoundsMax = SelectedFurniture->GetComponentsBoundingBox().GetExtent().GetMax();
			GizmoComp->UpdateRadialRotationRing(BoundsMax, DeltaAngle);
		}
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

	// ===== Edit bindings =====
	if (IA_Place)
	{
		EIC->BindAction(IA_Place, ETriggerEvent::Started, this, &AInteRealPlayerController::OnPlace);
		EIC->BindAction(IA_Place, ETriggerEvent::Completed, this, &AInteRealPlayerController::OnPlaceReleased);
	}

	if (IA_Remove)
	{
		EIC->BindAction(IA_Remove, ETriggerEvent::Started, this, &AInteRealPlayerController::OnRemove);
	}

	if (IA_Rotate)
	{
		EIC->BindAction(IA_Rotate, ETriggerEvent::Started, this, &AInteRealPlayerController::OnRotatePreview);
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
	if (IA_Move)
	{
		EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AInteRealPlayerController::EnhancedMove);
	}
	if (IA_Look)
	{
		EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AInteRealPlayerController::EnhancedLook);
	}
	if (IA_Zoom)
	{
		EIC->BindAction(IA_Zoom, ETriggerEvent::Triggered, this, &AInteRealPlayerController::EnhancedZoom);
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

void AInteRealPlayerController::ApplyCurrentControlMode()
{
	UpdateMappingContexts();
	UpdateInputModeForCurrentControlMode();
	UpdateModeUIVisibility();
}

void AInteRealPlayerController::UpdateMappingContexts()
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (IMC_EditMode)
		{
			Subsystem->RemoveMappingContext(IMC_EditMode);
		}

		if (ViewModeMappingContext)
		{
			Subsystem->RemoveMappingContext(ViewModeMappingContext);
		}

		if (CurrentControlMode == EInteRealControlMode::Edit)
		{
			if (IMC_EditMode)
			{
				Subsystem->AddMappingContext(IMC_EditMode, 0);
			}
		}
		else
		{
			if (ViewModeMappingContext)
			{
				Subsystem->AddMappingContext(ViewModeMappingContext, 0);
			}
		}
	}
}

void AInteRealPlayerController::UpdateInputModeForCurrentControlMode()
{
	if (CurrentControlMode == EInteRealControlMode::Edit)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);

		if (PlacementTabInstance)
		{
			InputMode.SetWidgetToFocus(PlacementTabInstance->TakeWidget());
		}

		SetInputMode(InputMode);
		bShowMouseCursor = true;
	}
	else
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);

		SetInputMode(InputMode);
		bShowMouseCursor = true;
	}
}

void AInteRealPlayerController::UpdateModeUIVisibility()
{
	const bool bIsEdit = (CurrentControlMode == EInteRealControlMode::Edit);
	const ESlateVisibility EditVisibility = bIsEdit ? ESlateVisibility::Visible : ESlateVisibility::Hidden;
	const ESlateVisibility ViewVisibility = bIsEdit ? ESlateVisibility::Hidden : ESlateVisibility::Visible;

	if (PlacementTabInstance)
	{
		PlacementTabInstance->SetVisibility(EditVisibility);
	}

	if (TooltipInstance)
	{
		TooltipInstance->SetVisibility(ESlateVisibility::Hidden);
	}

	if (RotationGuideInstance)
	{
		RotationGuideInstance->SetVisibility(ESlateVisibility::Hidden);
	}

	if (ViewModeWidgetInstance)
	{
		ViewModeWidgetInstance->SetVisibility(ViewVisibility);
	}

	if (MinimapWidgetInstance)
	{
		if (bIsEdit)
		{
			MinimapWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
		}
		else
		{
			MinimapWidgetInstance->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}
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

void AInteRealPlayerController::OnPlace()
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
		return;
	}

	UPrimitiveComponent* HitComp = LastCursorHit.GetComponent();
	if (HitComp && HitComp->GetFName() == FName(TEXT("RingMeshComp")) && SelectedFurniture)
	{
		bIsDraggingGizmo = true;
		DragStartFurnitureRot = SelectedFurniture->GetActorRotation();

		FVector WorldOrigin, WorldDir;
		DeprojectMousePositionToWorld(WorldOrigin, WorldDir);

		const FPlane Plane(SelectedFurniture->GetActorLocation(), FVector::UpVector);
		const FVector Hit = FMath::LinePlaneIntersection(
			WorldOrigin,
			WorldOrigin + WorldDir * 100000.f,
			Plane
		);

		const FVector Center = SelectedFurniture->GetActorLocation();
		DragStartAngleDeg = FMath::RadiansToDegrees(
			FMath::Atan2(Hit.Y - Center.Y, Hit.X - Center.X)
		);
		return;
	}

	if (AFurniture* HitFurniture = Cast<AFurniture>(LastCursorHit.GetActor()))
	{
		if (HitFurniture->GetPlacementState() == EPlacementState::Placed)
		{
			if (SelectedFurniture == HitFurniture)
			{
				const FFurnitureDataRow* Row = PlacementManager->FindFurnitureRowByID(HitFurniture->FurnitureID);
				if (Row)
				{
					PlacementManager->RemoveFurniture(HitFurniture);
					SelectedFurniture = nullptr;
					bIsDraggingGizmo = false;
					StartFurniturePlacement(*Row);
				}
				return;
			}

			SelectFurniture(HitFurniture);
			return;
		}
	}

	DeselectFurniture();
}

void AInteRealPlayerController::OnPlaceReleased()
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;

	if (bIsDraggingGizmo && SelectedFurniture)
	{
		if (UFurnitureGizmoComponent* GizmoComp = SelectedFurniture->FindComponentByClass<UFurnitureGizmoComponent>())
		{
			FBox Bounds = SelectedFurniture->GetComponentsBoundingBox();
			GizmoComp->SetupFromLocalBounds(Bounds.TransformBy(SelectedFurniture->GetActorTransform().Inverse()));
		}
	}

	bIsDraggingGizmo = false;
}

void AInteRealPlayerController::OnRemove()
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
		DeselectFurniture();
		return;
	}

	if (AFurniture* HitFurniture = Cast<AFurniture>(LastCursorHit.GetActor()))
	{
		PlacementManager->RemoveFurniture(HitFurniture);
	}
}

void AInteRealPlayerController::OnRotatePreview()
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

void AInteRealPlayerController::SelectFurniture(AFurniture* Furniture)
{
	if (CurrentControlMode != EInteRealControlMode::Edit) return;
	if (SelectedFurniture == Furniture) return;

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
}

void AInteRealPlayerController::UpdateTooltip()
{
	if (CurrentControlMode != EInteRealControlMode::Edit)
	{
		if (TooltipInstance)
		{
			TooltipInstance->SetVisibility(ESlateVisibility::Hidden);
		}
		return;
	}

	if (!TooltipInstance || !PlacementManager) return;

	if (!PlacementManager->HasActivePreview() ||
		PlacementManager->InvalidReason == EPlacementInvalidReason::None)
	{
		TooltipInstance->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	TooltipInstance->ShowReason(PlacementManager->InvalidReason);

	float MouseX, MouseY;
	GetMousePosition(MouseX, MouseY);
	TooltipInstance->SetPositionInViewport(FVector2D(MouseX + 16.f, MouseY + 16.f), false);
	TooltipInstance->SetVisibility(ESlateVisibility::HitTestInvisible);
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

	SetControlMode(EInteRealControlMode::View);
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

	UpdateMinimapIconVisibility(NewMode);
}

void AInteRealPlayerController::SetupMinimapHUD(
	UHarnessMinimapCaptureComponent* InCaptureComp,
	UTextureRenderTarget2D* InRT,
	TSubclassOf<UHarnessCaptureMinimapWidget> InWidgetClass)
{
	if (!InWidgetClass) return;

	if (MinimapWidgetInstance)
	{
		MinimapWidgetInstance->RemoveFromParent();
		MinimapWidgetInstance = nullptr;
	}

	MinimapWidgetInstance = CreateWidget<UHarnessCaptureMinimapWidget>(this, InWidgetClass);
	if (MinimapWidgetInstance)
	{
		MinimapWidgetInstance->InjectMinimapData(InCaptureComp, InRT);
		// 💡 [최종 수정] 미니맵을 버튼들(10)보다 아래인 ZOrder 5로 설정하여 차단 방지
		MinimapWidgetInstance->AddToViewport();
		MinimapWidgetInstance->SetVisibility(ESlateVisibility::Hidden);

		EHarnessViewMode CurrentMode = EHarnessViewMode::TopDown;
		if (CachedViewModeManager)
		{
			CurrentMode = CachedViewModeManager->GetCurrentViewMode();
		}
		UpdateMinimapIconVisibility(CurrentMode);
		UpdateModeUIVisibility();
	}
}

void AInteRealPlayerController::ShowMinimap()
{
	if (MinimapWidgetInstance && CurrentControlMode == EInteRealControlMode::View)
	{
		MinimapWidgetInstance->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void AInteRealPlayerController::UpdateMinimapIconVisibility(EHarnessViewMode NewMode)
{
	if (MinimapWidgetInstance && MinimapWidgetInstance->PlayerIcon)
	{
		const ESlateVisibility NewVisibility =
			(NewMode == EHarnessViewMode::FirstPerson)
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Hidden;

		MinimapWidgetInstance->PlayerIcon->SetVisibility(NewVisibility);
	}
}

void AInteRealPlayerController::OnTopDownKey()
{
	if (CurrentControlMode != EInteRealControlMode::View) return;
	SetViewMode(EHarnessViewMode::TopDown);
}

void AInteRealPlayerController::OnIsometricKey()
{
	if (CurrentControlMode != EInteRealControlMode::View) return;
	SetViewMode(EHarnessViewMode::Isometric);
}

void AInteRealPlayerController::OnFirstPersonKey()
{
	if (CurrentControlMode != EInteRealControlMode::View) return;
	SetViewMode(EHarnessViewMode::FirstPerson);
}

void AInteRealPlayerController::EnhancedMove(const FInputActionValue& Value)
{
	if (CurrentControlMode != EInteRealControlMode::View) return;

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

void AInteRealPlayerController::EnhancedLook(const FInputActionValue& Value)
{
	if (CurrentControlMode != EInteRealControlMode::View) return;

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

void AInteRealPlayerController::EnhancedZoom(const FInputActionValue& Value)
{
	if (CurrentControlMode != EInteRealControlMode::View) return;

	if (CachedViewModeManager && GetViewTarget() != GetPawn())
	{
		CachedViewModeManager->AddZoomInput(Value.Get<float>());
	}
}