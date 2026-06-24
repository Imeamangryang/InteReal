#include "RotationGuideWidget.h"
#include "Components/TextBlock.h"
#include "Components/RadialSlider.h"

namespace
{
	float GetDisplayRotationMagnitude(float DeltaAngle)
	{
		return FMath::Fmod(FMath::Abs(DeltaAngle), 360.0f);
	}

	FText FormatRotationDelta(float DeltaAngle)
	{
		const int32 Magnitude = FMath::RoundToInt(GetDisplayRotationMagnitude(DeltaAngle));
		return FText::FromString(FString::Printf(TEXT("%d°"), Magnitude));
	}
}

void URotationGuideWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (RS_Angle)
	{
		RS_Angle->OnValueChanged.AddDynamic(this, &URotationGuideWidget::HandleSliderValueChanged);
		RS_Angle->SetVisibility(ESlateVisibility::Collapsed);
	}

	HideGuide();
}

void URotationGuideWidget::ShowForRotation(float InitialYawDegrees)
{
	BaseYawDegrees = FRotator::NormalizeAxis(InitialYawDegrees);

	if (RS_Angle)
	{
		bUpdatingRadialSliderFromCode = true;
		RS_Angle->SetValue(0.0f);
		bUpdatingRadialSliderFromCode = false;
		RS_Angle->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (Txt_Angle)
	{
		Txt_Angle->SetText(FText::FromString(TEXT("0°")));
	}
	
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void URotationGuideWidget::HideGuide()
{
	SetVisibility(ESlateVisibility::Hidden);
}

void URotationGuideWidget::HandleSliderValueChanged(float Value)
{
	if (bUpdatingRadialSliderFromCode)
	{
		return;
	}

	const float NewYaw = FRotator::NormalizeAxis(Value * 360.0f);
	const float Delta = FRotator::NormalizeAxis(NewYaw - BaseYawDegrees);

	if (Txt_Angle)
	{
		Txt_Angle->SetText(FormatRotationDelta(Delta));
	}

	OnRotationChanged.Broadcast(NewYaw);
}


void URotationGuideWidget::UpdateRotation(float DeltaAngle)
{
	if (RS_Angle)
	{
		const float DisplayAngle = GetDisplayRotationMagnitude(DeltaAngle);
		const float SliderAngle = FRotator::ClampAxis(DeltaAngle);
		bUpdatingRadialSliderFromCode = true;
		RS_Angle->SetValue(SliderAngle / 360.0f);
		bUpdatingRadialSliderFromCode = false;
		RS_Angle->SetVisibility(
			bShowRadialSlider && DisplayAngle > RadialSliderRevealThresholdDegrees
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}

	if (Txt_Angle)
	{
		Txt_Angle->SetText(FormatRotationDelta(DeltaAngle));
	}
}

void URotationGuideWidget::ShowGuide()
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
}
