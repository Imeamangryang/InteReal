#include "WeatherWidget.h"
#include "InteReal/Lighting/UIManager/WeatherUISubsystem.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/Button.h"
#include "InteReal/Struct/LightingDataStruct.h"


void UWeatherWidget::NativeConstruct() {
    Super::NativeConstruct();
    auto* Sub = GetGameInstance()->GetSubsystem<UWeatherUISubsystem>();
    if (!Sub) return;

    // 1. 초기 리스트 채우기
    for(auto& Name : Sub->CityMainTable->GetRowNames()) CB_CityMain->AddOption(Name.ToString());
    CB_Season->AddOption(TEXT("Spring")); CB_Season->AddOption(TEXT("Summer"));
    CB_Season->AddOption(TEXT("Autumn")); CB_Season->AddOption(TEXT("Winter"));
    for(auto& Name : Sub->WeatherTable->GetRowNames()) CB_Weather->AddOption(Name.ToString());

    // 2. 이벤트 연결
    CB_CityMain->OnSelectionChanged.AddDynamic(this, &UWeatherWidget::OnCityMainChanged);
    CB_Season->OnSelectionChanged.AddDynamic(this, &UWeatherWidget::OnSeasonChanged);
    
    // 나머지 옵션 변경 시 즉시 업데이트
    CB_Weather->OnSelectionChanged.AddDynamic(this, &UWeatherWidget::OnAnySelectionChanged);
    CB_CityDetail->OnSelectionChanged.AddDynamic(this, &UWeatherWidget::OnAnySelectionChanged);
    CB_Solar->OnSelectionChanged.AddDynamic(this, &UWeatherWidget::OnAnySelectionChanged);
    Slider_Time->OnValueChanged.AddDynamic(this, &UWeatherWidget::OnSliderChanged); 
    
    // 텍스트 박스 바인딩
    Text_Time->OnTextCommitted.AddDynamic(this, &UWeatherWidget::OnTimeTextChanged);
    
    /*// 1. 방향 리스트 채우기
    CB_Orientation->AddOption(TEXT("North"));
    CB_Orientation->AddOption(TEXT("East"));
    CB_Orientation->AddOption(TEXT("South"));
    CB_Orientation->AddOption(TEXT("West"));
    // 2. 이벤트 바인딩
    CB_Orientation->OnSelectionChanged.AddDynamic(this, &UWeatherWidget::OnOrientationChanged);*/
    
    // 버튼 이벤트 연결
    Btn_North->OnClicked.AddDynamic(this, &UWeatherWidget::OnNorthClicked);
    Btn_East->OnClicked.AddDynamic(this, &UWeatherWidget::OnEastClicked);
    Btn_South->OnClicked.AddDynamic(this, &UWeatherWidget::OnSouthClicked);
    Btn_West->OnClicked.AddDynamic(this, &UWeatherWidget::OnWestClicked);
    
    
    // 서브시스템의 현재 시간값 가져와서 초기 UI 동기화
    float CurrentTime = 12.0f; // 서브시스템 변수값
    Slider_Time->SetValue(CurrentTime / 24.0f);
    Text_Time->SetText(FText::FromString(FormatTime(CurrentTime)));
}

void UWeatherWidget::OnNorthClicked() { UpdateOrientation(0.0f); }
void UWeatherWidget::OnEastClicked()  { UpdateOrientation(90.0f); }
void UWeatherWidget::OnSouthClicked() { UpdateOrientation(180.0f); }
void UWeatherWidget::OnWestClicked()  { UpdateOrientation(270.0f); }
void UWeatherWidget::UpdateOrientation(float Angle)
{
    auto* Sub = GetGameInstance()->GetSubsystem<UWeatherUISubsystem>();
    if (Sub) {
        Sub->SetOrientation(Angle);
        Btn_North->SetBackgroundColor(Angle == 0.0f ? FLinearColor::Blue : FLinearColor::White);
        Btn_East->SetBackgroundColor(Angle == 90.0f ? FLinearColor::Blue : FLinearColor::White);
        Btn_South->SetBackgroundColor(Angle == 180.0f ? FLinearColor::Blue : FLinearColor::White);
        Btn_West->SetBackgroundColor(Angle == 270.0f ? FLinearColor::Blue : FLinearColor::White);
    }
}
// 광역시 선택 시 상세지역 갱신
void UWeatherWidget::OnCityMainChanged(FString Selected, ESelectInfo::Type Type) {
    CB_CityDetail->ClearOptions();
    auto* Sub = GetGameInstance()->GetSubsystem<UWeatherUISubsystem>();
    for(auto& Name : Sub->GetCityDetails(FName(*Selected))) CB_CityDetail->AddOption(Name);
    TriggerUpdate();
}

