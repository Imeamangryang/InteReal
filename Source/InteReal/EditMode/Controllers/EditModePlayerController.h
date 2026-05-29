// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "EditModePlayerController.generated.h"

class AInteriorPlacementManager;
class UInputMappingContext;
class UInputAction;

UCLASS()
class INTEREAL_API AEditModePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AEditModePlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

	UPROPERTY(BlueprintReadOnly, Category = "EditMode | Input")
	FVector CurrentCursorWorldLoc;

	bool bIsHitting = false;
	FHitResult LastCursorHit;

public:
	UPROPERTY(EditAnywhere, Category = "EditMode")
	AInteriorPlacementManager* PlacementManager;

	UPROPERTY(EditAnywhere, Category = "EditMode | Input")
	UInputMappingContext* IMC_EditMode;

	UPROPERTY(EditAnywhere, Category = "EditMode | Input")
	UInputAction* IA_Place;

	UPROPERTY(EditAnywhere, Category = "EditMode | Input")
	UInputAction* IA_Remove;

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
	void OnTestSpawn(); // TODO: 나중에 UI로 교체
};
