#include "EnvironmentPanel.h"
#include "InteReal/Lighting/UIManager/WeatherUISubsystem.h" 
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Image.h"
#include "InteReal/Master/UI/Components/BaseComboBox.h"
#include "InteReal/Master/UI/Components/BaseSlider.h"
#include "InteReal/Master/UI/Components/SelectableButton.h"
#include "InteReal/Master/UI/DesignTemplate/InteRealThemeData.h"

UWeatherUISubsystem* UEnvironmentPanel::GetWeatherSubsystem() const {
    return GetGameInstance() ? GetGameInstance()->GetSubsystem<UWeatherUISubsystem>() : nullptr;
}

void UEnvironmentPanel::NativePreConstruct() {
    Super::NativePreConstruct();
    if (!ThemeData) return;

    if (Border_Background) {
       FSlateBrush BgBrush;
       BgBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
       BgBrush.TintColor = FSlateColor(ThemeData->Card_BG_White);
       BgBrush.OutlineSettings.Color = FSlateColor(ThemeData->Card_Border);
       BgBrush.OutlineSettings.Width = 1.0f;
       BgBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
       BgBrush.OutlineSettings.CornerRadii = FVector4(12.0f, 12.0f, 12.0f, 12.0f);
       Border_Background->SetBrush(BgBrush);
    }
    if (TitleText) TitleText->SetColorAndOpacity(FSlateColor(ThemeData->Body_Text));
    
    const FSlateColor SubtitleColor = FSlateColor(ThemeData->Body_Text);
    if (SubTitle_Time) SubTitle_Time->SetColorAndOpacity(SubtitleColor);
    if (SubTitle_Season) SubTitle_Season->SetColorAndOpacity(SubtitleColor);
    if (SubTitle_SolarTerm) SubTitle_SolarTerm->SetColorAndOpacity(SubtitleColor);
    if (SubTitle_Orientation) SubTitle_Orientation->SetColorAndOpacity(SubtitleColor);
    if (SubTitle_Weather) SubTitle_Weather->SetColorAndOpacity(SubtitleColor);
}

void UEnvironmentPanel::NativeConstruct() {
    Super::NativeConstruct();

    if (Btn_TogglePanel) Btn_TogglePanel->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleTogglePanelClicked);
    if (Btn_TabEnvControl && Btn_TabEnvControl->Btn_Main) Btn_TabEnvControl->Btn_Main->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleEnvControlTabClicked);
    if (Btn_TabLocationSettings && Btn_TabLocationSettings->Btn_Main) Btn_TabLocationSettings->Btn_Main->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleLocationSettingsTabClicked);

    if (Slider_Time) {
       Slider_Time->OnBaseValueChanged.AddUniqueDynamic(this, &UEnvironmentPanel::HandleTimeChanged);
       HandleTimeChanged(Slider_Time->Slider_Main ? Slider_Time->Slider_Main->GetValue() : 0.4f);
    }

    if (Btn_Spring && Btn_Spring->Btn_Main) Btn_Spring->Btn_Main->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleSpringClicked);
    if (Btn_Summer && Btn_Summer->Btn_Main) Btn_Summer->Btn_Main->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleSummerClicked);
    if (Btn_Autumn && Btn_Autumn->Btn_Main) Btn_Autumn->Btn_Main->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleAutumnClicked);
    if (Btn_Winter && Btn_Winter->Btn_Main) Btn_Winter->Btn_Main->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleWinterClicked);

    if (ComboBox_SolarTerm) ComboBox_SolarTerm->OnBaseSelectionChanged.AddUniqueDynamic(this, &UEnvironmentPanel::HandleSolarTermChanged);

    if (Btn_Clear && Btn_Clear->Btn_Main) Btn_Clear->Btn_Main->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleClearClicked);
    if (Btn_Cloudy && Btn_Cloudy->Btn_Main) Btn_Cloudy->Btn_Main->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleCloudyClicked);
    if (Btn_Rainy && Btn_Rainy->Btn_Main) Btn_Rainy->Btn_Main->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleRainyClicked);
    if (Btn_Foggy && Btn_Foggy->Btn_Main) Btn_Foggy->Btn_Main->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleFoggyClicked);
    if (Btn_Snowy && Btn_Snowy->Btn_Main) Btn_Snowy->Btn_Main->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleSnowyClicked);
    if (Btn_Stormy && Btn_Stormy->Btn_Main) Btn_Stormy->Btn_Main->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleStormyClicked);
    
    if (Btn_North) Btn_North->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleNorthClicked);
    if (Btn_East)  Btn_East->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleEastClicked);
    if (Btn_South) Btn_South->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleSouthClicked);
    if (Btn_West)  Btn_West->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleWestClicked);
    
    if (Btn_PlayTime) Btn_PlayTime->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandlePlayClicked);
    
    SetActiveMainTab(true);
    UpdateSeasonGroup(Btn_Spring);
    UpdateWeatherGroup(Btn_Clear);
    
    // 서브시스템의 기본값을 UI에 적용하는 트리거
    if (auto* S = GetWeatherSubsystem()) {
        S->ForceUpdate(); 
    }
}

void UEnvironmentPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime) 
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    // 부드러운 회전 로직
    if (FMath::Abs(CurrentAngle - TargetAngle) > 0.1f)
    {
        CurrentAngle = FMath::FInterpTo(CurrentAngle, TargetAngle, InDeltaTime, RotationSpeed);
        
        if (Img_Orientation) 
        {
            FWidgetTransform T;
            T.Angle = CurrentAngle; 
            Img_Orientation->SetRenderTransform(T);
        }
    }
    else
    {
        // 멈췄을 때만 360도 범위로 정규화하여 다음 계산을 준비
        CurrentAngle = FMath::Fmod(CurrentAngle, 360.0f);
        TargetAngle = FMath::Fmod(TargetAngle, 360.0f);
    }
    
    
    if (bIsPlaying) 
    {
        if (auto* Sub = GetWeatherSubsystem()) 
        {
            // 1. 누적 시간 증가
            AccumulatedTime += (InDeltaTime * PlaySpeed);
          
            // 2. 24시간(경과 시간) 경과 체크
            if (AccumulatedTime >= 24.0f) 
            {
                bIsPlaying = false; // 재생 중단
                // 시작 시간으로 복귀 (혹은 멈춘 시간 유지)
                Sub->SetTime(PlayStartTime);
                UE_LOG(LogTemp, Warning, TEXT("24시간 재생 완료, 정지합니다."));
            }
            else
            {
                // 3. 현재 시간 계산: 시작 시간 + 누적 시간
                // FMath::Fmod를 사용하여 24시 넘어가면 0시부터 다시 시작하도록 함
                float NewTime = FMath::Fmod(PlayStartTime + AccumulatedTime, 24.0f);
          
                // 4. 서브시스템에 시간 업데이트
                Sub->SetTime(NewTime);
          
                // 5. UI 업데이트
                if (Slider_Time && Slider_Time->Slider_Main) 
                    Slider_Time->Slider_Main->SetValue(NewTime / 24.0f);
                
                HandleTimeChanged(NewTime / 24.0f);
            }
        }
    }
}

void UEnvironmentPanel::HandleTimeChanged(float NewValue) {
    const float Hours = NewValue * 24.0f;
    if (Txt_CurrentTime) Txt_CurrentTime->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"), (int32)Hours, (int32)((Hours - (int32)Hours) * 60))));
    if (auto* Sub = GetWeatherSubsystem()) Sub->SetTime(Hours);
}

void UEnvironmentPanel::HandlePlayClicked()
{
    bIsPlaying = !bIsPlaying;
    
    if (bIsPlaying) 
    {
        // 재생을 시작할 때, 현재 시스템 시간을 저장
        if (auto* Sub = GetWeatherSubsystem())
        {
            PlayStartTime = Sub->GetCurrentTime();
            AccumulatedTime = 0.0f; // 누적 시간 0으로 초기화
        }
    }
}
void UEnvironmentPanel::HandleTogglePanelClicked() { bIsPanelExpanded = !bIsPanelExpanded; BP_OnPanelStateChanged(bIsPanelExpanded); }
void UEnvironmentPanel::HandleEnvControlTabClicked() { SetActiveMainTab(true); }
void UEnvironmentPanel::HandleLocationSettingsTabClicked() { SetActiveMainTab(false); }

void UEnvironmentPanel::HandleSpringClicked() { UpdateSeasonGroup(Btn_Spring); if(auto* S = GetWeatherSubsystem()) S->SetSolar(S->GetSolarRowName(TEXT("입춘"))); }
void UEnvironmentPanel::HandleSummerClicked() { UpdateSeasonGroup(Btn_Summer); if(auto* S = GetWeatherSubsystem()) S->SetSolar(S->GetSolarRowName(TEXT("입하"))); }
void UEnvironmentPanel::HandleAutumnClicked() { UpdateSeasonGroup(Btn_Autumn); if(auto* S = GetWeatherSubsystem()) S->SetSolar(S->GetSolarRowName(TEXT("입추"))); }
void UEnvironmentPanel::HandleWinterClicked() { UpdateSeasonGroup(Btn_Winter); if(auto* S = GetWeatherSubsystem()) S->SetSolar(S->GetSolarRowName(TEXT("입동"))); }

