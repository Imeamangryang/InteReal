#include "EnvironmentPanel.h"
#include "InteReal/Lighting/UIManager/WeatherUISubsystem.h" 
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "InteReal/Master/UI/Components/BaseComboBox.h"
#include "InteReal/Master/UI/Components/BaseSlider.h"
#include "InteReal/Master/UI/Components/SelectableButton.h"
#include "InteReal/Master/UI/DesignTemplate/InteRealThemeData.h"
#include "Components/EditableTextBox.h"

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
    if (SubTitle_City) SubTitle_Time->SetColorAndOpacity(SubtitleColor);
    if (SubTitle_District) SubTitle_Time->SetColorAndOpacity(SubtitleColor);
    if (SubTitle_Time) SubTitle_Time->SetColorAndOpacity(SubtitleColor);
    if (SubTitle_Season) SubTitle_Season->SetColorAndOpacity(SubtitleColor);
    if (SubTitle_SolarTerm) SubTitle_SolarTerm->SetColorAndOpacity(SubtitleColor);
    if (SubTitle_Orientation) SubTitle_Orientation->SetColorAndOpacity(SubtitleColor);
    if (SubTitle_Weather) SubTitle_Weather->SetColorAndOpacity(SubtitleColor);
}

void UEnvironmentPanel::NativeConstruct() {
    Super::NativeConstruct();
    
    if (Btn_TogglePanel) Btn_TogglePanel->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleTogglePanelClicked);
    
    // 지역 콤보박스 바인딩
    if (ComboBox_City && ComboBox_City->ComboBox_Main) {
        ComboBox_City->ComboBox_Main->ClearOptions();
        auto* Sub = GetWeatherSubsystem();
        if (Sub && Sub->CityMainTable) {
            for (auto RowName : Sub->CityMainTable->GetRowNames()) {
                auto* Data = Sub->CityMainTable->FindRow<FCityMainData>(RowName, TEXT(""));
                if (Data) ComboBox_City->ComboBox_Main->AddOption(Data->Name_KR);
            }
        }
    }
    if (ComboBox_City) ComboBox_City->OnBaseSelectionChanged.AddUniqueDynamic(this, &UEnvironmentPanel::HandleCityChanged);
    if (ComboBox_District) ComboBox_District->OnBaseSelectionChanged.AddUniqueDynamic(this, &UEnvironmentPanel::HandleDistrictChanged);
    
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
    
    if (Btn_Play && Btn_Play->Btn_Main) 
        Btn_Play->Btn_Main->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandlePlayClicked);
    
    // 2. 속도 콤보박스 초기화
    if (ComboBox_PlaySpeed && ComboBox_PlaySpeed->ComboBox_Main) {
        ComboBox_PlaySpeed->ComboBox_Main->ClearOptions();
        ComboBox_PlaySpeed->ComboBox_Main->AddOption(TEXT("0.5x"));
        ComboBox_PlaySpeed->ComboBox_Main->AddOption(TEXT("1.0x"));
        ComboBox_PlaySpeed->ComboBox_Main->AddOption(TEXT("2.0x"));
        ComboBox_PlaySpeed->ComboBox_Main->SetSelectedOption(TEXT("1.0x"));
        
        ComboBox_PlaySpeed->OnBaseSelectionChanged.AddUniqueDynamic(this, &UEnvironmentPanel::HandlePlaySpeedChanged);
    }
    
    UpdateSeasonGroup(Btn_Spring);
    UpdateWeatherGroup(Btn_Clear);
    
    // 8방향 버튼 연결
    if (Btn_North && Btn_North->Btn_Main) Btn_North->Btn_Main->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleNorthClicked);
    if (Btn_NorthEast && Btn_NorthEast->Btn_Main) Btn_NorthEast->Btn_Main->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleNorthEastClicked);
    if (Btn_East && Btn_East->Btn_Main) Btn_East->Btn_Main->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleEastClicked);
    if (Btn_SouthEast && Btn_SouthEast->Btn_Main) Btn_SouthEast->Btn_Main->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleSouthEastClicked);
    if (Btn_South && Btn_South->Btn_Main) Btn_South->Btn_Main->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleSouthClicked);
    if (Btn_SouthWest && Btn_SouthWest->Btn_Main) Btn_SouthWest->Btn_Main->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleSouthWestClicked);
    if (Btn_West && Btn_West->Btn_Main) Btn_West->Btn_Main->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleWestClicked);
    if (Btn_NorthWest && Btn_NorthWest->Btn_Main) Btn_NorthWest->Btn_Main->OnClicked.AddUniqueDynamic(this, &UEnvironmentPanel::HandleNorthWestClicked);

    if (Edit_Time) { Edit_Time->OnTextCommitted.AddUniqueDynamic(this, &UEnvironmentPanel::HandleTimeTextCommitted); }
    
    // 기본 초기화
    UpdateOrientationGroup(Btn_North);
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
    if (Edit_Time) {
        // 사용자가 입력 중일 때 텍스트가 덮어씌워지지 않도록 포커스 여부 확인
        if (!Edit_Time->HasKeyboardFocus()) {
            Edit_Time->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"), (int32)Hours, (int32)((Hours - (int32)Hours) * 60))));
        }
    }
    if (auto* Sub = GetWeatherSubsystem()) Sub->SetTime(Hours);
}

