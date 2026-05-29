// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GridSpaceManager.h"
#include "Intereal/EditMode/Furnitures/Furniture.h"
#include "Intereal/EditMode/Furnitures/FurnitureData.h"
#include "InteriorPlacementManager.generated.h"

UCLASS()
class INTEREAL_API AInteriorPlacementManager : public AActor
{
	GENERATED_BODY()

public:
	AInteriorPlacementManager();

protected:
	virtual void BeginPlay() override;

public:
	// 그리드 영역을 시각화할 액터
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	AActor* BoundsActor;

	// 배치 가능한 가구 데이터 목록 (FurnitureID = index)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture")
	TArray<UFurnitureData*> FurnitureDataList;

private:
	AGridSpaceManager* Grid;
	float CellSize;

	AFurniture* PreviewFurniture;
	UFurnitureData* PreviewFurnitureData;
	FVector2D PreviewGridAnchor; // 프리뷰 가구 좌상단 셀 좌표 (ConfirmFurniture에서 재사용)

public:
	UFUNCTION(BlueprintCallable)
	void InitializeGrid(int Length, int Breadth, float Cell);

	// 현재 프리뷰 위치가 비어있는지 (셀 전체)
	UFUNCTION(BlueprintPure)
	bool IsPreviewLotEmpty();

	// 프리뷰 가구를 확정 배치
	UFUNCTION(BlueprintCallable)
	void ConfirmFurniture();

	// 가구 선택 → 프리뷰 스폰 (FurnitureID = FurnitureDataList 인덱스)
	UFUNCTION(BlueprintCallable)
	void CreatePreviewFurniture(FVector RayPosition, FRotator Rotation, int FurnitureID);

	// 매 프레임 레이캐스트 결과로 프리뷰 위치 갱신 (그리드 스냅 + 색상 업데이트)
	UFUNCTION(BlueprintCallable)
	void UpdatePreviewLocation(FVector RayPosition);

	// 배치 취소
	UFUNCTION(BlueprintCallable)
	void CancelPreview();
};
