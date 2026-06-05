#pragma once

#include "CoreMinimal.h"
#include "Furnitures/FFurnitureDataRow.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "InteRealUISubSystem.generated.h"

class UMaterialInterface;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnModeChanged, bool, bIsEditMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFurnitureSpawn, FFurnitureDataRow, FurnitureData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWallMaterialChanged, UMaterialInterface*, NewMaterial);

UCLASS()
class INTEREAL_API UInteRealUISubSystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;	
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "InteReal|UI")					// 에디트 / 뷰 모드 체인지용 델리게이트
	FOnModeChanged OnModeChanged;
	
	UPROPERTY(BlueprintAssignable, Category = "InteReal|UI")
	FOnFurnitureSpawn OnFurnitureSpawn;
	
	UPROPERTY(BlueprintAssignable, Category = "InteReal|UI")
	FOnWallMaterialChanged OnWallMaterialChanged;

	UFUNCTION(BlueprintCallable, Category = "InteReal|UI")						// 에디트 / 뷰 모드 체인지용 함수
	void NotifyModeChanged(bool bIsEditMode);
	
	UFUNCTION(BlueprintCallable, Category = "InteReal|UI")
	void NotifyFurnitureSpawn(const FFurnitureDataRow& FurnitureData);
	
	UFUNCTION(BlueprintCallable, Category = "InteReal|UI")
	void NotifyWallMaterialChanged(UMaterialInterface* NewMaterial);
};