// 텍스트 박스에서 직접 입력 후 엔터/포커스 해제 시 호출
void UEnvironmentPanel::HandleTimeTextCommitted(const FText& Text, ETextCommit::Type CommitMethod) {
    if (CommitMethod == ETextCommit::OnEnter || CommitMethod == ETextCommit::OnUserMovedFocus) {
        FString TimeStr = Text.ToString();
        float NewHours = 0.0f;

        // "HH:MM" 형식 파싱 (콜론 기준)
        if (TimeStr.Contains(TEXT(":"))) {
            TArray<FString> Parts;
            TimeStr.ParseIntoArray(Parts, TEXT(":"), true);
            if (Parts.Num() >= 2) {
                NewHours = FCString::Atof(*Parts[0]) + (FCString::Atof(*Parts[1]) / 60.0f);
            }
        } else {
            NewHours = FCString::Atof(*TimeStr);
        }

        NewHours = FMath::Clamp(NewHours, 0.0f, 24.0f);

        // 슬라이더 위치 동기화
        if (Slider_Time && Slider_Time->Slider_Main) {
            Slider_Time->Slider_Main->SetValue(NewHours / 24.0f);
        }

        // 서브시스템 업데이트
        if (auto* Sub = GetWeatherSubsystem()) {
            Sub->SetTime(NewHours);
        }
        
        // 텍스트를 정확한 포맷으로 다시 갱신
        Edit_Time->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"), (int32)NewHours, (int32)((NewHours - (int32)NewHours) * 60))));
    }
}
void UEnvironmentPanel::HandlePlayClicked()
{
    bIsPlaying = !bIsPlaying;
    
    // 재생 버튼의 토글 상태 시각화 (SelectableButton인 경우)
    if (Btn_Play) Btn_Play->SetIsSelected(bIsPlaying);
    
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


void UEnvironmentPanel::HandleSpringClicked() { 
    UpdateSeasonGroup(Btn_Spring); 
    RefreshSolarComboBox(TEXT("Spring"), TEXT("춘분")); 
    if(auto* S = GetWeatherSubsystem()) S->SetSolar(S->GetSolarRowName(TEXT("춘분"))); 
}

void UEnvironmentPanel::HandleSummerClicked() { 
    UpdateSeasonGroup(Btn_Summer); 
    RefreshSolarComboBox(TEXT("Summer"), TEXT("하지")); 
    if(auto* S = GetWeatherSubsystem()) S->SetSolar(S->GetSolarRowName(TEXT("하지"))); 
}

void UEnvironmentPanel::HandleAutumnClicked() { 
    UpdateSeasonGroup(Btn_Autumn); 
    RefreshSolarComboBox(TEXT("Autumn"), TEXT("추분")); 
    if(auto* S = GetWeatherSubsystem()) S->SetSolar(S->GetSolarRowName(TEXT("추분"))); 
}

void UEnvironmentPanel::HandleWinterClicked() { 
    UpdateSeasonGroup(Btn_Winter); 
    RefreshSolarComboBox(TEXT("Winter"), TEXT("동지")); 
    if(auto* S = GetWeatherSubsystem()) S->SetSolar(S->GetSolarRowName(TEXT("동지"))); 
}

void UEnvironmentPanel::HandleClearClicked()  { UpdateWeatherGroup(Btn_Clear);  if(auto* S = GetWeatherSubsystem()) S->SetWeather(FName("Clear")); }
void UEnvironmentPanel::HandleCloudyClicked() { UpdateWeatherGroup(Btn_Cloudy); if(auto* S = GetWeatherSubsystem()) S->SetWeather(FName("Cloudy")); }
void UEnvironmentPanel::HandleRainyClicked()  { UpdateWeatherGroup(Btn_Rainy);  if(auto* S = GetWeatherSubsystem()) S->SetWeather(FName("Rainy")); }
void UEnvironmentPanel::HandleFoggyClicked()  { UpdateWeatherGroup(Btn_Foggy);  if(auto* S = GetWeatherSubsystem()) S->SetWeather(FName("Foggy")); }
void UEnvironmentPanel::HandleSnowyClicked()  { UpdateWeatherGroup(Btn_Snowy);  if(auto* S = GetWeatherSubsystem()) S->SetWeather(FName("Snowy")); }
void UEnvironmentPanel::HandleStormyClicked() { UpdateWeatherGroup(Btn_Stormy); if(auto* S = GetWeatherSubsystem()) S->SetWeather(FName("Stormy")); }

void UEnvironmentPanel::HandleNorthClicked()     { UpdateOrientationUI(0.0f);   UpdateOrientationGroup(Btn_North); }
void UEnvironmentPanel::HandleNorthEastClicked() { UpdateOrientationUI(45.0f);  UpdateOrientationGroup(Btn_NorthEast); }
void UEnvironmentPanel::HandleEastClicked()      { UpdateOrientationUI(90.0f);  UpdateOrientationGroup(Btn_East); }
void UEnvironmentPanel::HandleSouthEastClicked() { UpdateOrientationUI(135.0f); UpdateOrientationGroup(Btn_SouthEast); }
void UEnvironmentPanel::HandleSouthClicked()     { UpdateOrientationUI(180.0f); UpdateOrientationGroup(Btn_South); }
void UEnvironmentPanel::HandleSouthWestClicked() { UpdateOrientationUI(225.0f); UpdateOrientationGroup(Btn_SouthWest); }
void UEnvironmentPanel::HandleWestClicked()      { UpdateOrientationUI(270.0f); UpdateOrientationGroup(Btn_West); }
void UEnvironmentPanel::HandleNorthWestClicked() { UpdateOrientationUI(315.0f); UpdateOrientationGroup(Btn_NorthWest); }

void UEnvironmentPanel::UpdateOrientationGroup(TObjectPtr<USelectableButton> SB) {
    TArray<TObjectPtr<USelectableButton>> Buttons = { 
        Btn_North, Btn_NorthEast, Btn_East, Btn_SouthEast, 
        Btn_South, Btn_SouthWest, Btn_West, Btn_NorthWest 
    };
    for (auto& Btn : Buttons) {
        if (Btn) 
        {
            Btn->SetIsSelected(Btn == SB);
        }
    }
}

void UEnvironmentPanel::UpdateOrientationUI(float Angle) {
    // 1. 현재 각도를 0~360 범위로 정규화
    float CurrentNormalized = FMath::Fmod(CurrentAngle, 360.0f);
    if (CurrentNormalized < 0.0f) CurrentNormalized += 360.0f;

    // 2. 현재 각도와 목표 각도의 차이(짧은 경로) 계산
    float DeltaAngle = Angle - CurrentNormalized;

    // 3. 차이가 -180보다 작거나 180보다 크면 회전 방향을 반대로 조절하여 최단 거리로 이동
    if (DeltaAngle > 180.0f)
    {
        DeltaAngle -= 360.0f;
    }
    else if (DeltaAngle < -180.0f)
    {
        DeltaAngle += 360.0f;
    }

    // 4. 아주 작은 차이(예: 0.5도 미만)면 회전하지 않음 (공회전 방지)
    if (FMath::Abs(DeltaAngle) < 0.5f)
    {
        return;
    }
    
    // 5. 현재 위치에서 계산된 만큼만 더해서 Target 설정
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

void UEnvironmentPanel::HandleSolarTermChanged(FString SelectedItem, ESelectInfo::Type SelectionType) {
    if (SelectionType == ESelectInfo::Direct) return; // 초기화 시점에는 무시
    
    auto* Sub = GetWeatherSubsystem();
    if (Sub) {
        // 1. 선택된 절기 이름(KR)을 RowName으로 변환
        FName SolarRowName = Sub->GetSolarRowName(SelectedItem);
        
        // 2. 서브시스템에 절기 변경 전달
        Sub->SetSolar(SolarRowName);
        
        UE_LOG(LogTemp, Warning, TEXT("절기 변경됨: %s -> %s"), *SelectedItem, *SolarRowName.ToString());
    }
}

void UEnvironmentPanel::RefreshSolarComboBox(FString Season, FString DefaultTerm) {
    if (!ComboBox_SolarTerm || !ComboBox_SolarTerm->ComboBox_Main) return;
    
    ComboBox_SolarTerm->ComboBox_Main->ClearOptions(); 
    
    auto* Sub = GetWeatherSubsystem();
    if (Sub) {
        TArray<FString> SolarTerms = Sub->GetSolarTermsBySeason(Season);
        
        for (const FString& Term : SolarTerms) {
            ComboBox_SolarTerm->ComboBox_Main->AddOption(Term);
        }

        // DefaultTerm이 지정되었다면 해당 옵션을 선택, 아니면 0번째 선택
        if (!DefaultTerm.IsEmpty()) {
            ComboBox_SolarTerm->ComboBox_Main->SetSelectedOption(DefaultTerm);
            HandleSolarTermChanged(DefaultTerm, ESelectInfo::Direct);
        } else if (SolarTerms.Num() > 0) {
            ComboBox_SolarTerm->ComboBox_Main->SetSelectedOption(SolarTerms[0]);
            HandleSolarTermChanged(SolarTerms[0], ESelectInfo::Direct);
        }
    }
}

void UEnvironmentPanel::HandlePlaySpeedChanged(FString SelectedItem, ESelectInfo::Type SelectionType) {
    if (SelectedItem == TEXT("0.5x")) PlaySpeed = 0.5f;
    else if (SelectedItem == TEXT("1.0x")) PlaySpeed = 1.0f;
    else if (SelectedItem == TEXT("2.0x")) PlaySpeed = 2.0f;
    
    UE_LOG(LogTemp, Log, TEXT("재생 속도 변경: %f"), PlaySpeed);
}
void UEnvironmentPanel::HandleCityChanged(FString SelectedItem, ESelectInfo::Type SelectionType) {
    if (SelectionType == ESelectInfo::Direct) return;

    auto* Sub = GetWeatherSubsystem();
    if (!Sub) return;

    // 1. 선택된 한글(Name_KR)로 부모 RowName 찾기
    FName MainRowName = NAME_None;
    for (auto RowName : Sub->CityMainTable->GetRowNames()) {
        auto* Data = Sub->CityMainTable->FindRow<FCityMainData>(RowName, TEXT(""));
        if (Data && Data->Name_KR == SelectedItem) {
            MainRowName = RowName;
            break;
        }
    }

    // 2. 부모 RowName으로 하위 리스트(한글) 가져오기
    TArray<FString> Districts = Sub->GetCityDetails(MainRowName);

    if (ComboBox_District && ComboBox_District->ComboBox_Main) {
        ComboBox_District->ComboBox_Main->ClearOptions();
        for (const FString& D : Districts) {
            ComboBox_District->ComboBox_Main->AddOption(D);
        }
        
        // 첫 항목 자동 선택 및 처리
        if (Districts.Num() > 0) {
            ComboBox_District->ComboBox_Main->SetSelectedOption(Districts[0]);
            HandleDistrictChanged(Districts[0], ESelectInfo::Direct);
        }
    }
}

void UEnvironmentPanel::HandleDistrictChanged(FString SelectedItem, ESelectInfo::Type SelectionType) {
    if (SelectionType == ESelectInfo::Direct) return;

    auto* Sub = GetWeatherSubsystem();
    if (!Sub) return;

    // 1. 선택된 한글(SelectedItem)을 이용해 해당 지역의 RowName 역추적
    FName DetailRowName = NAME_None;
    for (auto RowName : Sub->CityDetailTable->GetRowNames()) {
        auto* Data = Sub->CityDetailTable->FindRow<FCityDetailData>(RowName, TEXT(""));
        if (Data && Data->Name_KR == SelectedItem) {
            DetailRowName = RowName;
            break;
        }
    }

    // 2. 찾은 RowName("Suwon-si" 등)을 서브시스템에 전달
    if (!DetailRowName.IsNone()) {
        Sub->SetCityDetail(DetailRowName);
    }
}