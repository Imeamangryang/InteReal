// Fill out your copyright notice in the Description page of Project Settings.


#include "GridSpaceManager.h"
#include "DrawDebugHelpers.h" 


// Sets default values
AGridSpaceManager::AGridSpaceManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

// Called when the game starts or when spawned
void AGridSpaceManager::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AGridSpaceManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AGridSpaceManager::Initialize(int L, int B, float Cell)
{
	Length = L;
	Breadth = B;
	CellSize = Cell;

	if (CellSize == 0.0f)
	{
		CellSize = 1.0f;
	}

	GridCells.SetNum(Length * Breadth);
}

int AGridSpaceManager::GetLength()
{
	return Length;
}

int AGridSpaceManager::GetBreadth()
{
	return Breadth;
}

float AGridSpaceManager::GetCellSize()
{
	return CellSize;
}

FVector2D AGridSpaceManager::ToGridPosition(FVector WorldPosition)
{
	int GridX = FMath::FloorToInt(WorldPosition.X / CellSize + Length * 0.5f);
	int GridY = FMath::FloorToInt(WorldPosition.Y / CellSize + Breadth * 0.5f);
	return FVector2D(GridX, GridY);
}

FVector AGridSpaceManager::ToWorldPosition(FVector2D GridPosition)
{
	float WorldX = (GridPosition.X - Length * 0.5f + 0.5f) * CellSize;
	float WorldY = (GridPosition.Y - Breadth * 0.5f + 0.5f) * CellSize;
	return FVector(WorldX, WorldY, 0.0f);
}

int AGridSpaceManager::GetIndex(FVector2D GridPosition)
{
	int X = (int)GridPosition.X;
	int Y = (int)GridPosition.Y;
	if (X < 0 || X >= Length || Y < 0 || Y >= Breadth)
	{
		return -1;
	}
		
	return X * Breadth + Y;
}

int AGridSpaceManager::GetIndexWorld(FVector WorldPosition)
{
	return GetIndex(ToGridPosition(WorldPosition));
}

// Occupancy 체크
AActor* AGridSpaceManager::GetFurniture(FVector2D GridPosition)
{
	int index = GetIndex(GridPosition);
	if (index == -1)
	{
		return nullptr;
	}
		
	return GridCells[index];
}

AActor* AGridSpaceManager::GetFurnitureWorld(FVector WorldPosition)
{
	int index = GetIndex(ToGridPosition(WorldPosition));
	if (index == -1)
	{
		return nullptr;
	}
		
	return GridCells[index];
}

void AGridSpaceManager::SetFurniture(FVector2D GridPosition, AActor* Furniture)
{
	int index = GetIndex(GridPosition);
	if (index == -1)
	{
		return;
	}
		
	GridCells[index] = Furniture;
}

void AGridSpaceManager::SetFurnitureWorld(FVector WorldPosition, AActor* Furniture)
{
	int index = GetIndex(ToGridPosition(WorldPosition));
	if (index == -1)
	{
		return;
	}
		
	GridCells[index] = Furniture;
}

void AGridSpaceManager::DrawGridLines()
{
	for (int i = 0; i <= Length; i++)
	{
		FVector Start = ToWorldPosition(FVector2D(i, 0)) - FVector(CellSize * 0.5f, CellSize * 0.5f, 0.0f);
		FVector End   = ToWorldPosition(FVector2D(i, Breadth)) - FVector(CellSize * 0.5f, CellSize * 0.5f, 0.0f);
		Start.Z = 1.0f;
		End.Z   = 1.0f;
		DrawDebugLine(GetWorld(), Start, End, FColor::White, false, -1.0f, 0, 0.5f);
	}

	for (int j = 0; j <= Breadth; j++)
	{
		FVector Start = ToWorldPosition(FVector2D(0, j)) - FVector(CellSize * 0.5f, CellSize * 0.5f, 0.0f);
		FVector End   = ToWorldPosition(FVector2D(Length, j)) - FVector(CellSize * 0.5f, CellSize * 0.5f, 0.0f);
		Start.Z = 1.0f;
		End.Z   = 1.0f;
		DrawDebugLine(GetWorld(), Start, End, FColor::White, false, -1.0f, 0, 0.5f);
	}
}
