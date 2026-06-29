#include "ColorWheelWidget.h"
#include "Components/SlateWrapperTypes.h"
#include "Widgets/Colors/SColorWheel.h"
#include "Widgets/Colors/SComplexGradient.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"

UColorWheelWidget::UColorWheelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UColorWheelWidget::SetSelectedColor(FLinearColor NewColor)
{
	SelectedColor = NewColor;
}

FLinearColor UColorWheelWidget::HandleGetWheelColor() const
{
	return SelectedColor.LinearRGBToHSV();
}

void UColorWheelWidget::HandleWheelValueChanged(FLinearColor NewHsvColor)
{
	SelectedColor = NewHsvColor.HSVToLinearRGB();
	OnValueChanged.Broadcast(SelectedColor);
}

float UColorWheelWidget::HandleGetValueSliderValue() const
{
	return SelectedColor.LinearRGBToHSV().B;
}

void UColorWheelWidget::HandleValueSliderChanged(float NewValue)
{
	FLinearColor Hsv = SelectedColor.LinearRGBToHSV();
	Hsv.B = NewValue;
	SelectedColor = Hsv.HSVToLinearRGB();
	OnValueChanged.Broadcast(SelectedColor);
}

TArray<FLinearColor> UColorWheelWidget::HandleGetValueGradientStops() const
{
	FLinearColor Hsv = SelectedColor.LinearRGBToHSV();
	Hsv.B = 1.0f; // 위쪽 끝은 항상 최대 명도의 순색
	return { Hsv.HSVToLinearRGB(), FLinearColor::Black };
}

TSharedRef<SWidget> UColorWheelWidget::RebuildWidget()
{
	MyColorWheel = SNew(SColorWheel)
		.SelectedColor(BIND_UOBJECT_ATTRIBUTE(FLinearColor, HandleGetWheelColor))
		.OnValueChanged(BIND_UOBJECT_DELEGATE(FOnLinearColorValueChanged, HandleWheelValueChanged));
	
	ValueSliderStyle = FCoreStyle::Get().GetWidgetStyle<FSliderStyle>("Slider");
	ValueSliderStyle.NormalBarImage.DrawAs = ESlateBrushDrawType::NoDrawType;
	ValueSliderStyle.HoveredBarImage.DrawAs = ESlateBrushDrawType::NoDrawType;
	ValueSliderStyle.DisabledBarImage.DrawAs = ESlateBrushDrawType::NoDrawType;
	ValueSliderStyle.BarThickness = 0.0f;

	MyValueSlider = SNew(SSlider)
		.Orientation(Orient_Vertical)
		.Style(&ValueSliderStyle)
		.Value(BIND_UOBJECT_ATTRIBUTE(float, HandleGetValueSliderValue))
		.OnValueChanged(BIND_UOBJECT_DELEGATE(FOnFloatValueChanged, HandleValueSliderChanged));
	
	TrackOutlineBrush = FSlateBrush();
	TrackOutlineBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	TrackOutlineBrush.TintColor = FSlateColor(FLinearColor::White);
	TrackOutlineBrush.OutlineSettings.Color = FSlateColor(FLinearColor::White);
	TrackOutlineBrush.OutlineSettings.Width = 2.0f;
	TrackOutlineBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			MyColorWheel.ToSharedRef()
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
		[
			SNew(SBox)
			.WidthOverride(24.0f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew(SBorder)
					.BorderImage(&TrackOutlineBrush)
					.Padding(FMargin(3.0f))
					[
						SNew(SComplexGradient)
						.Orientation(Orient_Horizontal)
						.GradientColors(BIND_UOBJECT_ATTRIBUTE(TArray<FLinearColor>, HandleGetValueGradientStops))
					]
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					MyValueSlider.ToSharedRef()
				]
			]
		];
}

void UColorWheelWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
}

void UColorWheelWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyColorWheel.Reset();
	MyValueSlider.Reset();
}
