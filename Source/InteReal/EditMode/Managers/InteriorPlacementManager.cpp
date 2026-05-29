// Fill out your copyright notice in the Description page of Project Settings.

#include "InteriorPlacementManager.h"

AInteriorPlacementManager::AInteriorPlacementManager()
{
	PrimaryActorTick.bCanEverTick = false;
	Grid = nullptr;
	PreviewFurniture = nullptr;
	PreviewFurnitureData = nullptr;
}

void AInteriorPlacementManager::BeginPlay()
{
	Super::BeginPlay();
}

void AInteriorPlacementManager::InitializeGrid(int Length, int Breadth, float Cell)
{
	Grid = GetWorld()->SpawnActor<AGridSpaceManager>(AGridSpaceManager::StaticClass());
	Grid->Initialize(Length, Breadth, Cell);
	CellSize = Grid->GetCellSize();

	// BoundsActor는 100x100 UU 기준 평면 메시를 사용한다고 가정
	if (BoundsActor)
	{
		float ScaleX = Length * CellSize / 100.0f;
		float ScaleY = Breadth * CellSize / 100.0f;
		BoundsActor->SetActorRelativeScale3D(FVector(ScaleX, ScaleY, 1.0f));
	}
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

void AInteriorPlacementManager::ConfirmFurniture()
{
	if (!PreviewFurniture || !IsPreviewLotEmpty())
		return;

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
	PreviewFurniture = nullptr;
	PreviewFurnitureData = nullptr;
}

void AInteriorPlacementManager::CreatePreviewFurniture(FVector RayPosition, FRotator Rotation, int FurnitureID)
{
	if (PreviewFurniture)
	{
		PreviewFurniture->Destroy();
		PreviewFurniture = nullptr;
	}

	if (!FurnitureDataList.IsValidIndex(FurnitureID))
		return;

	PreviewFurnitureData = FurnitureDataList[FurnitureID];
	if (!PreviewFurnitureData || !PreviewFurnitureData->FurnitureBP)
		return;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PreviewFurniture = GetWorld()->SpawnActor<AFurniture>(PreviewFurnitureData->FurnitureBP, RayPosition, Rotation, Params);
	if (!PreviewFurniture)
		return;

	PreviewFurniture->FurnitureData = PreviewFurnitureData;
	UpdatePreviewLocation(RayPosition);
}

void AInteriorPlacementManager::UpdatePreviewLocation(FVector RayPosition)
{
	if (!PreviewFurniture || !PreviewFurnitureData || !Grid)
		return;

	// 그리드 셀에 스냅
	FVector2D GridPos = Grid->ToGridPosition(RayPosition);
	int SnapX = (int)GridPos.X;
	int SnapY = (int)GridPos.Y;

	// 가구 중심 기준으로 좌상단 앵커 계산 (ConfirmFurniture에서 재사용)
	int L = (int)PreviewFurnitureData->Dimensions.X;
	int B = (int)PreviewFurnitureData->Dimensions.Y;
	PreviewGridAnchor = FVector2D(SnapX - L / 2, SnapY - B / 2);

	// 스냅된 셀 중심 월드 좌표로 이동
	FVector SnappedWorld = Grid->ToWorldPosition(FVector2D(SnapX, SnapY));
	PreviewFurniture->SetActorLocation(SnappedWorld);

	// 빈 칸이면 Preview(초록), 막혀있으면 Invalid(빨강)
	EPlacementState NewState = IsPreviewLotEmpty() ? EPlacementState::Preview : EPlacementState::Invalid;
	PreviewFurniture->SetPlacementState(NewState);
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
