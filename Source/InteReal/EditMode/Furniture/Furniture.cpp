#include "Furniture.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "PhysicsEngine/BodySetup.h"
#include "InteReal/EditMode/Managers/GridSpaceManager.h"

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
	
	UE_LOG(LogTemp, Warning, TEXT("[Furniture::SetSelected][Before] Furniture=%s bSelected=%d Mesh=%s CollisionEnabled=%d VisibilityResponse=%d"),
		*GetNameSafe(this),
		bSelected,
		*GetNameSafe(MeshComponent),
		MeshComponent ? static_cast<int32>(MeshComponent->GetCollisionEnabled()) : -1,
		MeshComponent ? static_cast<int32>(MeshComponent->GetCollisionResponseToChannel(ECC_Visibility)) : -1);

	SetMeshesCustomDepth(bSelected, 1);
	CollisionBoxComponent->SetHiddenInGame(true);
	
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
	
	UE_LOG(LogTemp, Warning, TEXT("[Furniture::SetSelected][After] Furniture=%s bSelected=%d Mesh=%s CollisionEnabled=%d VisibilityResponse=%d"),
		*GetNameSafe(this),
		bSelected,
		*GetNameSafe(MeshComponent),
		MeshComponent ? static_cast<int32>(MeshComponent->GetCollisionEnabled()) : -1,
		MeshComponent ? static_cast<int32>(MeshComponent->GetCollisionResponseToChannel(ECC_Visibility)) : -1);
}

void AFurniture::ApplyFurnitureRow(const FFurnitureDataRow& InFurnitureRow)
{
	FurnitureID = InFurnitureRow.ID;
	AllowedPlacementTypes = InFurnitureRow.AllowedPlacementTypes;

	if (InFurnitureRow.FurnitureMesh)
	{
		MeshComponent->SetStaticMesh(InFurnitureRow.FurnitureMesh);

		const FBoxSphereBounds MeshBounds = InFurnitureRow.FurnitureMesh->GetBounds();
		FBox PlacementBounds = MeshBounds.GetBox();
		bool bUsesSimpleCollision = false;
		if (const UBodySetup* BodySetup = InFurnitureRow.FurnitureMesh->GetBodySetup())
		{
			// Authored simple collision is a more reliable placement footprint than render bounds.
			if (BodySetup->AggGeom.GetElementCount() > 0)
			{
				const FBox CollisionBounds = BodySetup->AggGeom.CalcAABB(FTransform::Identity);
				if (CollisionBounds.IsValid && CollisionBounds.GetSize().X > KINDA_SMALL_NUMBER &&
					CollisionBounds.GetSize().Y > KINDA_SMALL_NUMBER)
				{
					PlacementBounds = CollisionBounds;
					bUsesSimpleCollision = true;
				}
			}
		}

		FVector Extent = PlacementBounds.GetExtent();
		PlacementLocalBounds = PlacementBounds;
		Extent.Z = 2.0f;
		CollisionBoxComponent->SetBoxExtent(Extent);
		
		const bool bCeilingOnly = SupportsPlacementType(EPlacementSurfaceType::Ceiling)
		                       && !SupportsPlacementType(EPlacementSurfaceType::Floor);
		const float IndicatorZ = bCeilingOnly ? PlacementBounds.Max.Z : PlacementBounds.Min.Z;
		CollisionBoxComponent->SetRelativeLocation(FVector(
			PlacementBounds.GetCenter().X, PlacementBounds.GetCenter().Y, IndicatorZ));
		CollisionBoxComponent->UpdateBounds();

		UE_LOG(LogTemp, Log,
			TEXT("[Placement] Furniture %d mesh=%s renderSize=%s placementSize=%s source=%s"),
			FurnitureID, *GetNameSafe(InFurnitureRow.FurnitureMesh),
			*MeshBounds.GetBox().GetSize().ToCompactString(),
			*PlacementBounds.GetSize().ToCompactString(),
			bUsesSimpleCollision ? TEXT("SimpleCollision") : TEXT("RenderBounds"));
		
		/*CollisionBoxComponent->SetBoxExtent(MeshBounds.BoxExtent);
		CollisionBoxComponent->SetRelativeLocation(MeshBounds.Origin);*/
	}

	SetPlacementState(EPlacementState::Preview);
}

