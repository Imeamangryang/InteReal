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
    
    // 옵션 변경 시 즉시 업데이트
    CB_CityDetail->OnSelectionChanged.AddDynamic(this, &UWeatherWidget::OnAnySelectionChanged);
    CB_Solar->OnSelectionChanged.AddDynamic(this, &UWeatherWidget::OnAnySelectionChanged);
    Slider_Time->OnValueChanged.AddDynamic(this, &UWeatherWidget::OnSliderChanged); 
    
    // 날씨 버튼 연결
    Btn_Clear->OnClicked.AddDynamic(this, &UWeatherWidget::OnClearClicked);
    Btn_Cloudy->OnClicked.AddDynamic(this, &UWeatherWidget::OnCloudyClicked);
    Btn_Rainy->OnClicked.AddDynamic(this, &UWeatherWidget::OnRainyClicked);
    Btn_Snowy->OnClicked.AddDynamic(this, &UWeatherWidget::OnSnowyClicked);
    Btn_Foggy->OnClicked.AddDynamic(this, &UWeatherWidget::OnFoggyClicked);
    Btn_Stormy->OnClicked.AddDynamic(this, &UWeatherWidget::OnStormyClicked);
    
    // 버튼 이벤트 연결
    Btn_Spring->OnClicked.AddDynamic(this, &UWeatherWidget::OnSpringClicked);
    Btn_Summer->OnClicked.AddDynamic(this, &UWeatherWidget::OnSummerClicked);
    Btn_Autumn->OnClicked.AddDynamic(this, &UWeatherWidget::OnAutumnClicked);
    Btn_Winter->OnClicked.AddDynamic(this, &UWeatherWidget::OnWinterClicked);
    
    // 텍스트 박스 바인딩
    Text_Time->OnTextCommitted.AddDynamic(this, &UWeatherWidget::OnTimeTextChanged);
    
    // 버튼 이벤트 연결
    Btn_North->OnClicked.AddDynamic(this, &UWeatherWidget::OnNorthClicked);
    Btn_East->OnClicked.AddDynamic(this, &UWeatherWidget::OnEastClicked);
    Btn_South->OnClicked.AddDynamic(this, &UWeatherWidget::OnSouthClicked);
    Btn_West->OnClicked.AddDynamic(this, &UWeatherWidget::OnWestClicked);
    
    // 도시 버튼 17개 이벤트 연결
    Btn_Gyeonggi->OnClicked.AddDynamic(this, &UWeatherWidget::OnGyeonggiClicked);
    Btn_Gyeongnam->OnClicked.AddDynamic(this, &UWeatherWidget::OnGyeongnamClicked);
    Btn_Gyeongbuk->OnClicked.AddDynamic(this, &UWeatherWidget::OnGyeongbukClicked);
    Btn_Chungnam->OnClicked.AddDynamic(this, &UWeatherWidget::OnChungnamClicked);
    Btn_Jeonnam->OnClicked.AddDynamic(this, &UWeatherWidget::OnJeonnamClicked);
    Btn_Jeonbuk->OnClicked.AddDynamic(this, &UWeatherWidget::OnJeonbukClicked);
    Btn_Chungbuk->OnClicked.AddDynamic(this, &UWeatherWidget::OnChungbukClicked);
    Btn_Gangwon->OnClicked.AddDynamic(this, &UWeatherWidget::OnGangwonClicked);
    Btn_Jeju->OnClicked.AddDynamic(this, &UWeatherWidget::OnJejuClicked);
    Btn_Busan->OnClicked.AddDynamic(this, &UWeatherWidget::OnBusanClicked);
    Btn_Incheon->OnClicked.AddDynamic(this, &UWeatherWidget::OnIncheonClicked);
    Btn_Daegu->OnClicked.AddDynamic(this, &UWeatherWidget::OnDaeguClicked);
    Btn_Daejeon->OnClicked.AddDynamic(this, &UWeatherWidget::OnDaejeonClicked);
    Btn_Gwangju->OnClicked.AddDynamic(this, &UWeatherWidget::OnGwangjuClicked);
    Btn_Ulsan->OnClicked.AddDynamic(this, &UWeatherWidget::OnUlsanClicked);
    Btn_Seoul->OnClicked.AddDynamic(this, &UWeatherWidget::OnSeoulClicked);
    Btn_Sejong->OnClicked.AddDynamic(this, &UWeatherWidget::OnSejongClicked);
    
    // 서브시스템의 현재 시간값 가져와서 초기 UI 동기화
    float CurrentTime = 12.0f; // 서브시스템 변수값
    Slider_Time->SetValue(CurrentTime / 24.0f);
    Text_Time->SetText(FText::FromString(FormatTime(CurrentTime)));
    
    // 새 버튼/콤보박스 연결
    Btn_PlayTime->OnClicked.AddDynamic(this, &UWeatherWidget::OnPlayClicked);
    CB_Speed->OnSelectionChanged.AddDynamic(this, &UWeatherWidget::OnSpeedChanged);
    
    CB_Speed->AddOption(TEXT("1"));
    CB_Speed->AddOption(TEXT("2"));
    CB_Speed->AddOption(TEXT("3"));
    CB_Speed->SetSelectedOption(TEXT("1"));
    
}

