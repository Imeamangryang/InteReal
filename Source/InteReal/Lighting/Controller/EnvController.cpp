#include "EnvController.h"

#include "Blueprint/UserWidget.h"
#include "InteReal/Lighting/UIManager/WeatherUISubsystem.h"
#include "InteReal/Struct/LightingDataStruct.h"
#include "Engine/SkyLight.h"
#include "Kismet/GameplayStatics.h"
#include "Components/InputComponent.h"

AEnvController::AEnvController()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoReceiveInput = EAutoReceiveInput::Player0;
}

void AEnvController::BeginPlay() {
    Super::BeginPlay();
    
    auto* Sub = GetGameInstance()->GetSubsystem<UWeatherUISubsystem>();
    if (Sub) {
       Sub->OnEnvironmentUpdate.AddDynamic(this, &AEnvController::UpdateEnvironment);
    }
    
    if (WeatherWidgetClass) {
       WeatherWidgetInstance = CreateWidget<UUserWidget>(GetWorld(), WeatherWidgetClass);
       if (WeatherWidgetInstance) {
          WeatherWidgetInstance->AddToViewport();
       }
    }
    
    EnableInput(GetWorld()->GetFirstPlayerController());
    if (InputComponent) {
        InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AEnvController::OnToggleUIMode);
    }
}

void AEnvController::OnToggleUIMode() {
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;

    FInputModeGameAndUI InputMode;
    if (IsValid(WeatherWidgetInstance)) {
        InputMode.SetWidgetToFocus(WeatherWidgetInstance->TakeWidget());
    }
    PC->SetInputMode(InputMode);
    PC->bShowMouseCursor = true;
    bWasInFirstPerson = false;
}

void AEnvController::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;

    bool bIsFirstPerson = (PC->GetViewTarget() == PC->GetPawn());

    if (bIsFirstPerson != bWasInFirstPerson) {
       if (bIsFirstPerson) {
          FInputModeGameOnly InputMode;
          PC->SetInputMode(InputMode);
          PC->bShowMouseCursor = false;
       } else {
          FInputModeGameAndUI InputMode;
          if (IsValid(WeatherWidgetInstance)) {
             InputMode.SetWidgetToFocus(WeatherWidgetInstance->TakeWidget());
          }
          PC->SetInputMode(InputMode);
          PC->bShowMouseCursor = true;
       }
       bWasInFirstPerson = bIsFirstPerson;
    }
}

void AEnvController::UpdateEnvironment(FWeatherData W, FCityMainData C, FCityDetailData D, FSolarTermData S, float Time, float Orientation) {
    if (!SunLight) return;

    float HourAngle = (Time - 12.0f) * 15.0f;
    float RadLat = FMath::DegreesToRadians(C.Latitude);
    float RadDec = FMath::DegreesToRadians(S.Declination);
    float RadHA = FMath::DegreesToRadians(HourAngle);

    float SinAlt = FMath::Sin(RadLat) * FMath::Sin(RadDec) + FMath::Cos(RadLat) * FMath::Cos(RadDec) * FMath::Cos(RadHA);
    float Altitude = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(SinAlt, -1.0f, 1.0f)));
    float Azimuth = FMath::RadiansToDegrees(FMath::Atan2(-FMath::Sin(RadHA), FMath::Cos(RadDec)*FMath::Sin(RadLat) - FMath::Sin(RadDec)*FMath::Cos(RadLat)*FMath::Cos(RadHA)));

    SunLight->SetRelativeRotation(FRotator(-Altitude, Azimuth + Orientation, 0.0f));
    
    float IntensityMultiplier = (Altitude > 0) ? 1.0f : 0.05f; 
    SunLight->SetIntensity(W.IntensityLux * IntensityMultiplier);
    SunLight->SetLightColor(FLinearColor::MakeFromColorTemperature(W.Temperature));

    if (IsValid(SkyLight) && SkyLight->GetLightComponent()) {
       SkyLight->GetLightComponent()->SetIntensity(W.SkyIntensity * IntensityMultiplier);
       SkyLight->GetLightComponent()->RecaptureSky();
    }

    if (IsValid(Fog))
    {
       Fog->SetFogDensity(W.FogDensity);
       FLinearColor FogColor = FLinearColor::LerpUsingHSV(FLinearColor::White, FLinearColor::Gray, W.SkyIntensity);
       Fog->SetFogInscatteringColor(FogColor);
    }
}