#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "EnvironmentPanel.generated.h"

class USelectableButton;
class UBaseSlider;
class UBaseComboBox;
class UTextBlock;
class UWidgetSwitcher;
class UButton;
class UBorder;
class UInteRealThemeData;
class UImage; 
class UWeatherUISubsystem;
class UEditableTextBox;

UCLASS(Abstract)
class INTEREAL_API UEnvironmentPanel : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    
    // UI 컴포넌트
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UBorder> Border_Background;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TitleText;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget)) TObjectPtr<UButton> Btn_TogglePanel;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> ContentContainer;

    // 지역 선택 콤보박스
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UBaseComboBox> ComboBox_City;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UBaseComboBox> ComboBox_District;
    
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> SubTitle_City;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> SubTitle_District;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> SubTitle_Time;
    
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> SubTitle_Season;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> SubTitle_SolarTerm;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> SubTitle_Orientation;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> SubTitle_Weather;

    UPROPERTY(meta = (BindWidget)) TObjectPtr<UBaseSlider> Slider_Time;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UEditableTextBox> Edit_Time;
    
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USelectableButton> Btn_Play;
    // 재생 속도 조절용 콤보박스
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UBaseComboBox> ComboBox_PlaySpeed;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USelectableButton> Btn_Spring;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USelectableButton> Btn_Summer;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USelectableButton> Btn_Autumn;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USelectableButton> Btn_Winter;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UBaseComboBox> ComboBox_SolarTerm;

    UPROPERTY(meta = (BindWidget)) TObjectPtr<USelectableButton> Btn_North;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USelectableButton> Btn_NorthEast;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USelectableButton> Btn_East;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USelectableButton> Btn_SouthEast;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USelectableButton> Btn_South;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USelectableButton> Btn_SouthWest;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USelectableButton> Btn_West;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USelectableButton> Btn_NorthWest;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> Img_Orientation;
    
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USelectableButton> Btn_Clear;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USelectableButton> Btn_Cloudy;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USelectableButton> Btn_Rainy;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USelectableButton> Btn_Foggy;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USelectableButton> Btn_Snowy;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USelectableButton> Btn_Stormy;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
    TObjectPtr<UInteRealThemeData> ThemeData;

    UPROPERTY(BlueprintReadOnly, Category = "State")
    bool bIsPanelExpanded = true;

    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
    void BP_OnPanelStateChanged(bool bIsExpanded);

private:
    class UWeatherUISubsystem* GetWeatherSubsystem() const;
    
    // 특정 계절에 맞춰 콤보박스 리스트를 갱신하는 함수
    void RefreshSolarComboBox(FString Season, FString DefaultTerm = TEXT(""));
    
    // 속도 변경 이벤트 핸들러
    UFUNCTION() void HandlePlaySpeedChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void HandleCityChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void HandleDistrictChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    
    UFUNCTION() void HandleTogglePanelClicked();
    UFUNCTION() void HandleTimeChanged(float NewValue);
    UFUNCTION() void HandleSolarTermChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void HandlePlayClicked();
    
    UFUNCTION() void HandleTimeTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);
    
    UFUNCTION() void HandleSpringClicked();
    UFUNCTION() void HandleSummerClicked();
    UFUNCTION() void HandleAutumnClicked();
    UFUNCTION() void HandleWinterClicked();

    UFUNCTION() void HandleNorthClicked();
    UFUNCTION() void HandleNorthEastClicked();
    UFUNCTION() void HandleEastClicked();
    UFUNCTION() void HandleSouthEastClicked();
    UFUNCTION() void HandleSouthClicked();
    UFUNCTION() void HandleSouthWestClicked();
    UFUNCTION() void HandleWestClicked();
    UFUNCTION() void HandleNorthWestClicked();
    
    UFUNCTION() void HandleClearClicked();
    UFUNCTION() void HandleCloudyClicked();
    UFUNCTION() void HandleRainyClicked();
    UFUNCTION() void HandleFoggyClicked();
    UFUNCTION() void HandleSnowyClicked();
    UFUNCTION() void HandleStormyClicked();
    
    void UpdateOrientationUI(float Angle);
    void UpdateSeasonGroup(TObjectPtr<USelectableButton> SelectedButton);
    void UpdateWeatherGroup(TObjectPtr<USelectableButton> SelectedButton);
    void UpdateOrientationGroup(TObjectPtr<USelectableButton> SelectedButton);
    
    bool bIsPlaying = false;
    float PlaySpeed = 1.0f;
    float PlayStartTime = 0.0f;
    float AccumulatedTime = 0.0f; // 누적 시간 추적용
    
    float TargetAngle = 0.0f;
    float CurrentAngle = 0.0f;
    float RotationSpeed = 10.0f; // 회전 속도 (값이 클수록 빠름)
};
