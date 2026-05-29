#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/Button.h"
#include "WeatherWidget.generated.h"

UCLASS()
class INTEREAL_API UWeatherWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	// UI 디자인의 위젯 이름과 정확히 일치해야 함
	UPROPERTY(meta = (BindWidget)) UComboBoxString* CB_City;
	UPROPERTY(meta = (BindWidget)) UComboBoxString* CB_Weather;
	UPROPERTY(meta = (BindWidget)) UComboBoxString* CB_Solar;
	UPROPERTY(meta = (BindWidget)) USlider* Slider_Time;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Apply;

	UFUNCTION() void OnApplyClicked();
};