// 날씨 통합 로직
void UWeatherWidget::HandleWeatherUpdate(FName RowName, UButton* ClickedButton) {
    // 1. 버튼 색상 초기화
    TArray<UButton*> AllBtns = { Btn_Clear, Btn_Cloudy, Btn_Rainy, Btn_Snowy, Btn_Foggy, Btn_Stormy };
    for(auto* Btn : AllBtns) Btn->SetBackgroundColor(FLinearColor::White);
    
    // 2. 선택된 버튼 색상 변경
    if(ClickedButton) ClickedButton->SetBackgroundColor(FLinearColor::Blue);

    // 3. 서브시스템 업데이트
    auto* Sub = GetGameInstance()->GetSubsystem<UWeatherUISubsystem>();
    if (Sub) {
        Sub->SetWeather(RowName);
        TriggerUpdate();
    }
}

// 날씨 버튼 이벤트
void UWeatherWidget::OnClearClicked()    { HandleWeatherUpdate(FName("Clear"), Btn_Clear); }
void UWeatherWidget::OnCloudyClicked()   { HandleWeatherUpdate(FName("Cloudy"), Btn_Cloudy); }
void UWeatherWidget::OnRainyClicked() { HandleWeatherUpdate(FName("Rainy"), Btn_Rainy); }
void UWeatherWidget::OnSnowyClicked()     { HandleWeatherUpdate(FName("Snowy"), Btn_Snowy); }
void UWeatherWidget::OnFoggyClicked()  { HandleWeatherUpdate(FName("Foggy"), Btn_Foggy); }
void UWeatherWidget::OnStormyClicked()     { HandleWeatherUpdate(FName("Stormy"), Btn_Stormy); }

// 계절 버튼 이벤트 
void UWeatherWidget::OnSpringClicked() { UpdateSolarBySeason(TEXT("Spring")); }
void UWeatherWidget::OnSummerClicked() { UpdateSolarBySeason(TEXT("Summer")); }
void UWeatherWidget::OnAutumnClicked() { UpdateSolarBySeason(TEXT("Autumn")); }
void UWeatherWidget::OnWinterClicked() { UpdateSolarBySeason(TEXT("Winter")); }
// 방향 버튼 이벤트
void UWeatherWidget::OnNorthClicked() { UpdateOrientation(0.0f); }
void UWeatherWidget::OnEastClicked()  { UpdateOrientation(90.0f); }
void UWeatherWidget::OnSouthClicked() { UpdateOrientation(180.0f); }
void UWeatherWidget::OnWestClicked()  { UpdateOrientation(270.0f); }

