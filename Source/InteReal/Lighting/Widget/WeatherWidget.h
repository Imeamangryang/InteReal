#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"

#include "WeatherWidget.generated.h"

UCLASS()
class INTEREAL_API UWeatherWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	// 계층형 콤보박스 
	UPROPERTY(meta = (BindWidget)) UComboBoxString* CB_CityMain;   // 광역시
	UPROPERTY(meta = (BindWidget)) UComboBoxString* CB_CityDetail; // 세부지역
	// UPROPERTY(meta = (BindWidget)) UComboBoxString* CB_Season;     // 계절
	UPROPERTY(meta = (BindWidget)) UComboBoxString* CB_Solar;      // 절기
	UPROPERTY(meta = (BindWidget)) UComboBoxString* CB_Weather;    // 날씨
	UPROPERTY(meta = (BindWidget)) USlider* Slider_Time;		   // 시간
	UPROPERTY(meta = (BindWidget)) UEditableTextBox* Text_Time;		// 시간 텍스트
	//UPROPERTY(meta = (BindWidget)) UComboBoxString* CB_Orientation; // 방향 선택용
	
	// 계절 버튼 4개 추가
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Spring;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Summer;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Autumn;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Winter;

	// 버튼 클릭 이벤트 함수
	UFUNCTION() void OnSpringClicked();
	UFUNCTION() void OnSummerClicked();
	UFUNCTION() void OnAutumnClicked();
	UFUNCTION() void OnWinterClicked();
	
	// 방향 버튼 4개
	UPROPERTY(meta = (BindWidget)) UButton* Btn_North;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_East;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_South;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_West;
	// 방향 버튼 클릭 이벤트
	UFUNCTION() void OnNorthClicked();
	UFUNCTION() void OnEastClicked();
	UFUNCTION() void OnSouthClicked();
	UFUNCTION() void OnWestClicked();
	// 공통 업데이트 함수
	void UpdateOrientation(float Angle);
	void UpdateSolarBySeason(FString SeasonName);
	
	// 변경 이벤트 핸들러
	UFUNCTION() void OnCityMainChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	// UFUNCTION() void OnSeasonChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnAnySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnSliderChanged(float Value);
	UFUNCTION() void OnTimeTextChanged(const FText& Text, ETextCommit::Type CommitMethod); 
	// UFUNCTION() void OnOrientationChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	
	void TriggerUpdate(); // 조명 업데이트 호출 함수
	
	FString FormatTime(float Hours); // 시간 포맷(00:00) 생성용 함수
};