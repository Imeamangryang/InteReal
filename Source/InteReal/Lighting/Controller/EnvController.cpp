#include "EnvController.h"
#include "InteReal/Lighting/UIManager/WeatherUISubsystem.h"
#include "InteReal/Struct/LightingDataStruct.h"
#include "Engine/SkyLight.h"
AEnvController::AEnvController()
{
	PrimaryActorTick.bCanEverTick = false; // 틱이 필요 없다면 false
}

void AEnvController::BeginPlay() {
	Super::BeginPlay();
	auto* Sub = GetGameInstance()->GetSubsystem<UWeatherUISubsystem>();
	if (Sub) {
		// 서브시스템의 FiveParams 델리게이트에 바인딩
		Sub->OnEnvironmentUpdate.AddDynamic(this, &AEnvController::UpdateEnvironment);
	}
}

void AEnvController::UpdateEnvironment(FWeatherData W, FCityMainData C, FCityDetailData D, FSolarTermData S, float Time) {
	if (!SunLight) return;

	// 1. 태양 위치 (고도: 위도-적위, 방위: 시간대)
	float SunPitch = 90.0f - C.Latitude + S.Declination;
	float SunYaw = (Time / 24.0f) * 360.0f;
	SunLight->SetRelativeRotation(FRotator(SunPitch, SunYaw, 0.0f));

	// 2. 조명 강도 및 색상 (CSV의 Temperature 변환)
	SunLight->SetIntensity(W.IntensityLux);
	SunLight->SetLightColor(FLinearColor::MakeFromColorTemperature(W.Temperature));
	// 3. 루멘 및 안개 갱신
	if (SkyLight) {
		// SkyLight는 이제 액터이므로 GetLightComponent()를 통해 컴포넌트를 가져옴
		USkyLightComponent* LightComp = SkyLight->GetLightComponent();
		if (LightComp) {
			LightComp->SetIntensity(W.SkyIntensity);
			LightComp->RecaptureSky();
		}
	}
	if (Fog) Fog->SetFogDensity(W.FogDensity);
}