// 도시 버튼 이벤트 구현
void UWeatherWidget::OnGyeonggiClicked() { HandleCityClicked(FName("Gyeonggi"), Btn_Gyeonggi); }
void UWeatherWidget::OnGyeongnamClicked() { HandleCityClicked(FName("Gyeongnam"), Btn_Gyeongnam); }
void UWeatherWidget::OnGyeongbukClicked() { HandleCityClicked(FName("Gyeongbuk"), Btn_Gyeongbuk); }
void UWeatherWidget::OnChungnamClicked() { HandleCityClicked(FName("Chungnam"), Btn_Chungnam); }
void UWeatherWidget::OnJeonnamClicked() { HandleCityClicked(FName("Jeonnam"), Btn_Jeonnam); }
void UWeatherWidget::OnJeonbukClicked() { HandleCityClicked(FName("Jeonbuk"), Btn_Jeonbuk); }
void UWeatherWidget::OnChungbukClicked() { HandleCityClicked(FName("Chungbuk"), Btn_Chungbuk); }
void UWeatherWidget::OnGangwonClicked() { HandleCityClicked(FName("Gangwon"), Btn_Gangwon); }
void UWeatherWidget::OnJejuClicked() { HandleCityClicked(FName("Jeju"), Btn_Jeju); }
void UWeatherWidget::OnBusanClicked() { HandleCityClicked(FName("Busan"), Btn_Busan); }
void UWeatherWidget::OnIncheonClicked() { HandleCityClicked(FName("Incheon"), Btn_Incheon); }
void UWeatherWidget::OnDaeguClicked() { HandleCityClicked(FName("Daegu"), Btn_Daegu); }
void UWeatherWidget::OnDaejeonClicked() { HandleCityClicked(FName("Daejeon"), Btn_Daejeon); }
void UWeatherWidget::OnGwangjuClicked() { HandleCityClicked(FName("Gwangju"), Btn_Gwangju); }
void UWeatherWidget::OnUlsanClicked() { HandleCityClicked(FName("Ulsan"), Btn_Ulsan); }
void UWeatherWidget::OnSeoulClicked() { HandleCityClicked(FName("Seoul"), Btn_Seoul); }
void UWeatherWidget::OnSejongClicked() { HandleCityClicked(FName("Sejong"), Btn_Sejong); }

void UWeatherWidget::HandleCityClicked(FName CityRowName, UButton* ClickedButton) {
    TArray<UButton*> AllBtns = { Btn_Seoul, Btn_Busan, Btn_Incheon, Btn_Daegu, Btn_Daejeon, Btn_Gwangju, Btn_Ulsan, Btn_Sejong, Btn_Gyeonggi, Btn_Gangwon, Btn_Chungbuk, Btn_Chungnam, Btn_Jeonbuk, Btn_Jeonnam, Btn_Gyeongbuk, Btn_Gyeongnam, Btn_Jeju };
    for(auto* Btn : AllBtns) Btn->SetBackgroundColor(FLinearColor::White);
    if(ClickedButton) ClickedButton->SetBackgroundColor(FLinearColor::Blue);

    auto* Sub = GetGameInstance()->GetSubsystem<UWeatherUISubsystem>();
    if (!Sub) return;
    CB_CityDetail->ClearOptions();
    auto* Data = Sub->CityMainTable->FindRow<FCityMainData>(CityRowName, TEXT(""));
    if (Data) {
        for (auto& RowName : Sub->CityDetailTable->GetRowNames()) {
            auto* DetailData = Sub->CityDetailTable->FindRow<FCityDetailData>(RowName, TEXT(""));
            if (DetailData && DetailData->Parent_CityID == Data->CityID) {
                CB_CityDetail->AddOption(DetailData->Name_KR);
            }
        }
    }
    TriggerUpdate();
}

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

void UWeatherWidget::UpdateSolarBySeason(FString SeasonName)
{
    // 모든 버튼 색상 초기화 (흰색)
    Btn_Spring->SetBackgroundColor(FLinearColor::White);
    Btn_Summer->SetBackgroundColor(FLinearColor::White);
    Btn_Autumn->SetBackgroundColor(FLinearColor::White);
    Btn_Winter->SetBackgroundColor(FLinearColor::White);

    // 선택된 버튼만 파란색으로 변경
    if (SeasonName == TEXT("Spring")) Btn_Spring->SetBackgroundColor(FLinearColor::Blue);
    else if (SeasonName == TEXT("Summer")) Btn_Summer->SetBackgroundColor(FLinearColor::Blue);
    else if (SeasonName == TEXT("Autumn")) Btn_Autumn->SetBackgroundColor(FLinearColor::Blue);
    else if (SeasonName == TEXT("Winter")) Btn_Winter->SetBackgroundColor(FLinearColor::Blue);
    
    // 기존 절기 로직
    CB_Solar->ClearOptions();
    auto* Sub = GetGameInstance()->GetSubsystem<UWeatherUISubsystem>();
    if (!Sub) return;

    for (auto RowName : Sub->SolarTermTable->GetRowNames()) {
        auto* Data = Sub->SolarTermTable->FindRow<FSolarTermData>(RowName, TEXT(""));
        if (Data && Data->Season == SeasonName) {
            CB_Solar->AddOption(Data->Name_KR); 
        }
    }
    if (CB_Solar->GetOptionCount() > 0) 
    {
        CB_Solar->SetSelectedIndex(0); 
        TriggerUpdate();
    }
}