void AFurniture::GetOccupiedGridCells(const AGridSpaceManager* Grid, FVector2D Anchor,
	FVector2D Dimensions, TArray<FIntPoint>& OutCells) const
{
	OutCells.Reset();
	if (!Grid)
	{
		return;
	}

	const int32 L = FMath::Max(1, FMath::RoundToInt(Dimensions.X));
	const int32 B = FMath::Max(1, FMath::RoundToInt(Dimensions.Y));
	const UStaticMesh* StaticMesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
	const UBodySetup* BodySetup = StaticMesh ? StaticMesh->GetBodySetup() : nullptr;
	const bool bHasSimpleCollision = BodySetup && BodySetup->AggGeom.GetElementCount() > 0;
	const float CellHalfExtent = Grid->GetCellSize() * 0.48f;
	const FBox Bounds = GetPlacementGeometryBounds();
	const float HalfHeight = FMath::Max(1.0f, Bounds.GetExtent().Z - 1.0f);

	for (int32 X = 0; X < L; ++X)
	{
		for (int32 Y = 0; Y < B; ++Y)
		{
			const FIntPoint Cell(FMath::RoundToInt(Anchor.X) + X, FMath::RoundToInt(Anchor.Y) + Y);
			bool bOccupied = true;
			if (bHasSimpleCollision && MeshComponent)
			{
				FVector CellCenter = Grid->ToWorldPosition(FVector2D(Cell.X, Cell.Y));
				CellCenter.Z = Bounds.GetCenter().Z;
				bOccupied = MeshComponent->OverlapComponent(
					CellCenter, FQuat::Identity,
					FCollisionShape::MakeBox(FVector(CellHalfExtent, CellHalfExtent, HalfHeight)));
			}

			if (bOccupied)
			{
				OutCells.Add(Cell);
			}
		}
	}

	// A missing physics body must not make furniture occupy no cells at all.
	if (OutCells.IsEmpty())
	{
		for (int32 X = 0; X < L; ++X)
		{
			for (int32 Y = 0; Y < B; ++Y)
			{
				OutCells.Emplace(FMath::RoundToInt(Anchor.X) + X, FMath::RoundToInt(Anchor.Y) + Y);
			}
		}
	}
}

void AFurniture::AlignMeshBottomToZ(float SurfaceZ)
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh())
	{
		return;
	}

	const float DeltaZ = SurfaceZ - MeshComponent->Bounds.GetBox().Min.Z;
	if (!FMath::IsNearlyZero(DeltaZ))
	{
		AddActorWorldOffset(FVector(0.0f, 0.0f, DeltaZ));
	}
}

void AFurniture::AlignMeshBottomCenterTo(const FVector& TargetCenter, float SurfaceZ)
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh())
	{
		return;
	}

	const FBox MeshBounds = GetMeshBounds();
	const FVector MeshCenter = MeshBounds.GetCenter();
	AddActorWorldOffset(FVector(
		TargetCenter.X - MeshCenter.X,
		TargetCenter.Y - MeshCenter.Y,
		SurfaceZ - MeshBounds.Min.Z));
}

void AFurniture::AlignPlacementBottomCenterTo(const FVector& TargetCenter, float SurfaceZ)
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh())
	{
		return;
	}

	const FBox Bounds = GetPlacementGeometryBounds();
	const FVector Center = Bounds.GetCenter();
	AddActorWorldOffset(FVector(
		TargetCenter.X - Center.X,
		TargetCenter.Y - Center.Y,
		SurfaceZ - Bounds.Min.Z));
}

void AFurniture::SetRotationPreservingPlacement(const FRotator& NewRotation)
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh())
	{
		SetActorRotation(NewRotation);
		return;
	}

	const FBox Before = GetMeshBounds();
	SetActorRotation(NewRotation);
	const FBox After = GetMeshBounds();

	FVector Offset(
		Before.GetCenter().X - After.GetCenter().X,
		Before.GetCenter().Y - After.GetCenter().Y,
		Before.GetCenter().Z - After.GetCenter().Z);

	if (PlacedSurfaceType == EPlacementSurfaceType::Floor ||
		PlacedSurfaceType == EPlacementSurfaceType::Surface)
	{
		Offset.Z = Before.Min.Z - After.Min.Z;
	}
	else if (PlacedSurfaceType == EPlacementSurfaceType::Ceiling)
	{
		Offset.Z = Before.Max.Z - After.Max.Z;
	}

	AddActorWorldOffset(Offset);
}
