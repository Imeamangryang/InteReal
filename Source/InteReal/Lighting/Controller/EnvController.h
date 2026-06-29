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
	
	// 추가된 사운드 시스템
	UPROPERTY(EditAnywhere, Category = "WeatherFX|Sound") USoundBase* RainSound;      // 일반 비
	UPROPERTY(EditAnywhere, Category = "WeatherFX|Sound") USoundBase* StormRainSound; // 폭우
	UPROPERTY(EditAnywhere, Category = "WeatherFX|Sound") USoundBase* ThunderSound;   // 천둥
	
	// 조도 쨍함 방지를 위한 배율 변수 (Auto Exposure OFF 상태에서는 0.001 정도로 낮춰야 적절한 밝기가 나옵니다)
	UPROPERTY(EditAnywhere, Category = "Lighting|Control") float MasterIntensityMultiplier = 0.01f;
	
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
    
	UFUNCTION() 
	void UpdateEnvironment(FWeatherData W, FCityMainData C, FCityDetailData D, FSolarTermData S, float Time, float Orientation);
	
private:
	
	void HandleWeatherChange(FName WeatherID);
	void TriggerRandomLightning();
	
	UPROPERTY()
	UAudioComponent* RainAudioComponent; // 지속 재생 중인 비 사운드 컴포넌트
	
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
	
	void DelayedPlayRainSound();
	USoundBase* PendingRainSound;
	FTimerHandle RainSoundDelayHandle;
};