void UWeatherWidget::OnAnySelectionChanged(FString Selected, ESelectInfo::Type Type) { TriggerUpdate(); }

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

void UWeatherWidget::TriggerUpdate() {
    auto* Sub = GetGameInstance()->GetSubsystem<UWeatherUISubsystem>();
    if (!Sub) return;

    // 상세 지역 처리 (한글 이름을 RowName으로 변환)
    FString SelectedDetailKR = CB_CityDetail->GetSelectedOption();
    if (!SelectedDetailKR.IsEmpty()) {
        for (auto& RowName : Sub->CityDetailTable->GetRowNames()) {
            auto* Data = Sub->CityDetailTable->FindRow<FCityDetailData>(RowName, TEXT(""));
            if (Data && Data->Name_KR == SelectedDetailKR) {
                Sub->SetCityDetail(RowName); // 서브시스템에는 실제 RowName을 전달
                break;
            }
        }
    }
    // 절기 처리
    Sub->SetSolar(Sub->GetSolarRowName(CB_Solar->GetSelectedOption()));
}

// 24시 도달 시 멈추는 대신 StartTime으로 초기화하는 루프 로직
void UWeatherWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime) {
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (bIsPlaying) {
        auto* Sub = GetGameInstance()->GetSubsystem<UWeatherUISubsystem>();
        if (!Sub) return;

        float CurrentTime = Sub->GetCurrentTime();
        float NewTime = CurrentTime + (InDeltaTime * PlaySpeed);

        // [수정됨] 24시간이 흐른 후 멈추는 로직
        // 현재 시간이 시작 시간(StartTime)보다 작아지면 하루가 지났음을 의미함
        // (예: 17시에서 시작 -> 24시 -> 00시 -> 다시 17시가 되는 지점)
        if (NewTime >= 24.0f) {
            NewTime = FMath::Fmod(NewTime, 24.0f); // 24시 넘기면 0시부터 시작
        }
        
        // 시작 시간(StartTime)을 다시 지나치는 순간 멈춤
        // (PlaySpeed가 빠를 경우를 대비해 살짝 보정)
        if (NewTime >= StartTime && CurrentTime < StartTime && CurrentTime != StartTime) {
            bIsPlaying = false;
            Btn_PlayTime->SetBackgroundColor(FLinearColor::White);
            NewTime = StartTime; // 마지막 시간을 시작 시간으로 고정
        }

        // 시간 업데이트 (서브시스템 및 UI 동기화)
        Sub->SetTime(NewTime);
        Slider_Time->SetValue(NewTime / 24.0f);
        Text_Time->SetText(FText::FromString(FormatTime(NewTime)));
    }
}
// 시작 시간을 기록하는 로직 추가
void UWeatherWidget::OnPlayClicked() {
    bIsPlaying = !bIsPlaying;
    
    if (bIsPlaying) {
        // 재생 시작 시 현재 서브시스템의 시간을 저장
        auto* Sub = GetGameInstance()->GetSubsystem<UWeatherUISubsystem>();
        if (Sub) StartTime = Sub->GetCurrentTime(); 
        
        Btn_PlayTime->SetBackgroundColor(FLinearColor::Green);
    } else {
        Btn_PlayTime->SetBackgroundColor(FLinearColor::White);
    }
}

void UWeatherWidget::OnSpeedChanged(FString Selected, ESelectInfo::Type Type) {
    PlaySpeed = FCString::Atof(*Selected);
}