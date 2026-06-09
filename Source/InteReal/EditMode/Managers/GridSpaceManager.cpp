// Fill out your copyright notice in the Description page of Project Settings.

#include "GridSpaceManager.h"

AGridSpaceManager::AGridSpaceManager()
{
	PrimaryActorTick.bCanEverTick = false;
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

	GridOrigin = FVector2D::ZeroVector;
	GridCells.SetNum(Length * Breadth);
	TileStates.Init(EGridTileState::Walkable, Length * Breadth);
}

void AGridSpaceManager::SetOrigin(FVector2D Origin)
{
	GridOrigin = Origin;
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
	int GridX = FMath::FloorToInt((WorldPosition.X - GridOrigin.X) / CellSize + Length * 0.5f);
	int GridY = FMath::FloorToInt((WorldPosition.Y - GridOrigin.Y) / CellSize + Breadth * 0.5f);
	return FVector2D(GridX, GridY);
}

FVector AGridSpaceManager::ToWorldPosition(FVector2D GridPosition)
{
	float WorldX = (GridPosition.X - Length * 0.5f + 0.5f) * CellSize + GridOrigin.X;
	float WorldY = (GridPosition.Y - Breadth * 0.5f + 0.5f) * CellSize + GridOrigin.Y;
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

AActor* AGridSpaceManager::GetFurniture(FVector2D GridPosition)
{
	int index = GetIndex(GridPosition);
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

EGridTileState AGridSpaceManager::GetTileState(FVector2D GridPosition)
{
	int index = GetIndex(GridPosition);
	if (index < 0 || index >= TileStates.Num())
	{
		return EGridTileState::None;
	}
	return TileStates[index];
}

void AGridSpaceManager::SetTileState(FVector2D GridPosition, EGridTileState State)
{
	int index = GetIndex(GridPosition);
	if (index < 0 || index >= TileStates.Num()) return;
	TileStates[index] = State;
}
