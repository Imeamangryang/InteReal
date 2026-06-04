#include "RotationGuideWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"

void URotationGuideWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Img_Radial)
	{
		RadialDynMat = Img_Radial->GetDynamicMaterial();
	}

	HideGuide();
}

void URotationGuideWidget::UpdateRotation(float DeltaAngle)
{
	if (RadialDynMat)
	{
		float Fraction = FMath::Abs(DeltaAngle) / 360.f;
		RadialDynMat->SetScalarParameterValue(TEXT("RadialWipe"), Fraction);
	}

	if (Txt_Angle)
	{
		int32 Degrees = FMath::RoundToInt(FMath::Abs(DeltaAngle));
		Txt_Angle->SetText(FText::FromString(FString::Printf(TEXT("%d°"), Degrees)));
	}
}

void URotationGuideWidget::ShowGuide()
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void URotationGuideWidget::HideGuide()
{
	SetVisibility(ESlateVisibility::Hidden);
}