// 계절 선택 시 절기 갱신
void UWeatherWidget::OnSeasonChanged(FString Selected, ESelectInfo::Type Type) {
    CB_Solar->ClearOptions();
    auto* Sub = GetGameInstance()->GetSubsystem<UWeatherUISubsystem>();
    if (!Sub) return;

    // 계절별 절기 필터링 후 Name_KR 추출
    for (auto RowName : Sub->SolarTermTable->GetRowNames()) {
        auto* Data = Sub->SolarTermTable->FindRow<FSolarTermData>(RowName, TEXT(""));
        if (Data && Data->Season == Selected) {
            // RowName 대신 Name_KR을 콤보박스에 추가
            CB_Solar->AddOption(Data->Name_KR); 
        }
    }
    TriggerUpdate();
}

void UWeatherWidget::OnAnySelectionChanged(FString Selected, ESelectInfo::Type Type) 
{
    TriggerUpdate();
}

// 시간 문자열 생성 (00:00 포맷)
FString UWeatherWidget::FormatTime(float Hours) {
    int32 TotalMinutes = FMath::RoundToInt(Hours * 60.0f);
    int32 H = (TotalMinutes / 60) % 24;
    int32 M = TotalMinutes % 60;
    return FString::Printf(TEXT("%02d:%02d"), H, M);
}
// 슬라이더 이동 시 텍스트 업데이트
void UWeatherWidget::OnSliderChanged(float Value) {
    float CurrentHours = Value * 24.0f;
    
    // 텍스트 박스에 00:00 형식 적용
    Text_Time->SetText(FText::FromString(FormatTime(CurrentHours)));
    
    // 서브시스템 업데이트
    auto* Sub = GetGameInstance()->GetSubsystem<UWeatherUISubsystem>();
    if (Sub) Sub->SetTime(CurrentHours);
}

// 텍스트 입력 후 엔터/포커스 해제 시 슬라이더 업데이트
void UWeatherWidget::OnTimeTextChanged(const FText& Text, ETextCommit::Type CommitMethod) {
    if (CommitMethod == ETextCommit::OnEnter || CommitMethod == ETextCommit::OnUserMovedFocus) {
        FString Input = Text.ToString();
        float NewHours = 0.0f;

        // 콜론(:)이 포함된 경우 시간과 분으로 파싱
        if (Input.Contains(TEXT(":"))) {
            TArray<FString> Parts;
            Input.ParseIntoArray(Parts, TEXT(":"), true);
            if (Parts.Num() >= 2) {
                // 시간 + (분 / 60)
                NewHours = FCString::Atof(*Parts[0]) + (FCString::Atof(*Parts[1]) / 60.0f);
            }
        } else {
            // 숫자만 입력된 경우 (예: 14)
            NewHours = FCString::Atof(*Input);
        }

        NewHours = FMath::Clamp(NewHours, 0.0f, 24.0f);
        
        // 슬라이더 위치 동기화 (0.0 ~ 1.0)
        Slider_Time->SetValue(NewHours / 24.0f);
        
        // 텍스트 박스를 정규화된 00:00 포맷으로 다시 갱신 (사용자가 14:5라고 쳐도 14:05로 자동 보정)
        Text_Time->SetText(FText::FromString(FormatTime(NewHours)));
        
        // 서브시스템 업데이트
        auto* Sub = GetGameInstance()->GetSubsystem<UWeatherUISubsystem>();
        if (Sub) Sub->SetTime(NewHours);
    }
}
void UWeatherWidget::OnOrientationChanged(FString Selected, ESelectInfo::Type Type) {
    auto* Sub = GetGameInstance()->GetSubsystem<UWeatherUISubsystem>();
    if (!Sub) return;
    float Offset = (Selected == TEXT("South")) ? 180.0f : (Selected == TEXT("East") ? 90.0f : (Selected == TEXT("West") ? 270.0f : 0.0f));
    Sub->SetOrientation(Offset);
}

void UWeatherWidget::TriggerUpdate() {
    auto* Sub = GetGameInstance()->GetSubsystem<UWeatherUISubsystem>();
    if (!Sub) return;

    // 선택된 값들로 서브시스템 상태만 갱신
    Sub->SetCityDetail(FName(*CB_CityDetail->GetSelectedOption()));
    Sub->SetWeather(FName(*CB_Weather->GetSelectedOption()));
    Sub->SetSolar(Sub->GetSolarRowName(CB_Solar->GetSelectedOption()));
}
