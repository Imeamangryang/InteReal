#include "EnvController.h"

#include "InteReal/Lighting/UIManager/WeatherUISubsystem.h"
#include "InteReal/Struct/LightingDataStruct.h"
#include "Engine/SkyLight.h"

AEnvController::AEnvController()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AEnvController::BeginPlay()
{
    Super::BeginPlay();
    
    // 서브시스템 가져오기
    UWeatherUISubsystem* Sub = GetGameInstance()->GetSubsystem<UWeatherUISubsystem>();
    if (Sub)
    {
        Sub->OnEnvironmentUpdate.AddDynamic(this, &AEnvController::UpdateEnvironment);
        // 강제로 업데이트 실행
        Sub->ForceUpdate();
    }
    
    // 초기값 동기화
    if (SunLight) TargetSunIntensity = SunLight->Intensity;
    if (IsValid(SkyLight) && SkyLight->GetLightComponent()) TargetSkyIntensity = SkyLight->GetLightComponent()->Intensity;
    
}

void AEnvController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    if (!bIsInitialized) return;
    
    // 부드러운 조도 변화 적용
    if (SunLight)
    {
        float CurrentIntensity = FMath::FInterpTo(SunLight->Intensity, TargetSunIntensity, DeltaTime, InterpSpeed);
        SunLight->SetIntensity(CurrentIntensity);
    }

    if (IsValid(SkyLight) && SkyLight->GetLightComponent())
    {
        float CurrentSkyIntensity = FMath::FInterpTo(SkyLight->GetLightComponent()->Intensity, TargetSkyIntensity, DeltaTime, InterpSpeed);
        SkyLight->GetLightComponent()->SetIntensity(CurrentSkyIntensity);
    }
}

void AEnvController::UpdateEnvironment(FWeatherData W, FCityMainData C, FCityDetailData D, FSolarTermData S, float Time, float Orientation)
{
    if (!SunLight) return;

    // 계산 로직
    float HourAngle = (Time - 12.0f) * 15.0f;
    float RadLat = FMath::DegreesToRadians(C.Latitude);
    float RadDec = FMath::DegreesToRadians(S.Declination);
    float RadHA = FMath::DegreesToRadians(HourAngle);

    float SinAlt = FMath::Sin(RadLat) * FMath::Sin(RadDec) + FMath::Cos(RadLat) * FMath::Cos(RadDec) * FMath::Cos(RadHA);
    float Altitude = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(SinAlt, -1.0f, 1.0f)));
    float Azimuth = FMath::RadiansToDegrees(
        FMath::Atan2(
            FMath::Sin(RadHA),
            FMath::Cos(RadDec) * FMath::Sin(RadLat) - FMath::Sin(RadDec) * FMath::Cos(RadLat) * FMath::Cos(RadHA)
        )
    );
    
    // 즉시 적용 항목
    SunLight->SetRelativeRotation(FRotator(-Altitude, Azimuth + Orientation + 180.0f, 0.0f));
    SunLight->SetLightColor(FLinearColor::MakeFromColorTemperature(W.Temperature));
    
    // 목표값 갱신
    float IntensityMultiplier = (Altitude > 0) ? 1.0f : 0.05f;
    
    TargetSunIntensity = W.IntensityLux * IntensityMultiplier; 
    TargetSkyIntensity = W.SkyIntensity * IntensityMultiplier; 

    if (IsValid(SkyLight) && SkyLight->GetLightComponent())
    {
        SkyLight->GetLightComponent()->RecaptureSky();
    }

    if (IsValid(Fog))
    {
        Fog->SetFogDensity(W.FogDensity);
        FLinearColor FogColor = FLinearColor::LerpUsingHSV(FLinearColor::White, FLinearColor::Gray, W.SkyIntensity);
        Fog->SetFogInscatteringColor(FogColor);
    }
}