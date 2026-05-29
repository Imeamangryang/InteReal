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
	// Sets default values for this actor's properties
	AGridSpaceManager();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
private:
	int Length;
	int Breadth;
	float CellSize;
	TArray<AActor*> GridCells;
	
public:
	UFUNCTION(BlueprintCallable)
	void Initialize(int L, int B, float Cell);
	
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
	
	UFUNCTION(BlueprintPure, meta=(DisplayName="Get Index"))
	int GetIndexWorld(FVector WorldPosition);
	
	// --- occupancy
	UFUNCTION(BlueprintPure)
	AActor* GetFurniture(FVector2D GridPosition);
	
	UFUNCTION(BlueprintPure, meta=(DisplayName="Get Furniture"))
	AActor* GetFurnitureWorld(FVector WorldPosition);
	
	UFUNCTION(BlueprintCallable)
	void SetFurniture(FVector2D GridPosition, AActor* Furniture);

	UFUNCTION(BlueprintCallable, meta=(DisplayName="Set Furniture"))
	void SetFurnitureWorld(FVector WorldPosition, AActor* Furniture);
	
	void DrawGridLines();
	
};
