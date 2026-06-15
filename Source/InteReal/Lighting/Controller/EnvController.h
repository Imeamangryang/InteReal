#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
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
	
	// 추가된 이펙트 시스템
	UPROPERTY(VisibleAnywhere, Category = "WeatherFX") UNiagaraComponent* WeatherNiagara;
	UPROPERTY(EditAnywhere, Category = "WeatherFX") TMap<FName, UNiagaraSystem*> WeatherEffectsMap; 
	UPROPERTY(EditAnywhere, Category = "WeatherFX") TArray<AActor*> LightningSplineActors;
	UPROPERTY(EditAnywhere, Category = "WeatherFX") UPointLightComponent* LightningLight;
	
	
	// 조도 쨍함 방지를 위한 배율 변수
	UPROPERTY(EditAnywhere, Category = "Lighting|Control") float MasterIntensityMultiplier = 1.0f;
	
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
    
	UFUNCTION() 
	void UpdateEnvironment(FWeatherData W, FCityMainData C, FCityDetailData D, FSolarTermData S, float Time, float Orientation);
	
private:
	
	void HandleWeatherChange(FName WeatherID);
	void TriggerRandomLightning();
	FTimerHandle LightningTimerHandle;
	FName LastWeatherID = NAME_None;
	
	float TargetSunIntensity = 0.0f;
	float TargetSkyIntensity = 0.0f;
	// 첫 데이터 수신 여부
	bool bIsInitialized = false; 
	const float InterpSpeed = 3.0f;
	
	UPROPERTY(EditAnywhere, Category = "WeatherFX") 
	float AvoidanceRadius = 500.0f;
	
	void UpdateBuildingMask();
	FTimerHandle BuildingScanTimer;
	
};