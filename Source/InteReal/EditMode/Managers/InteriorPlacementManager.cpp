// Fill out your copyright notice in the Description page of Project Settings.

#include "InteriorPlacementManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DrawDebugHelpers.h"

AInteriorPlacementManager::AInteriorPlacementManager()
{
	PrimaryActorTick.bCanEverTick = false;
	Grid = nullptr;
	PreviewFurniture = nullptr;
	PreviewFurnitureData = nullptr;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	GridDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("GridDecal"));
	GridDecal->SetupAttachment(RootComponent);
	
	GridDecal->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	GridDecal->SetFadeScreenSize(0.0f); 
	GridDecal->SetVisibility(false);
}

void AInteriorPlacementManager::BeginPlay()
{
	Super::BeginPlay(); 
	InitializeGrid(GridLength, GridBreadth, GridCellSize);
}

void AInteriorPlacementManager::InitializeFromFloorData(const FHarnessFloorData& FloorData, float Cell)
{
	if (FloorData.vertices.IsEmpty())
	{
		return;
	}

	float MinX = TNumericLimits<float>::Max();
	float MaxX = TNumericLimits<float>::Lowest();
	float MinY = TNumericLimits<float>::Max();
	float MaxY = TNumericLimits<float>::Lowest();

	for (const FTopologyVertex& V : FloorData.vertices)
	{
		MinX = FMath::Min(MinX, V.x);
		MaxX = FMath::Max(MaxX, V.x);
		MinY = FMath::Min(MinY, V.y);
		MaxY = FMath::Max(MaxY, V.y);
	}

	// 도면 중심으로 액터 이동 (Z는 유지)
	float CenterX = (MinX + MaxX) * 0.5f;
	float CenterY = (MinY + MaxY) * 0.5f;
	SetActorLocation(FVector(CenterX, CenterY, GetActorLocation().Z));

	int Length = FMath::CeilToInt((MaxX - MinX) / Cell);
	int Breadth = FMath::CeilToInt((MaxY - MinY) / Cell);
	InitializeGrid(Length, Breadth, Cell);
}

void AInteriorPlacementManager::InitializeGrid(int Length, int Breadth, float Cell)
{
	Grid = GetWorld()->SpawnActor<AGridSpaceManager>(AGridSpaceManager::StaticClass());
	Grid->Initialize(Length, Breadth, Cell);
	CellSize = Grid->GetCellSize();

	// DecalSize: X = 투영 깊이(바닥 아래로), Y/Z = 범위 반절
	float HalfX = Length * CellSize * 0.5f;
	float HalfY = Breadth * CellSize * 0.5f;
	GridDecal->DecalSize = FVector(50.0f, HalfX, HalfY);

	if (GridMaterial)
	{
		UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(GridMaterial, this);
		DynMat->SetScalarParameterValue(TEXT("CellSize"), Cell);
		GridDecal->SetDecalMaterial(DynMat);
	}
}

void AInteriorPlacementManager::SetGridVisible(bool bVisible)
{
	GridDecal->SetVisibility(bVisible);
}

bool AInteriorPlacementManager::HasActivePreview() const
{
	return PreviewFurniture != nullptr;
}

bool AInteriorPlacementManager::IsPreviewLotEmpty()
{
	if (!PreviewFurniture || !PreviewFurnitureData || !Grid)
	{
		return false;
	}

	int L = (int)PreviewFurnitureData->Dimensions.X;
	int B = (int)PreviewFurnitureData->Dimensions.Y;

	for (int i = 0; i < L; i++)
	{
		for (int j = 0; j < B; j++)
		{
			if (Grid->GetFurniture(FVector2D(PreviewGridAnchor.X + i, PreviewGridAnchor.Y + j)) != nullptr)
			{
				return false;
			}
		}
	}
	return true;
}

bool AInteriorPlacementManager::IsPreviewBoundsEmpty() const
{
	if (!PreviewFurniture)
	{
		return false;
	}

	FBox PreviewBox = PreviewFurniture->GetComponentsBoundingBox().ExpandBy(-1.0f);

	for (AFurniture* Placed : PlacedFurnitures)
	{
		if (!IsValid(Placed))
		{
			continue;
		}

		if (PreviewBox.Intersect(Placed->GetComponentsBoundingBox()))
		{
			return false;
		}
	}
	return true;
}

