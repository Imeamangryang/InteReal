#include "ViewModeManager.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"
#include "InteReal/Harness/Public/HarnessPipelineManager.h"

AViewModeManager::AViewModeManager()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->bDoCollisionTest = false; 
	SpringArm->SetRelativeRotation(FRotator(-45.f, 45.f, 0.f));
	SpringArm->TargetArmLength = 1500.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);

	// Initial Targets
	TargetLocation = FVector::ZeroVector;
	TargetRotation = FRotator(-45.f, 45.f, 0.f);
	TargetArmLength = 1500.f;
	TargetFOV = 60.f;
}

void AViewModeManager::BeginPlay()
{
	Super::BeginPlay();
	
	FocusOnBuilding();
	UpdateTargetParameters();

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetViewTarget(this);
	}
}

void AViewModeManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Smooth Interpolation
	FVector CurrentLoc = GetActorLocation();
	SetActorLocation(FMath::VInterpTo(CurrentLoc, TargetLocation, DeltaTime, TransitionSpeed));

	FRotator CurrentRot = SpringArm->GetRelativeRotation();
	SpringArm->SetRelativeRotation(FMath::RInterpTo(CurrentRot, TargetRotation, DeltaTime, TransitionSpeed));

	SpringArm->TargetArmLength = FMath::FInterpTo(SpringArm->TargetArmLength, TargetArmLength, DeltaTime, TransitionSpeed);
	Camera->FieldOfView = FMath::FInterpTo(Camera->FieldOfView, TargetFOV, DeltaTime, TransitionSpeed);
}

void AViewModeManager::SetViewMode(EHarnessViewMode NewMode)
{
	CurrentMode = NewMode;
	UpdateTargetParameters();
}

void AViewModeManager::FocusOnBuilding()
{
	UHarnessGeneratorComponent* GenComp = nullptr;

	if (UHarnessPipelineManager* PipelineManager = GetWorld()->GetSubsystem<UHarnessPipelineManager>())
	{
		GenComp = PipelineManager->GetGeneratorComp();
	}

	if (GenComp)
	{
		FVector2D Min, Max;
		GenComp->GetFloorBounds(Min, Max);
		FVector Center((Min.X + Max.X) / 2.0f, (Min.Y + Max.Y) / 2.0f, 0.0f);
		TargetLocation = Center;

		CalculateOptimalZoom();
	}
}

void AViewModeManager::ToggleCanvasRotation()
{
	bIsCanvasRotated = !bIsCanvasRotated;
	CalculateOptimalZoom();
	UpdateTargetParameters();
}

void AViewModeManager::CalculateOptimalZoom()
{
	UHarnessGeneratorComponent* GenComp = nullptr;

	if (UHarnessPipelineManager* PipelineManager = GetWorld()->GetSubsystem<UHarnessPipelineManager>())
	{
		GenComp = PipelineManager->GetGeneratorComp();
	}

	if (!GenComp) return;

	FVector2D Min, Max;
	GenComp->GetFloorBounds(Min, Max);
	
	float DistX = Max.X - Min.X; // 남북(세로) 길이
	float DistY = Max.Y - Min.Y; // 동서(가로) 길이

	// (캔버스 회전) 기능 대응을 위해 가로/세로 매핑
	float ScreenWidthRequired = bIsCanvasRotated ? DistX : DistY;
	float ScreenHeightRequired = bIsCanvasRotated ? DistY : DistX;

	// 16:9 모니터의 좁은 상하 폭에 담기 위해 세로 길이에 1.77배 가중치 적용
	float AdjustedHeight = ScreenHeightRequired * 1.77f; 
	float EffectiveSize = FMath::Max(ScreenWidthRequired, AdjustedHeight);

	if (EffectiveSize > 0)
	{
		// 1. TopDown 전용 ArmLength 계산 (TopDownFOV 기준)
		float BaseDistTD = (EffectiveSize * 0.5f) / FMath::Tan(FMath::DegreesToRadians(TopDownFOV * 0.5f));
		TopDownArmLength = FMath::Clamp(BaseDistTD * 1.2f, 1000.f, 15000.f);

		// 2. Isometric 전용 ArmLength 계산 (IsometricFOV 기준)
		float BaseDistIso = (EffectiveSize * 0.5f) / FMath::Tan(FMath::DegreesToRadians(IsometricFOV * 0.5f));
		IsometricArmLength = FMath::Clamp(BaseDistIso * 1.2f, 1000.f, 15000.f) * 1.2f;

		if (CurrentMode == EHarnessViewMode::TopDown) TargetArmLength = TopDownArmLength;
		else if (CurrentMode == EHarnessViewMode::Isometric) TargetArmLength = IsometricArmLength;
	}
}

