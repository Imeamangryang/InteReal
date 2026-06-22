#include "RotationGuideWidget.h"
#include "Components/TextBlock.h"
#include "Components/RadialSlider.h"

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
		// Yaw (-180 ~ 180) → (0 ~ 1)
		const float InitialValue = (BaseYawDegrees + 180.0f) / 360.0f;
		RS_Angle->SetValue(InitialValue);
	}

	if (Txt_Angle)
	{
		Txt_Angle->SetText(FText::FromString(TEXT("0°")));
	}

	// Visible: 클릭/드래그 입력을 받아야 하므로 HitTestInvisible 아님
	SetVisibility(ESlateVisibility::Visible);
}

void URotationGuideWidget::HideGuide()
{
	SetVisibility(ESlateVisibility::Hidden);
}

void URotationGuideWidget::HandleSliderValueChanged(float Value)
{
	const float NewYaw = Value * 360.0f - 180.0f;
	const float Delta = FRotator::NormalizeAxis(NewYaw - BaseYawDegrees);

	if (Txt_Angle)
	{
		Txt_Angle->SetText(FText::FromString(
			FString::Printf(TEXT("%d°"), FMath::RoundToInt(FMath::Abs(Delta)))));
	}

	OnRotationChanged.Broadcast(NewYaw);
}


void URotationGuideWidget::UpdateRotation(float DeltaAngle)
{
	if (Txt_Angle)
	{
		Txt_Angle->SetText(FText::FromString(
			FString::Printf(TEXT("%d°"), FMath::RoundToInt(FMath::Abs(DeltaAngle)))));
	}
}

void URotationGuideWidget::ShowGuide()
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
}
