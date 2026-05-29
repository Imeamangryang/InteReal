// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FurnitureData.generated.h"

class AFurniture;

UCLASS()
class INTEREAL_API UFurnitureData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FurnitureData")
	FText DisplayName = FText::FromString("Furniture");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FurnitureData")
	int ID = 0;

	// 리스트 썸네일
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture")
	TObjectPtr<UTexture2D> DisplayImage = nullptr;
	
	// 가로(X) x 세로(Y) 칸 수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FurnitureData")
	FVector2D Dimensions = FVector2D(1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FurnitureData")
	TSubclassOf<AFurniture> FurnitureBP;
};
