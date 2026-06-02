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

	GizmoComponent = CreateDefaultSubobject<UFurnitureGizmoComponent>(TEXT("GizmoComponent"));
	GizmoComponent->SetupAttachment(RootComponent);
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
			break;
		case EPlacementState::Invalid:
			CollisionBoxComponent->SetHiddenInGame(false);
			CollisionBoxComponent->ShapeColor = FColor(255, 0, 0, 255);
			break;
		case EPlacementState::Placed:
			CollisionBoxComponent->SetHiddenInGame(true);
			break;
		}
	}

	if (GizmoComponent)
	{
		if (NewState == EPlacementState::Preview)
		{
			GizmoComponent->SetGizmoVisible(true);
			GizmoComponent->SetGizmoColor(FLinearColor(0.0f, 1.0f, 1.0f, 1.0f), 4.0f);
		}
		else if (NewState == EPlacementState::Invalid)
		{
			GizmoComponent->SetGizmoVisible(true);
			GizmoComponent->SetGizmoColor(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f), 4.0f);
		}
		else
		{
			GizmoComponent->SetGizmoVisible(false);
		}
	}
}

void AFurniture::SetSelected(bool bSelected)
{
	if (CollisionBoxComponent)
	{
		CollisionBoxComponent->SetHiddenInGame(!bSelected);
		CollisionBoxComponent->ShapeColor = FColor::White;
	}

	if (GizmoComponent)
	{
		GizmoComponent->SetGizmoVisible(bSelected);
		if (bSelected)
		{
			GizmoComponent->SetGizmoColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), 4.0f);
		}
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

	if (InFurnitureRow.FurnitureMesh && GizmoComponent)
	{
		const FBoxSphereBounds MeshBounds = InFurnitureRow.FurnitureMesh->GetBounds();
		GizmoComponent->SetupFromLocalBounds(MeshBounds.GetBox());
	}

	SetPlacementState(EPlacementState::Preview);
}
