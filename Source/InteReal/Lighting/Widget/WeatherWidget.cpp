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

	// 콤보박스 데이터 자동 채우기
	if (Sub->WeatherTable) for(auto& Name : Sub->WeatherTable->GetRowNames()) CB_Weather->AddOption(Name.ToString());
	if (Sub->CityMainTable) for(auto& Name : Sub->CityMainTable->GetRowNames()) CB_City->AddOption(Name.ToString());
	if (Sub->SolarTermTable) for(auto& Name : Sub->SolarTermTable->GetRowNames()) CB_Solar->AddOption(Name.ToString());

	if (Btn_Apply) Btn_Apply->OnClicked.AddDynamic(this, &UWeatherWidget::OnApplyClicked);
}

void UWeatherWidget::OnApplyClicked() {
	auto* Sub = GetGameInstance()->GetSubsystem<UWeatherUISubsystem>();
	if (Sub) {
		// 콤보박스 선택값과 슬라이더 시간(0~24)을 전달
		Sub->ApplyEnvironment(
			FName(*CB_City->GetSelectedOption()), 
			FName(*CB_Weather->GetSelectedOption()), 
			FName(*CB_Solar->GetSelectedOption()), 
			Slider_Time->GetValue() * 24.0f
		);
	}
}