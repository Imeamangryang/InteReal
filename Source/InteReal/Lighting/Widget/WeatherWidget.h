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

	// UI 컴포넌트
	// UPROPERTY(meta = (BindWidget)) UComboBoxString* CB_CityMain;   // 광역시
	UPROPERTY(meta = (BindWidget)) UComboBoxString* CB_CityDetail; // 세부지역
	UPROPERTY(meta = (BindWidget)) UComboBoxString* CB_Solar;      // 절기
	// UPROPERTY(meta = (BindWidget)) UComboBoxString* CB_Weather;    // 날씨
	UPROPERTY(meta = (BindWidget)) USlider* Slider_Time;		   // 시간
	UPROPERTY(meta = (BindWidget)) UEditableTextBox* Text_Time;		// 시간 텍스트
	
	// 계절 버튼 4개
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Spring;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Summer;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Autumn;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Winter;

	// 날씨 버튼 6개
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Clear;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Cloudy;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Rainy;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Snowy;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Foggy;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Stormy;
	
	// 방향 버튼 4개
	UPROPERTY(meta = (BindWidget)) UButton* Btn_North;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_East;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_South;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_West;
	
	// 도시 버튼 17개
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Seoul;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Busan;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Incheon;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Daegu;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Daejeon;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Gwangju;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Ulsan;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Sejong;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Gyeonggi;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Gangwon;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Chungbuk;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Chungnam;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Jeonbuk;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Jeonnam;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Gyeongbuk;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Gyeongnam;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Jeju;
	
	// 버튼 클릭 이벤트 함수
	UFUNCTION() void OnSpringClicked();
	UFUNCTION() void OnSummerClicked();
	UFUNCTION() void OnAutumnClicked();
	UFUNCTION() void OnWinterClicked();
	
	// 날씨 버튼용 전용 바인딩 함수들
	UFUNCTION() void OnClearClicked();
	UFUNCTION() void OnCloudyClicked();
	UFUNCTION() void OnRainyClicked();
	UFUNCTION() void OnSnowyClicked();
	UFUNCTION() void OnFoggyClicked();
	UFUNCTION() void OnStormyClicked();
	
	// 방향 버튼 클릭 이벤트
	UFUNCTION() void OnNorthClicked();
	UFUNCTION() void OnEastClicked();
	UFUNCTION() void OnSouthClicked();
	UFUNCTION() void OnWestClicked();
	
	// 지역 버튼 클릭 이벤트
	UFUNCTION() void OnGyeonggiClicked(); UFUNCTION() void OnGyeongnamClicked(); UFUNCTION() void OnGyeongbukClicked();
	UFUNCTION() void OnChungnamClicked(); UFUNCTION() void OnJeonnamClicked(); UFUNCTION() void OnJeonbukClicked();
	UFUNCTION() void OnChungbukClicked(); UFUNCTION() void OnGangwonClicked(); UFUNCTION() void OnJejuClicked(); 
	UFUNCTION() void OnBusanClicked(); UFUNCTION() void OnIncheonClicked();  UFUNCTION() void OnDaeguClicked();
	UFUNCTION() void OnDaejeonClicked(); UFUNCTION() void OnGwangjuClicked(); UFUNCTION() void OnUlsanClicked();    
	UFUNCTION() void OnSeoulClicked(); UFUNCTION() void OnSejongClicked(); 
	
	// 내부 로직 함수 // 공통 업데이트 함수
	void HandleCityClicked(FName CityRowName, UButton* ClickedButton);
	void HandleWeatherUpdate(FName RowName, UButton* ClickedButton);
	void UpdateOrientation(float Angle);
	void UpdateSolarBySeason(FString SeasonName);
	
	// 변경 이벤트 핸들러
	// UFUNCTION() void OnCityMainChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnAnySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnSliderChanged(float Value);
	UFUNCTION() void OnTimeTextChanged(const FText& Text, ETextCommit::Type CommitMethod); 
	
	void TriggerUpdate(); // 조명 업데이트 호출 함수
	FString FormatTime(float Hours); // 시간 포맷(00:00) 생성용 함수
};