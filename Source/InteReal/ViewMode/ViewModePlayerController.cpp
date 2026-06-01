#include "ViewModePlayerController.h"
#include "ViewModeManager.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Blueprint/UserWidget.h"
#include "InputActionValue.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Overlay.h"
#include "Public/HarnessCaptureMinimapWidget.h"

AViewModePlayerController::AViewModePlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void AViewModePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Add Input Mapping Context if available
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (ViewModeMappingContext)
		{
			Subsystem->AddMappingContext(ViewModeMappingContext, 0);
		}
	}

	// Create and add UI Widget
	if (ViewModeWidgetClass)
	{
		ViewModeWidgetInstance = CreateWidget<UUserWidget>(this, ViewModeWidgetClass);
		if (ViewModeWidgetInstance)
		{
			ViewModeWidgetInstance->AddToViewport();
		}
	}

	FindViewModeManager();

	// DefaultPawn 초기 설정: 보이지 않게 처리
	if (APawn* P = GetPawn())
	{
		P->SetActorHiddenInGame(true);
	}

	// 처음에는 TopDown이므로 매니저를 쳐다봄
	SetViewMode(EHarnessViewMode::TopDown);
}

void AViewModePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// 1. View Switching
		if (IA_SwitchToTopDown) EIC->BindAction(IA_SwitchToTopDown, ETriggerEvent::Started, this, &AViewModePlayerController::OnTopDownKey);
		if (IA_SwitchToIsometric) EIC->BindAction(IA_SwitchToIsometric, ETriggerEvent::Started, this, &AViewModePlayerController::OnIsometricKey);
		if (IA_SwitchToFirstPerson) EIC->BindAction(IA_SwitchToFirstPerson, ETriggerEvent::Started, this, &AViewModePlayerController::OnFirstPersonKey);

		// 2. Interactive Controls
		if (IA_Move) EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AViewModePlayerController::EnhancedMove);
		if (IA_Look) EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AViewModePlayerController::EnhancedLook);
		if (IA_Zoom) EIC->BindAction(IA_Zoom, ETriggerEvent::Triggered, this, &AViewModePlayerController::EnhancedZoom);
	}
}

void AViewModePlayerController::EnhancedMove(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	
	// 1인칭일 때는 Pawn을 직접 이동시킴
	if (GetViewTarget() == GetPawn())
	{
		APawn* P = GetPawn();
		if (P)
		{
			const FRotator Rotation = GetControlRotation();
			const FRotator YawRotation(0, Rotation.Yaw, 0);
			const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
			const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

			// 수정: X축(MovementVector.X)을 전진/후진으로, Y축(MovementVector.Y)을 좌우로 매칭
			P->AddMovementInput(ForwardDirection, MovementVector.X);
			P->AddMovementInput(RightDirection, MovementVector.Y);
		}
	}
	else if (CachedViewModeManager)
	{
		// 평면/ISO 이동
		CachedViewModeManager->AddMovementInput(FVector::ForwardVector, MovementVector.X);
		CachedViewModeManager->AddMovementInput(FVector::RightVector, MovementVector.Y);
	}
}

void AViewModePlayerController::EnhancedLook(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	bool bIsRightClick = IsInputKeyDown(EKeys::RightMouseButton);
	bool bIsMiddleClick = IsInputKeyDown(EKeys::MiddleMouseButton);

	// 1인칭일 때는 마우스 우클릭 시 고개 돌리기
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
		// 평면/ISO 회전 및 Pan
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

void AViewModePlayerController::EnhancedZoom(const FInputActionValue& Value)
{
	if (CachedViewModeManager && GetViewTarget() != GetPawn())
	{
		CachedViewModeManager->AddZoomInput(Value.Get<float>());
	}
}


void AViewModePlayerController::FindViewModeManager()
{
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AViewModeManager::StaticClass(), FoundActors);
	if (FoundActors.Num() > 0)
	{
		CachedViewModeManager = Cast<AViewModeManager>(FoundActors[0]);
	}
}

void AViewModePlayerController::SetViewMode(EHarnessViewMode NewMode)
{
	if (!CachedViewModeManager)
	{
		FindViewModeManager();
	}

	APawn* P = GetPawn();
	if (!P) return;

	if (NewMode == EHarnessViewMode::FirstPerson)
	{
		// 1인칭: DefaultPawn에 빙의하고 건물 중앙으로 텔레포트
		if (CachedViewModeManager)
		{
			CachedViewModeManager->FocusOnBuilding(); // 최신 센터 계산
			FVector Center = CachedViewModeManager->GetTargetLocation();
			Center.Z = 160.0f; // 눈높이
			P->SetActorLocation(Center);
		}
		
		Possess(P);
		SetViewTarget(P);
		P->SetActorHiddenInGame(true); // 혹시 보이게 설정되었다면 다시 숨김
	}
	else
	{
		// 평면/ISO: 카메라 매니저 액터를 뷰 타겟으로 설정
		if (CachedViewModeManager)
		{
			CachedViewModeManager->SetViewMode(NewMode);
			SetViewTarget(CachedViewModeManager);
		}
	}

	UpdateMinimapIconVisibility(NewMode);
}

void AViewModePlayerController::UpdateMinimapIconVisibility(EHarnessViewMode NewMode)
{
	TArray<UUserWidget*> FoundWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), FoundWidgets, UHarnessCaptureMinimapWidget::StaticClass());

	for (UUserWidget* Widget : FoundWidgets)
	{
		if (UHarnessCaptureMinimapWidget* Minimap = Cast<UHarnessCaptureMinimapWidget>(Widget))
		{
			if (Minimap->PlayerIcon)
			{
				ESlateVisibility NewVisibility = (NewMode == EHarnessViewMode::FirstPerson) ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden;
				Minimap->PlayerIcon->SetVisibility(NewVisibility);
			}
		}
	}
}

void AViewModePlayerController::OnTopDownKey()
{
	SetViewMode(EHarnessViewMode::TopDown);
}

void AViewModePlayerController::OnIsometricKey()
{
	SetViewMode(EHarnessViewMode::Isometric);
}

void AViewModePlayerController::OnFirstPersonKey()
{
	SetViewMode(EHarnessViewMode::FirstPerson);
}
