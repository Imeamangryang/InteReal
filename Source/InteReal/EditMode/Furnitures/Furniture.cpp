#include "Furniture.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"

static void UpdatePostProcessOutlineColor(UWorld* World, FLinearColor Color)
{
	if (!World) return;
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		for (FWeightedBlendable& WB : It->Settings.WeightedBlendables.Array)
		{
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(WB.Object);
			if (!MID)
			{
				if (UMaterialInterface* MI = Cast<UMaterialInterface>(WB.Object))
				{
					MID = UMaterialInstanceDynamic::Create(MI, *It);
					WB.Object = MID;
				}
			}
			if (MID)
			{
				MID->SetVectorParameterValue(TEXT("OutlineColor"), Color);
			}
		}
		break; // 첫 번째 PostProcessVolume만 적용
	}
}

AFurniture::AFurniture()
{
	PrimaryActorTick.bCanEverTick = false;
	PlacementState = EPlacementState::Preview;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;
	MeshComponent->bReceivesDecals = false;
	MeshComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);

	CollisionBoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBoxComponent->SetupAttachment(MeshComponent);
	CollisionBoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionBoxComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionBoxComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionBoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CollisionBoxComponent->SetHiddenInGame(true);
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
			CollisionBoxComponent->ShapeColor = FLinearColor(0.090755f, 0.328019f, 0.850000f, 0.5f).ToFColor(true);
			CollisionBoxComponent->MarkRenderStateDirty();
			CollisionBoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
			MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
			MeshComponent->SetRenderCustomDepth(true);
			// UpdatePostProcessOutlineColor(GetWorld(), FLinearColor(0.090755f, 0.328019f, 0.850000f, 0.5f));
			break;

		case EPlacementState::Invalid:
			CollisionBoxComponent->SetHiddenInGame(false);
			CollisionBoxComponent->ShapeColor = FLinearColor(0.85, 0.08f, 0.12f, 0.5f).ToFColor(true);
			CollisionBoxComponent->MarkRenderStateDirty();
			CollisionBoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
			MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
			MeshComponent->SetRenderCustomDepth(true);
			// UpdatePostProcessOutlineColor(GetWorld(), FLinearColor(0.85f, 0.08f, 0.12f, 0.5f));
			break;

		case EPlacementState::Placed:
			CollisionBoxComponent->SetHiddenInGame(true);
			CollisionBoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			MeshComponent->SetRenderCustomDepth(false);
			// UpdatePostProcessOutlineColor(GetWorld(), FLinearColor(1.f, 1.f, 1.f, 1.f)); // 선택 색상(흰색)으로 복원
			break;
		}
	}
}

void AFurniture::SetSelected(bool bSelected)
{
	MeshComponent->SetRenderCustomDepth(bSelected);
	CollisionBoxComponent->SetHiddenInGame(true);
	
	/*if (CollisionBoxComponent)
	{
		CollisionBoxComponent->SetHiddenInGame(!bSelected);
		CollisionBoxComponent->ShapeColor = FColor::White;
		// CollisionBoxComponent->LineThickness = 0.5f;
		CollisionBoxComponent->MarkRenderStateDirty();
	}*/
}

void AFurniture::ApplyFurnitureRow(const FFurnitureDataRow& InFurnitureRow)
{
	FurnitureID = InFurnitureRow.ID;
	AllowedPlacementTypes = InFurnitureRow.AllowedPlacementTypes;

	if (InFurnitureRow.FurnitureMesh)
	{
		MeshComponent->SetStaticMesh(InFurnitureRow.FurnitureMesh);

		const FBoxSphereBounds MeshBounds = InFurnitureRow.FurnitureMesh->GetBounds();
		FVector Extent = MeshBounds.BoxExtent;
		Extent.Z = 2.0f;
		CollisionBoxComponent->SetBoxExtent(Extent);
		CollisionBoxComponent->SetRelativeLocation(FVector(MeshBounds.Origin.X, MeshBounds.Origin.Y, MeshBounds.Origin.Z));
		
		/*CollisionBoxComponent->SetBoxExtent(MeshBounds.BoxExtent);
		CollisionBoxComponent->SetRelativeLocation(MeshBounds.Origin);*/
	}

	SetPlacementState(EPlacementState::Preview);
}
