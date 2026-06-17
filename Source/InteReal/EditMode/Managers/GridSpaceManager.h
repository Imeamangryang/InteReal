// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GridSpaceManager.generated.h"

UENUM(BlueprintType)
enum class EGridTileState : uint8
{
	None UMETA(DisplayName = "배치 불가 (도면 외부)"),
	Walkable UMETA(DisplayName = "배치 가능 (도면 내부)"),
};

UCLASS()
class INTEREAL_API AGridSpaceManager : public AActor
{
	GENERATED_BODY()

public:
	AGridSpaceManager();

private:
	int Length;
	int Breadth;
	float CellSize;
	FVector2D GridOrigin;
	TArray<AActor*> GridCells;
	TArray<EGridTileState> TileStates;

public:
	UFUNCTION(BlueprintCallable)
	void Initialize(int L, int B, float Cell);

	UFUNCTION(BlueprintCallable)
	void SetOrigin(FVector2D Origin);

	UFUNCTION(BlueprintPure)
	int GetLength();

	UFUNCTION(BlueprintPure)
	int GetBreadth();

	UFUNCTION(BlueprintPure)
	float GetCellSize();

	UFUNCTION(BlueprintPure)
	FVector2D ToGridPosition(FVector WorldPosition);

	UFUNCTION(BlueprintPure)
	FVector ToWorldPosition(FVector2D GridPosition);

	UFUNCTION(BlueprintPure)
	int GetIndex(FVector2D GridPosition);

	// --- occupancy
	UFUNCTION(BlueprintPure)
	AActor* GetFurniture(FVector2D GridPosition);

	UFUNCTION(BlueprintCallable)
	void SetFurniture(FVector2D GridPosition, AActor* Furniture);

	UFUNCTION(BlueprintCallable)
	void ClearFurnitureOccupancy();

	// --- tile state (도면 내부/외부 판정)
	UFUNCTION(BlueprintPure)
	EGridTileState GetTileState(FVector2D GridPosition);

	UFUNCTION(BlueprintCallable)
	void SetTileState(FVector2D GridPosition, EGridTileState State);
};
