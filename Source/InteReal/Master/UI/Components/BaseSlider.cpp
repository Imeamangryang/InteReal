#include "BaseSlider.h"
#include "Components/Slider.h"
#include "Components/ProgressBar.h"
#include "InteReal/Master/UI/DesignTemplate/InteRealThemeData.h"
#include "Styling/SlateTypes.h"

void UBaseSlider::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (!ThemeData) return;

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
		
		if (Slider_Main)
		{
			Progress_Track->SetPercent(Slider_Main->GetValue());
		}
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

		ConfigureThumb(SliderStyle.NormalThumbImage, 20.0f);
		ConfigureThumb(SliderStyle.HoveredThumbImage, 28.0f); // 마우스를 올렸을 때 조금 더 커지게(24 -> 28) 하면 터치감이 좋아집니다!
		
		Slider_Main->SetWidgetStyle(SliderStyle);
	}
}

void UBaseSlider::NativeConstruct()
{
	Super::NativeConstruct();

	if (Slider_Main)
	{
		Slider_Main->OnValueChanged.AddUniqueDynamic(this, &UBaseSlider::HandleOnValueChanged);
		
		if (Progress_Track)
		{
			Progress_Track->SetPercent(Slider_Main->GetValue());
		}
	}
}

void UBaseSlider::HandleOnValueChanged(float NewValue)
{
	if (Progress_Track)
	{
		Progress_Track->SetPercent(NewValue);
	}

	OnBaseValueChanged.Broadcast(NewValue);
}
