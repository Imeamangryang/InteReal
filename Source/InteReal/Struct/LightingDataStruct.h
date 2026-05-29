#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "LightingDataStruct.generated.h"

// 1. Weather
USTRUCT(BlueprintType)
struct FWeatherData : public FTableRowBase {
	GENERATED_BODY()
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float IntensityLux = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Temperature = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FogDensity = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SkyIntensity = 0.0f;
};

// 2. CityMain
USTRUCT(BlueprintType)
struct FCityMainData : public FTableRowBase {
	GENERATED_BODY()
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 CityID = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Latitude = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Longitude = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Description = TEXT("");
};

// 3. CityDetail
USTRUCT(BlueprintType)
struct FCityDetailData : public FTableRowBase {
	GENERATED_BODY()
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Parent_CityID = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Latitude = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Longitude = 0.0f;
};

// 4. SolarTerm
USTRUCT(BlueprintType)
struct FSolarTermData : public FTableRowBase {
	GENERATED_BODY()
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name_KR = TEXT("");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name_EN = TEXT("");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Declination = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Season = TEXT("");
};