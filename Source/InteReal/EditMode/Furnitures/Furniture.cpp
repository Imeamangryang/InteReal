#include "Furniture.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"

static void UpdatePostProcessOutlineColor(UWorld* World, FLinearColor Color, float Thickness)
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
				MID->SetScalarParameterValue(TEXT("OutlineThickness"), Thickness);
			}
		}
		break; // 첫 번째 PostProcessVolume만 적용
	}
}

void AFurniture::SetMeshesCustomDepth(bool bEnabled, int32 StencilValue)
{
	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		Mesh->SetRenderCustomDepth(bEnabled);
		Mesh->SetCustomDepthStencilValue(StencilValue);
	}
}

void AFurniture::SetMeshesVisibilityCollision(ECollisionResponse Response)
{
	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		Mesh->SetCollisionResponseToChannel(ECC_Visibility, Response);
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
			CollisionBoxComponent->ShapeColor = FLinearColor(0.225781f, 0.420849f, 0.850000f, 0.5f).ToFColor(true);
			CollisionBoxComponent->MarkRenderStateDirty();
			CollisionBoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
			SetMeshesVisibilityCollision(ECR_Ignore);
			SetMeshesCustomDepth(true, 1);
			UpdatePostProcessOutlineColor(GetWorld(), FLinearColor(0.348958f, 0.552408f, 0.800000f, 0.5f), PreviewOutlineThickness);
			break;

		case EPlacementState::Invalid:
			CollisionBoxComponent->SetHiddenInGame(false);
			CollisionBoxComponent->ShapeColor = FLinearColor(0.850000f, 0.188151f, 0.222533f, 0.5f).ToFColor(true);
			CollisionBoxComponent->MarkRenderStateDirty();
			CollisionBoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
			SetMeshesVisibilityCollision(ECR_Ignore);
			SetMeshesCustomDepth(true, 1);
			UpdatePostProcessOutlineColor(GetWorld(), FLinearColor(0.800000f, 0.312500f, 0.348214f, 0.5f), PreviewOutlineThickness);
			break;

		case EPlacementState::Placed:
			CollisionBoxComponent->SetHiddenInGame(true);
			CollisionBoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			SetMeshesVisibilityCollision(ECR_Block);
			SetMeshesCustomDepth(false, 0);
			UpdatePostProcessOutlineColor(GetWorld(), FLinearColor(1.f, 1.f, 1.f, 1.f), PlacedOutlineThickness);
			break;
		}
	}
}

void AFurniture::SetSelected(bool bSelected)
{
	SetMeshesCustomDepth(bSelected, 1);
	CollisionBoxComponent->SetHiddenInGame(true);

	// OutlineColor/Thickness는 레벨 PostProcessVolume에 공유되는 값이라
	// 연속배치 중 마지막 프리뷰가 Invalid(빨강) 상태로 남아있으면 선택 하이라이트도 빨갱게 보임.
	// 선택 시 흰색(Placed 상태와 동일한 색)으로 명시적으로 되돌려준다.
	if (bSelected)
	{
		UpdatePostProcessOutlineColor(GetWorld(), FLinearColor(1.f, 1.f, 1.f, 1.f), PlacedOutlineThickness);
	}
	
	/*if (CollisionBoxComponent)
	{
		CollisionBoxComponent->SetHiddenInGame(!bSelected);
		CollisionBoxComponent->ShapeColor = FColor::Orange;
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
		// Z는 메시 바운드의 바닥(Origin - Extent)에 맞춰 가구 바닥에 표시되도록 함
		CollisionBoxComponent->SetRelativeLocation(FVector(MeshBounds.Origin.X, MeshBounds.Origin.Y, MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z));
		
		/*CollisionBoxComponent->SetBoxExtent(MeshBounds.BoxExtent);
		CollisionBoxComponent->SetRelativeLocation(MeshBounds.Origin);*/
	}

	SetPlacementState(EPlacementState::Preview);
}
