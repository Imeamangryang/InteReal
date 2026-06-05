#include "RotationGuideWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"

void URotationGuideWidget::NativeConstruct()
{
	Super::NativeConstruct();

	HideGuide();
}

void URotationGuideWidget::UpdateRotation(float DeltaAngle)
{
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
