#include "BaseSlider.h"
#include "Components/Slider.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "InteReal/Master/UI/DesignTemplate/InteRealThemeData.h"
#include "Styling/SlateTypes.h"

void UBaseSlider::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (!ThemeData) return;

	// 0. Min/Max 범위 적용 — Progress_Track 비율 계산보다 먼저 해야 함
	if (Slider_Main)
	{
		Slider_Main->SetMinValue(MinValue);
		Slider_Main->SetMaxValue(MaxValue);
	}

	// 1. Progress Bar (뒷배경 트랙) 스타일 세팅
	if (Progress_Track)
	{
		FProgressBarStyle ProgStyle = Progress_Track->GetWidgetStyle();
		
		auto ConfigureTrack = [&](FSlateBrush& OutBrush, const FLinearColor& InColor)
		{
			OutBrush = FSlateBrush(); // 안전한 초기화
			OutBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
			OutBrush.TintColor = FSlateColor(InColor);
			OutBrush.OutlineSettings.CornerRadii = FVector4(10.0, 10.0, 10.0, 10.0);
		};

		ConfigureTrack(ProgStyle.BackgroundImage, ThemeData->Card_Border);
		ConfigureTrack(ProgStyle.FillImage, ThemeData->Accent_Gold);

		Progress_Track->SetWidgetStyle(ProgStyle);

		UpdateProgressPercent();
	}

	if (SizeBox_Track)
	{
		SizeBox_Track->SetHeightOverride(TrackThickness);
	}

	// 2. Slider (핸들 및 투명 트랙) 스타일 세팅
	if (Slider_Main)
	{
		FSliderStyle SliderStyle = Slider_Main->GetWidgetStyle();

		SliderStyle.NormalBarImage.DrawAs = ESlateBrushDrawType::NoDrawType;
		SliderStyle.HoveredBarImage.DrawAs = ESlateBrushDrawType::NoDrawType;
		SliderStyle.DisabledBarImage.DrawAs = ESlateBrushDrawType::NoDrawType;
		SliderStyle.BarThickness = 0.0f; 

		auto ConfigureThumb = [&](FSlateBrush& OutBrush, float Size)
		{
			OutBrush = FSlateBrush(); // 안전한 초기화
			
			// 🔥 Image에서 RoundedBox로 변경
			OutBrush.DrawAs = ESlateBrushDrawType::RoundedBox; 
			OutBrush.TintColor = FSlateColor(ThemeData->Accent_Gold);
			OutBrush.ImageSize = FVector2D(Size, Size);
			
			// 🔥 크기의 절반을 반경(Radius)으로 설정하여 완벽한 원형 생성
			const float HalfSize = Size / 2.0f;
			OutBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			OutBrush.OutlineSettings.CornerRadii = FVector4(HalfSize, HalfSize, HalfSize, HalfSize);
		};

		ConfigureThumb(SliderStyle.NormalThumbImage, ThumbSize);
		ConfigureThumb(SliderStyle.HoveredThumbImage, HoveredThumbSize);
		
		Slider_Main->SetWidgetStyle(SliderStyle);
	}
}

void UBaseSlider::NativeConstruct()
{
	Super::NativeConstruct();

	if (Slider_Main)
	{
		Slider_Main->OnValueChanged.AddUniqueDynamic(this, &UBaseSlider::HandleOnValueChanged);
		UpdateProgressPercent();
	}
}

void UBaseSlider::HandleOnValueChanged(float NewValue)
{
	UpdateProgressPercent();
	OnBaseValueChanged.Broadcast(NewValue);
}

void UBaseSlider::SetValue(float NewValue)
{
	if (Slider_Main)
	{
		Slider_Main->SetValue(NewValue);
	}
	UpdateProgressPercent();
}

float UBaseSlider::GetValue() const
{
	return Slider_Main ? Slider_Main->GetValue() : 0.0f;
}

void UBaseSlider::UpdateProgressPercent()
{
	if (!Progress_Track || !Slider_Main)
	{
		return;
	}

	// Slider_Main의 Min/Max가 0~1이 아닐 수도 있으므로(밝기 후보 등) 실제 범위 기준으로 비율을 계산
	const float Min = Slider_Main->GetMinValue();
	const float Max = Slider_Main->GetMaxValue();
	const float Range = Max - Min;
	const float Percent = Range > 0.0f ? (Slider_Main->GetValue() - Min) / Range : 0.0f;
	Progress_Track->SetPercent(Percent);
}
