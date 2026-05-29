#pragma once
#include "CoreMinimal.h"
#include "InteReal/Struct/LightingDataStruct.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WeatherUISubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnEnvironmentUpdate, FWeatherData, W, FCityMainData, C, FCityDetailData, D, FSolarTermData, S, float, Time);

UCLASS()
class INTEREAL_API UWeatherUISubsystem : public UGameInstanceSubsystem {
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    
	UPROPERTY() UDataTable* WeatherTable;
	UPROPERTY() UDataTable* CityMainTable;
	UPROPERTY() UDataTable* CityDetailTable;
	UPROPERTY() UDataTable* SolarTermTable;
    
	UPROPERTY(BlueprintAssignable) FOnEnvironmentUpdate OnEnvironmentUpdate;
    
	UFUNCTION(BlueprintCallable)
	void ApplyEnvironment(FName City, FName Weather, FName Solar, float Time);
};