#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateBrush.h"
#include "ColorWheelWidget.generated.h"

class SColorWheel;
class SSlider;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnColorWheelValueChanged, FLinearColor, NewColor);

UCLASS()
class INTEREAL_API UColorWheelWidget : public UWidget
{
	GENERATED_BODY()

public:
	UColorWheelWidget(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ColorWheel")
	FLinearColor SelectedColor = FLinearColor::White;

	UPROPERTY(BlueprintAssignable, Category = "ColorWheel")
	FOnColorWheelValueChanged OnValueChanged;

	UFUNCTION(BlueprintCallable, Category = "ColorWheel")
	void SetSelectedColor(FLinearColor NewColor);

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	FLinearColor HandleGetWheelColor() const;
	void HandleWheelValueChanged(FLinearColor NewHsvColor);

	float HandleGetValueSliderValue() const;
	void HandleValueSliderChanged(float NewValue);

	// 슬라이더 뒤에 깔리는 그라디언트용 — 위쪽은 현재 Hue/Saturation의 순색, 아래쪽은 검정
	TArray<FLinearColor> HandleGetValueGradientStops() const;

	TSharedPtr<SColorWheel> MyColorWheel;
	TSharedPtr<SSlider> MyValueSlider;

	// SSlider는 Style 포인터만 들고 있으므로, 위젯이 살아있는 동안 같이 유지되어야 함
	FSliderStyle ValueSliderStyle;

	// SBorder도 브러시 포인터만 들고 있으므로 마찬가지로 멤버로 유지
	FSlateBrush TrackOutlineBrush;
};