void UEnvironmentPanel::HandleClearClicked()  { UE_LOG(LogTemp, Warning, TEXT("Sunny Button Clicked!")); UpdateWeatherGroup(Btn_Clear);  if(auto* S = GetWeatherSubsystem()) S->SetWeather(FName("Clear")); }
void UEnvironmentPanel::HandleCloudyClicked() { UpdateWeatherGroup(Btn_Cloudy); if(auto* S = GetWeatherSubsystem()) S->SetWeather(FName("Cloudy")); }
void UEnvironmentPanel::HandleRainyClicked()  { UpdateWeatherGroup(Btn_Rainy);  if(auto* S = GetWeatherSubsystem()) S->SetWeather(FName("Rainy")); }
void UEnvironmentPanel::HandleFoggyClicked()  { UpdateWeatherGroup(Btn_Foggy);  if(auto* S = GetWeatherSubsystem()) S->SetWeather(FName("Foggy")); }
void UEnvironmentPanel::HandleSnowyClicked()  { UpdateWeatherGroup(Btn_Snowy);  if(auto* S = GetWeatherSubsystem()) S->SetWeather(FName("Snowy")); }
void UEnvironmentPanel::HandleStormyClicked() { UpdateWeatherGroup(Btn_Stormy); if(auto* S = GetWeatherSubsystem()) S->SetWeather(FName("Stormy")); }

void UEnvironmentPanel::HandleNorthClicked() { UpdateOrientationUI(0.0f); }
void UEnvironmentPanel::HandleEastClicked()  { UpdateOrientationUI(90.0f); }
void UEnvironmentPanel::HandleSouthClicked() { UpdateOrientationUI(180.0f); }
void UEnvironmentPanel::HandleWestClicked()  { UpdateOrientationUI(270.0f); }

void UEnvironmentPanel::UpdateOrientationUI(float Angle) {
    // 1. 현재 0~360 범위로 정규화된 각도 사용 (CurrentAngle을 360으로 나눈 나머지)
    float CurrentNormalized = FMath::Fmod(CurrentAngle, 360.0f);
    if (CurrentNormalized < 0.0f) CurrentNormalized += 360.0f;

    // 2. 시계 방향 거리 계산
    // 목표가 현재보다 크면 그대로 차이, 작으면 360도에서 차이만큼 더함
    float DeltaAngle = Angle - CurrentNormalized;
    
    if (DeltaAngle <= 0.0f)
    {
        DeltaAngle += 360.0f;
    }

    // 3. 만약 이미 목표와 거의 같다면(예: 1도 이내) 회전하지 않음
    if (DeltaAngle < 1.0f)
    {
        return;
    }
    
    // 4. 새로운 목표 설정 (현재 목표치가 아닌, 현재 위치부터 회전 시작)
    TargetAngle = CurrentAngle + DeltaAngle;
    
    if (auto* Sub = GetWeatherSubsystem()) Sub->SetOrientation(Angle);
}

void UEnvironmentPanel::UpdateSeasonGroup(TObjectPtr<USelectableButton> SB) {
    if(!Btn_Spring || !Btn_Summer || !Btn_Autumn || !Btn_Winter) return;
    Btn_Spring->SetIsSelected(Btn_Spring == SB); Btn_Summer->SetIsSelected(Btn_Summer == SB);
    Btn_Autumn->SetIsSelected(Btn_Autumn == SB); Btn_Winter->SetIsSelected(Btn_Winter == SB);
}

void UEnvironmentPanel::UpdateWeatherGroup(TObjectPtr<USelectableButton> SB) {
    if(!Btn_Clear || !Btn_Cloudy || !Btn_Rainy || !Btn_Foggy || !Btn_Snowy || !Btn_Stormy) return;
    Btn_Clear->SetIsSelected(Btn_Clear == SB); Btn_Cloudy->SetIsSelected(Btn_Cloudy == SB);
    Btn_Rainy->SetIsSelected(Btn_Rainy == SB); Btn_Foggy->SetIsSelected(Btn_Foggy == SB);
    Btn_Snowy->SetIsSelected(Btn_Snowy == SB); Btn_Stormy->SetIsSelected(Btn_Stormy == SB);
}

void UEnvironmentPanel::SetActiveMainTab(bool bEnv) { if(Btn_TabEnvControl) Btn_TabEnvControl->SetIsSelected(bEnv); if(Btn_TabLocationSettings) Btn_TabLocationSettings->SetIsSelected(!bEnv); if(ContentSwitcher) ContentSwitcher->SetActiveWidgetIndex(bEnv ? 0 : 1); }
void UEnvironmentPanel::HandleSolarTermChanged(FString I, ESelectInfo::Type T) {}