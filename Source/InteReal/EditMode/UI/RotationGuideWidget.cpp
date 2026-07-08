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
		if (Magnitude == 0)
		{
			return FText::FromString(TEXT("0°"));
		}
		// 화면 기반 회전 UI 관례: 시계 방향(오른쪽)은 +, 반시계 방향(왼쪽)은 -.
		// 기즈모 계산에서는 시계 방향 Delta가 음수로 들어오므로 표시 부호만 반전한다.
		return DeltaAngle < 0.0f
			? FText::FromString(FString::Printf(TEXT("+%d°"), Magnitude))
			: FText::FromString(FString::Printf(TEXT("-%d°"), Magnitude));
	}
}

void URotationGuideWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (RS_Angle)
	{
		RS_Angle->OnValueChanged.AddDynamic(this, &URotationGuideWidget::HandleSliderValueChanged);
		RS_Angle->bUseCustomDefaultValue = true;
		RS_Angle->SetCustomDefaultValue(0.0f);
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
		RS_Angle->SetCustomDefaultValue(0.0f);
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
		const float NormalizedMagnitude = DisplayAngle / 360.0f;
		const bool bClockwise = DeltaAngle < 0.0f;
		bUpdatingRadialSliderFromCode = true;
		// 왼쪽 회전은 0에서 증가하고, 오른쪽 회전은 1에서 감소시킨다.
		// CustomDefaultValue도 같은 시작점으로 맞춰 두 방향 모두 빈 상태에서 시작한다.
		RS_Angle->SetCustomDefaultValue(bClockwise ? 1.0f : 0.0f);
		RS_Angle->SetValue(bClockwise ? 1.0f - NormalizedMagnitude : NormalizedMagnitude);
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
