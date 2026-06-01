#include "ViewModeManager.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"

AViewModeManager::AViewModeManager()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->bDoCollisionTest = false; 
	SpringArm->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	SpringArm->TargetArmLength = 1500.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);

	// Initial Targets
	TargetLocation = FVector::ZeroVector;
	TargetRotation = FRotator(-90.f, 0.f, 0.f);
	TargetArmLength = 1500.f;
	TargetFOV = 90.f;
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
	
	// 1. Try finding actor with tag "HarnessGenerator"
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), TEXT("HarnessGenerator"), FoundActors);
	
	if (FoundActors.Num() > 0)
	{
		GenComp = FoundActors[0]->FindComponentByClass<UHarnessGeneratorComponent>();
	}

	// 2. If not found by tag, search by class
	if (!GenComp)
	{
		TArray<AActor*> AllActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), AllActors);
		for (AActor* Actor : AllActors)
		{
			GenComp = Actor->FindComponentByClass<UHarnessGeneratorComponent>();
			if (GenComp) break;
		}
	}

	if (GenComp)
	{
		FVector2D Min, Max;
		GenComp->GetFloorBounds(Min, Max);
		FVector Center((Min.X + Max.X) / 2.0f, (Min.Y + Max.Y) / 2.0f, 0.0f);
		TargetLocation = Center;
	}
}

void AViewModeManager::UpdateTargetParameters()
{
	switch (CurrentMode)
	{
	case EHarnessViewMode::TopDown:
		TargetRotation = FRotator(-90.f, 0.f, 0.f);
		TargetArmLength = TopDownArmLength;
		TargetFOV = TopDownFOV;
		break;

	case EHarnessViewMode::Isometric:
		TargetRotation = FRotator(-45.f, 45.f, 0.f);
		TargetArmLength = IsometricArmLength;
		TargetFOV = IsometricFOV;
		break;

	case EHarnessViewMode::FirstPerson:
		TargetLocation.Z = FirstPersonHeight; 
		TargetRotation = FRotator(0.f, 0.f, 0.f);
		TargetArmLength = 0.f; 
		TargetFOV = FirstPersonFOV;
		break;
	}
}

void AViewModeManager::AddRotationInput(float DeltaYaw, float DeltaPitch)
{
	if (CurrentMode == EHarnessViewMode::TopDown) return; // 평면 뷰는 회전 금지

	TargetRotation.Yaw += DeltaYaw * RotationSensitivity;
	
	if (CurrentMode == EHarnessViewMode::FirstPerson)
	{
		TargetRotation.Pitch = FMath::Clamp(TargetRotation.Pitch + DeltaPitch * RotationSensitivity, -80.f, 80.f);
	}
	else if (CurrentMode == EHarnessViewMode::Isometric)
	{
		// ISO 뷰에서는 Pitch를 약간만 조절 가능하거나 일정 범위로 제한
		TargetRotation.Pitch = FMath::Clamp(TargetRotation.Pitch + DeltaPitch * RotationSensitivity, -85.f, -15.f);
	}
}

void AViewModeManager::AddMovementInput(FVector Direction, float Scale)
{
	if (CurrentMode != EHarnessViewMode::FirstPerson) return;

	// 현재 카메라 방향 기준으로 이동 방향 계산
	FRotator Rotation = TargetRotation;
	Rotation.Pitch = 0.f; // 수평 이동만 허용
	
	FVector WorldDirection = FRotationMatrix(Rotation).TransformVector(Direction);
	TargetLocation += WorldDirection * MovementSpeed * Scale * GetWorld()->GetDeltaSeconds();
}

void AViewModeManager::AddPanInput(float DeltaX, float DeltaY)
{
	if (CurrentMode == EHarnessViewMode::FirstPerson) return; // 1인칭은 WASD 사용

	// 현재 카메라의 Yaw 방향을 기준으로 화면 이동 (Pan)
	FRotator YawRot(0.f, TargetRotation.Yaw, 0.f);
	FVector RightDir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	FVector ForwardDir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);

	// 마우스 이동 방향과 반대로 이동해야 "화면을 잡고 끄는" 느낌이 납니다.
	TargetLocation -= (RightDir * DeltaX + ForwardDir * DeltaY) * PanSpeed;
}

void AViewModeManager::AddZoomInput(float Delta)
{
	if (CurrentMode == EHarnessViewMode::FirstPerson) return; // 1인칭은 줌인/아웃 없음

	// 줌 인/아웃 (카메라와 타겟 사이의 거리 조절)
	TargetArmLength = FMath::Clamp(TargetArmLength - (Delta * ZoomSpeed), 300.f, 5000.f);
}
