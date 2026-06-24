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
#include "InteReal/Harness/Public/HarnessPipelineManager.h"
#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"

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
			// 💡 [ZOrder 수정] 상호작용 버튼들이 최상단에 오도록 10 설정
			ViewModeWidgetInstance->AddToViewport();
		}
	}


	FindViewModeManager();

	// DefaultPawn 초기 설정: 보이지 않게 처리
	if (APawn* P = GetPawn())
	{
		P->SetActorHiddenInGame(true);
	}

	// 처음에는 ISO 뷰이므로 매니저를 쳐다봄
	SetViewMode(EHarnessViewMode::Isometric);
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

	// 💡 [핵심 수정 3] 1인칭이든 아니든 Manager의 상태(CurrentMode)를 무조건 동기화시킵니다.
	if (CachedViewModeManager)
	{
		CachedViewModeManager->SetViewMode(NewMode);
	}

	APawn* P = GetPawn();
	if (!P) return;

	if (NewMode == EHarnessViewMode::FirstPerson)
	{
		// 1인칭: DefaultPawn에 빙의하고 건물 중앙으로 텔레포트
		if (CachedViewModeManager)
		{
			CachedViewModeManager->FocusOnBuilding(); // 최신 센터 계산
			FVector Center = CachedViewModeManager->GetCameraTargetLocation();
			if (UHarnessPipelineManager* PipelineManager = GetWorld()->GetSubsystem<UHarnessPipelineManager>())
			{
				if (UHarnessGeneratorComponent* GenComp = PipelineManager->GetGeneratorComp())
				{
					Center = GenComp->GetSafeSpawnLocation();
				}
			}
			
			// GetSafeSpawnLocation에서 반환하는 안전한 Z값을 유지하여 건물의 높이와 무관하게 동작하게 수정
			// 충돌체 끼임 방지를 위해 Teleport 옵션 추가
			P->SetActorLocation(Center, false, nullptr, ETeleportType::TeleportPhysics);

			// 캔버스 회전 시 캐릭터/컨트롤러 방위각 동기화
			if (CachedViewModeManager->IsCanvasRotated())
			{
				FRotator Rot(0.f, 0.f, 0.f);
				P->SetActorRotation(Rot);
				SetControlRotation(Rot);
			}
			else
			{
				FRotator Rot(0.f, -90.f, 0.f);
				P->SetActorRotation(Rot);
				SetControlRotation(Rot);
			}
		}
		
		Possess(P);
		SetViewTarget(P);
		P->SetActorHiddenInGame(true);
	}
	else
	{
		// 평면/ISO: 카메라 매니저 액터를 뷰 타겟으로 설정
		if (CachedViewModeManager)
		{
			SetViewTarget(CachedViewModeManager);
		}
	}

	UpdateMinimapIconVisibility(NewMode);
}


// 캡처가 끝난 후 위젯을 다시 활성화
void AViewModePlayerController::ShowMinimap()
{
	if (MinimapWidgetInstance)
	{
		// 미니맵은 클릭 상호작용이 필요하므로 Visible 처리
		MinimapWidgetInstance->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void AViewModePlayerController::UpdateMinimapIconVisibility(EHarnessViewMode NewMode)
{
	if (MinimapWidgetInstance && MinimapWidgetInstance->PlayerIcon)
	{
		ESlateVisibility NewVisibility = (NewMode == EHarnessViewMode::FirstPerson) ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden;
		MinimapWidgetInstance->PlayerIcon->SetVisibility(NewVisibility);
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