void AInteriorPlacementManager::ConfirmFurniture()
{
	if (!PreviewFurniture || !IsPreviewBoundsEmpty())
	{
		return;
	}

	int L = (int)PreviewFurnitureData->Dimensions.X;
	int B = (int)PreviewFurnitureData->Dimensions.Y;

	for (int i = 0; i < L; i++)
	{
		for (int j = 0; j < B; j++)
		{
			Grid->SetFurniture(FVector2D(PreviewGridAnchor.X + i, PreviewGridAnchor.Y + j), PreviewFurniture);
		}
	}

	PreviewFurniture->SetPlacementState(EPlacementState::Placed);
	PlacedFurnitures.Add(PreviewFurniture);
	PreviewFurniture = nullptr;
	PreviewFurnitureData = nullptr;
}

void AInteriorPlacementManager::CreatePreviewFurnitureFromData(FVector RayPosition, FRotator Rotation, UFurnitureData* InFurnitureData)
{
	if (PreviewFurniture)
	{
		PreviewFurniture->Destroy();
		PreviewFurniture = nullptr;
	}

	if (!InFurnitureData || !InFurnitureData->FurnitureBP)
	{
		return;
	}

	PreviewFurnitureData = InFurnitureData;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PreviewFurniture = GetWorld()->SpawnActor<AFurniture>(PreviewFurnitureData->FurnitureBP, RayPosition, Rotation, Params);
	if (!PreviewFurniture)
	{
		return;
	}

	PreviewFurniture->ApplyFurnitureData(PreviewFurnitureData);
	UpdatePreviewLocation(RayPosition);
}

void AInteriorPlacementManager::UpdatePreviewLocation(FVector RayPosition)
{
	if (!PreviewFurniture || !PreviewFurnitureData || !Grid)
	{
		return;
	}

	FVector2D GridPos = Grid->ToGridPosition(RayPosition);
	int SnapX = (int)GridPos.X;
	int SnapY = (int)GridPos.Y;

	int L = (int)PreviewFurnitureData->Dimensions.X;
	int B = (int)PreviewFurnitureData->Dimensions.Y;
	PreviewGridAnchor = FVector2D(SnapX - L / 2, SnapY - B / 2);

	FVector SnappedWorld = Grid->ToWorldPosition(FVector2D(SnapX, SnapY));
	PreviewFurniture->SetActorLocation(SnappedWorld);

	EPlacementState NewState = IsPreviewBoundsEmpty() ? EPlacementState::Preview : EPlacementState::Invalid;
	PreviewFurniture->SetPlacementState(NewState);

	FVector Center, Extent;
	PreviewFurniture->GetActorBounds(false, Center, Extent);
	FColor BoxColor = (NewState == EPlacementState::Preview) ? FColor::Green : FColor::Red;
	DrawDebugBox(GetWorld(), Center, Extent, BoxColor, false, -1.0f, 0, 2.0f);
}

void AInteriorPlacementManager::DrawBounds() const
{
	for (AFurniture* Placed : PlacedFurnitures)
	{
		if (!IsValid(Placed))
		{
			continue;
		}

		FVector Center, Extent;
		Placed->GetActorBounds(false, Center, Extent);
		DrawDebugBox(GetWorld(), Center, Extent, FColor::White, false, -1.0f, 0, 2.0f);
	}
}

void AInteriorPlacementManager::RemoveFurniture(AFurniture* Target)
{
	if (!Target || !Grid)
	{
		return;
	}

	for (int i = 0; i < Grid->GetLength(); i++)
	{
		for (int j = 0; j < Grid->GetBreadth(); j++)
		{
			if (Grid->GetFurniture(FVector2D(i, j)) == Target)
			{
				Grid->SetFurniture(FVector2D(i, j), nullptr);
			}
		}
	}

	PlacedFurnitures.Remove(Target);
	Target->Destroy();
}

void AInteriorPlacementManager::CancelPreview()
{
	if (PreviewFurniture)
	{
		PreviewFurniture->Destroy();
		PreviewFurniture = nullptr;
	}
	PreviewFurnitureData = nullptr;
}
