// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "EditModePlayerController.generated.h"

class AInteriorPlacementManager;
class UInputMappingContext;
class UInputAction;
class UFurnitureData;

UCLASS()
class INTEREAL_API AEditModePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AEditModePlayerController();

	UFUNCTION(BlueprintCallable, Category = "EditMode | Furniture")
	void StartFurniturePlacement(UFurnitureData* FurnitureData);

	// 웹 프론트엔드(EmitUIInteraction)에서 들어오는 JSON 명령 수신 단일 창구
	UFUNCTION(BlueprintCallable, Category = "EditMode | Web")
	void ReceiveWebCommand(const FString& JsonString);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

	UPROPERTY(BlueprintReadOnly, Category = "EditMode | Input")
	FVector CurrentCursorWorldLoc;

	bool bIsHitting = false;
	FHitResult LastCursorHit;

public:
	UPROPERTY(editAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> PlacementTabWidget;
	
	UPROPERTY()
	UUserWidget* PlacementTabInstance;
	
	UPROPERTY(EditAnywhere, Category = "EditMode")
	AInteriorPlacementManager* PlacementManager;

	UPROPERTY(EditAnywhere, Category = "EditMode | Input")
	UInputMappingContext* IMC_EditMode;

	UPROPERTY(EditAnywhere, Category = "EditMode | Input")
	UInputAction* IA_Place;

	UPROPERTY(EditAnywhere, Category = "EditMode | Input")
	UInputAction* IA_Remove;
	
	UPROPERTY(EditAnywhere, Category = "EditMode | Input")
	UInputAction* IA_Rotate;

private:
	bool bGridVisible = false;

	UFUNCTION(BlueprintPure, Category = "EditMode | Input")
	FVector GetCurrentCursorWorldLocation() const { return CurrentCursorWorldLoc; }

	UFUNCTION(BlueprintPure, Category = "EditMode | Input")
	bool GetIsHitting() const { return bIsHitting; }

	void UpdateCursorHit();
	void ToggleGrid();
	void OnPlace();
	void OnRemove();
	void OnRotatePreview();
	// void OnTestSpawn(); // TODO: 나중에 UI로 교체
};
