#pragma once
#include "CoreMinimal.h"
#include "InteReal/Struct/LightingDataStruct.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WeatherUISubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams(FOnEnvironmentUpdate, FWeatherData, W, FCityMainData, C, FCityDetailData, D, FSolarTermData, S, float, Time, float, Orientation);

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
    
	UFUNCTION(BlueprintCallable) TArray<FString> GetCityDetails(FName ParentCityName);
	UFUNCTION(BlueprintCallable) TArray<FString> GetSolarTermsBySeason(FString Season);
	UFUNCTION(BlueprintCallable) FName GetSolarRowName(FString NameKR);
    
	// 개별 업데이트 함수
	UFUNCTION(BlueprintCallable) void SetCityDetail(FName ID);
	UFUNCTION(BlueprintCallable) void SetWeather(FName ID);
	UFUNCTION(BlueprintCallable) void SetSolar(FName ID);
	UFUNCTION(BlueprintCallable) void SetTime(float Time);
	UFUNCTION(BlueprintCallable) void SetOrientation(float Offset);

	// 현재 설정된 값으로 즉시 브로드캐스트
	UFUNCTION(BlueprintCallable) void ForceUpdate();
	
private:
	FName CurrentCityDetailID = TEXT("Seoul_Gangnam");
	FName CurrentWeatherID = TEXT("Clear");
	FName CurrentSolarID = TEXT("SpringEquinox");
	float CurrentTime = 12.0f;
	float CurrentOrientation = 0.0f;
    
	void BroadcastEnvironment();
};