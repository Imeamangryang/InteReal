#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "InteReal/EditMode/Furniture/FFurnitureDataRow.h"
#include "LightAttributesPanelWidget.generated.h"

class AFurniture;
class ALightFixture;
class UColorWheelWidget;
class UBaseSlider;
class UBaseInput;
class UBaseToggleSwitch;
class UBaseButton;
class UBorder;
class UButton;

// 조명(ALightFixture) 테스트용 패널 — 색상/밝기/범위/on-off
UCLASS()
class INTEREAL_API ULightAttributesPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	
	UFUNCTION(BlueprintCallable, Category = "LightAttributesPanel")
	void RefreshForFurniture(AFurniture* Furniture);

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_OnOff;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UColorWheelWidget> ColorWheel;

	// "# FFD26A" 형태 
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBaseInput> Input_Hex;

	// 0~255
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBaseInput> Input_R;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBaseInput> Input_G;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBaseInput> Input_B;

	// 현재 색을 그대로 채워 보여주는 작은 정사각형
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_ColorSwatch;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBaseSlider> Slider_Intensity;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBaseInput> Input_IntensityValue;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBaseSlider> Slider_Radius;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBaseInput> Input_RadiusValue;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBaseToggleSwitch> Toggle_Enabled;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBaseButton> Btn_Cancel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBaseButton> Btn_Apply;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Close;

private:
	UFUNCTION()
	void HandleColorChanged(FLinearColor NewColor);

	UFUNCTION()
	void HandleHexTextCommitted(const FText& NewText, ETextCommit::Type CommitType);

	UFUNCTION()
	void HandleRInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleGInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleBInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	// R/G/B 입력칸 하나가 바뀌었을 때 나머지 채널은 현재 색에서 그대로 가져와 합치고 반영
	void ApplyRgbChannelChange(uint8 NewR, uint8 NewG, uint8 NewB);

	// ColorWheel/Hex/RGB 표시를 한 색상값으로 동기화
	void RefreshColorReadouts(const FLinearColor& Color);

	UFUNCTION()
	void HandleIntensityChanged(float NewValue);

	UFUNCTION()
	void HandleRadiusChanged(float NewValue);

	UFUNCTION()
	void HandleEnabledToggled(bool bIsOn);

	UFUNCTION()
	void HandleIntensityInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleRadiusInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleCancelClicked();

	UFUNCTION()
	void HandleApplyClicked();

	void ApplyToTarget();

	TWeakObjectPtr<ALightFixture> TargetLight;

	// 취소 눌렀을 때 복원할 패널 열기 전 원래 값
	FLightAttributes SnapshotAttributes;
};
