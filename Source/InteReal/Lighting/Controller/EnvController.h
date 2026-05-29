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

	virtual void BeginPlay() override;
	
	// 서브시스템의 5개 매개변수 델리게이트와 정확히 매칭되는 함수
	UFUNCTION() 
	void UpdateEnvironment(FWeatherData W, FCityMainData C, FCityDetailData D, FSolarTermData S, float Time);
};