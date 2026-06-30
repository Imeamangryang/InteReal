#include "ViewModeCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

namespace
{
	void ConfigureFirstPersonCapsuleCollision(UCapsuleComponent* Capsule)
	{
		if (!Capsule)
		{
			return;
		}

		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Capsule->SetCollisionObjectType(ECC_Pawn);
		Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
		Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	}
}

AViewModeCharacter::AViewModeCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// 1. 물리 설정: 캡슐 크기를 사람 정도로 설정
	GetCapsuleComponent()->InitCapsuleSize(35.f, 90.0f);
	ConfigureFirstPersonCapsuleCollision(GetCapsuleComponent());

	// 2. 이동 설정: 공중을 날지 않고 바닥을 걷도록 설정
	GetCharacterMovement()->DefaultLandMovementMode = MOVE_Walking;
	GetCharacterMovement()->MaxWalkSpeed = 400.f;

	// 3. 회전 설정: 카메라를 돌리면 몸체(캡슐)도 함께 돌도록 설정
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true; // 이 설정이 미니맵 아이콘 회전의 핵심입니다.
	bUseControllerRotationRoll = false;

	// 4. 카메라 설정: 160cm 높이에 배치
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	// 캡슐 중심(0,0,0)은 캐릭터 배꼽 위치이므로 상대 좌표로 눈높이 보정
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, EyeHeight - 90.0f)); 
	FirstPersonCamera->bUsePawnControlRotation = true; // 마우스 우클릭 시 고개 돌리기 가능
}

void AViewModeCharacter::BeginPlay()
{
	Super::BeginPlay();
	ConfigureFirstPersonCapsuleCollision(GetCapsuleComponent());
	
	// 시각적으로 숨김 (요청 사항)
	SetActorHiddenInGame(true);
}
