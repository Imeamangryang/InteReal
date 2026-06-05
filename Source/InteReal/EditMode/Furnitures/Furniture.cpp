#include "Furniture.h"

AFurniture::AFurniture()
{
	PrimaryActorTick.bCanEverTick = false;
	PlacementState = EPlacementState::Preview;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;
	MeshComponent->bReceivesDecals = false;

	CollisionBoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBoxComponent->SetupAttachment(MeshComponent);
	CollisionBoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionBoxComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionBoxComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionBoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CollisionBoxComponent->SetHiddenInGame(true);
}

void AFurniture::BeginPlay()
{
	Super::BeginPlay();
}

void AFurniture::SetPlacementState(EPlacementState NewState)
{
	PlacementState = NewState;

	if (CollisionBoxComponent)
	{
		switch (NewState)
		{
		case EPlacementState::Preview:
			CollisionBoxComponent->SetHiddenInGame(false);
			CollisionBoxComponent->ShapeColor = FColor(0, 255, 255, 255);
			CollisionBoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
			// 메시도 Visibility 트레이스에서 제외 — 프리뷰 자신이 커서 레이를 막으면 바닥을 못 찾음
			MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
			break;

		case EPlacementState::Invalid:
			CollisionBoxComponent->SetHiddenInGame(false);
			CollisionBoxComponent->ShapeColor = FColor(255, 0, 0, 255);
			CollisionBoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
			MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
			break;

		case EPlacementState::Placed:
			CollisionBoxComponent->SetHiddenInGame(true);
			CollisionBoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			break;
		}
	}
}

void AFurniture::SetSelected(bool bSelected)
{
	CollisionBoxComponent->SetHiddenInGame(true);
	
	MeshComponent->SetRenderCustomDepth(bSelected);
	
	
	if (CollisionBoxComponent)
	{
		CollisionBoxComponent->SetHiddenInGame(!bSelected);
		CollisionBoxComponent->ShapeColor = FColor::White;
	}
}

void AFurniture::ApplyFurnitureRow(const FFurnitureDataRow& InFurnitureRow)
{
	FurnitureID = InFurnitureRow.ID;

	if (InFurnitureRow.FurnitureMesh)
	{
		MeshComponent->SetStaticMesh(InFurnitureRow.FurnitureMesh);

		const FBoxSphereBounds MeshBounds = InFurnitureRow.FurnitureMesh->GetBounds();
		CollisionBoxComponent->SetBoxExtent(MeshBounds.BoxExtent);
		CollisionBoxComponent->SetRelativeLocation(MeshBounds.Origin);
	}

	SetPlacementState(EPlacementState::Preview);
}
