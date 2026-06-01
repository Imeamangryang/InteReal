// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GridSpaceManager.generated.h"

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
	
};
