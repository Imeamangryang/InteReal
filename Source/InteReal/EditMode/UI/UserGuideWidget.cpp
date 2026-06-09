#include "UserGuideWidget.h"

void UUserGuideWidget::UpdateTooltip(EPlacementInvalidReason Reason)
{
	if (!PlacementTooltip) return;

	if (Reason == EPlacementInvalidReason::None)
	{
		PlacementTooltip->SetVisibility(ESlateVisibility::Hidden);
	}
	else
	{
		PlacementTooltip->ShowReason(Reason);
		PlacementTooltip->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}
