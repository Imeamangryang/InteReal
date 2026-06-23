#include "RotationGuideWidget.h"
#include "Components/TextBlock.h"
#include "Components/RadialSlider.h"

namespace
{
	FText FormatRotationDelta(float DeltaAngle)
	{
		const int32 Magnitude = FMath::Min(FMath::RoundToInt(FMath::Abs(DeltaAngle)), 359);
		if (Magnitude == 0)
		{
			return FText::FromString(TEXT("0°"));
		}
		
		if (DeltaAngle < 0.0f)
		{
			return FText::FromString(FString::Printf(TEXT("+ %d°"), Magnitude));
		}

		return FText::FromString(FString::Printf(TEXT("- %d°"), Magnitude));
	}
}

void URotationGuideWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (RS_Angle)
	{
		RS_Angle->OnValueChanged.AddDynamic(this, &URotationGuideWidget::HandleSliderValueChanged);
		RS_Angle->SetVisibility(bShowRadialSlider
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}

	HideGuide();
}

void URotationGuideWidget::ShowForRotation(float InitialYawDegrees)
{
	BaseYawDegrees = FRotator::NormalizeAxis(InitialYawDegrees);

	if (RS_Angle)
	{
		RS_Angle->SetVisibility(ESlateVisibility::Visible);
		// Map 0..360 degrees directly onto the radial slider's 0..1 range.
		const float InitialValue = FRotator::ClampAxis(BaseYawDegrees) / 360.0f;
		RS_Angle->SetValue(InitialValue);
	}

	if (Txt_Angle)
	{
		Txt_Angle->SetText(FText::FromString(TEXT("0°")));
	}

	// The active gizmo drag owns the mouse; this widget mirrors that drag.
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void URotationGuideWidget::HideGuide()
{
	SetVisibility(ESlateVisibility::Hidden);
}

void URotationGuideWidget::HandleSliderValueChanged(float Value)
{
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
		const float CurrentAngle = FRotator::ClampAxis(BaseYawDegrees + DeltaAngle);
		RS_Angle->SetValue(CurrentAngle / 360.0f);
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