void AViewModeManager::UpdateTargetParameters()
{
	float BaseYaw = bIsCanvasRotated ? -90.f : 0.f;

	switch (CurrentMode)
	{
	case EHarnessViewMode::TopDown:
		TargetRotation = FRotator(-90.f, BaseYaw, 0.f);
		TargetArmLength = TopDownArmLength;
		TargetFOV = TopDownFOV;
		break;

	case EHarnessViewMode::Isometric:
		TargetRotation = FRotator(-45.f, 45.f + BaseYaw, 0.f);
		TargetArmLength = IsometricArmLength;
		TargetFOV = IsometricFOV;
		break;

	case EHarnessViewMode::FirstPerson:
		TargetLocation.Z = FirstPersonHeight; 
		TargetRotation = FRotator(0.f, BaseYaw, 0.f);
		TargetArmLength = 0.f; 
		TargetFOV = FirstPersonFOV;
		break;
	}
}

void AViewModeManager::AddRotationInput(float DeltaYaw, float DeltaPitch)
{
	if (CurrentMode == EHarnessViewMode::TopDown) return;

	TargetRotation.Yaw += DeltaYaw * RotationSensitivity;
	
	if (CurrentMode == EHarnessViewMode::FirstPerson)
	{
		TargetRotation.Pitch = FMath::Clamp(TargetRotation.Pitch + DeltaPitch * RotationSensitivity, -80.f, 80.f);
	}
	else if (CurrentMode == EHarnessViewMode::Isometric)
	{
		TargetRotation.Pitch = FMath::Clamp(TargetRotation.Pitch + DeltaPitch * RotationSensitivity, -85.f, -15.f);
	}
}

void AViewModeManager::AddMovementInput(FVector Direction, float Scale)
{
	// 1인칭은 PlayerController에서 Pawn을 직접 조작하므로 매니저 이동 제외
	if (CurrentMode == EHarnessViewMode::FirstPerson) return;

	// 카메라가 멀어질수록(줌아웃) 이동 속도도 비례해서 증폭
	float ZoomSpeedMultiplier = FMath::Max(1.0f, TargetArmLength / 1000.0f);
	float ActualSpeed = MovementSpeed * ZoomSpeedMultiplier; 

	// 현재 캔버스 회전각(Yaw)을 기준으로 방향(Local Axis) 추출
	FRotator YawRot(0.f, TargetRotation.Yaw, 0.f);
	FVector WorldDir = FRotationMatrix(YawRot).TransformVector(Direction);
	
	TargetLocation += WorldDir * ActualSpeed * Scale * GetWorld()->GetDeltaSeconds();
}

void AViewModeManager::AddPanInput(float DeltaX, float DeltaY)
{
	if (CurrentMode == EHarnessViewMode::FirstPerson) return;

	FRotator YawRotation(0.f, TargetRotation.Yaw, 0.f);
	FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

	TargetLocation -= (RightDir * DeltaX + ForwardDir * DeltaY) * PanSpeed;
}

void AViewModeManager::AddZoomInput(float Delta)
{
	if (CurrentMode == EHarnessViewMode::FirstPerson) return;
	TargetArmLength = FMath::Clamp(TargetArmLength - (Delta * ZoomSpeed), 300.f, 5000.f);
}

void AViewModeManager::SnapToTarget()
{
    SetActorLocation(TargetLocation);
    SpringArm->SetRelativeRotation(TargetRotation);
    SpringArm->TargetArmLength = TargetArmLength;
    Camera->FieldOfView = TargetFOV;
}

void AViewModeManager::FocusOnLocation(FVector WorldLocation)
{
	TargetLocation.X = WorldLocation.X;
	TargetLocation.Y = WorldLocation.Y;
}
