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

	UPROPERTY(EditAnywhere, Category = "UI") TSubclassOf<class UUserWidget> WeatherWidgetClass;
    
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
    
	UFUNCTION() 
	void UpdateEnvironment(FWeatherData W, FCityMainData C, FCityDetailData D, FSolarTermData S, float Time, float Orientation);

	void OnToggleUIMode();

private:
	UPROPERTY() UUserWidget* WeatherWidgetInstance;
	bool bWasInFirstPerson = false;
};