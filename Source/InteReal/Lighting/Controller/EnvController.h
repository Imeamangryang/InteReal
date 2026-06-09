#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "InteReal/Struct/LightingDataStruct.h"
#include "EnvController.generated.h"

UCLASS()
class INTEREAL_API AEnvController : public AActor
{
	GENERATED_BODY()

public:
	AEnvController();

	UPROPERTY(EditAnywhere, Category = "Lighting") UDirectionalLightComponent* SunLight;
	UPROPERTY(EditAnywhere, Category = "Lighting") class ASkyLight* SkyLight;
	UPROPERTY(EditAnywhere, Category = "Lighting") UExponentialHeightFogComponent* Fog;
    
	// 조도 쨍함 방지를 위한 배율 변수
	UPROPERTY(EditAnywhere, Category = "Lighting|Control") float MasterIntensityMultiplier = 1.0f;
	
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
    
	UFUNCTION() 
	void UpdateEnvironment(FWeatherData W, FCityMainData C, FCityDetailData D, FSolarTermData S, float Time, float Orientation);
	
private:
	float TargetSunIntensity = 0.0f;
	float TargetSkyIntensity = 0.0f;
	// 첫 데이터 수신 여부
	bool bIsInitialized = false; 
	const float InterpSpeed = 3.0f;
};