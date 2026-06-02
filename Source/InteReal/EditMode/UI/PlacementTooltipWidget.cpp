#include "PlacementTooltipWidget.h"

void UPlacementTooltipWidget::ShowReason(EPlacementInvalidReason Reason)
{
	if (!Txt_Message) return;

	switch (Reason)
	{
	case EPlacementInvalidReason::Overlapping:
		Txt_Message->SetText(FText::FromString(TEXT("다른 가구와 겹쳐서 배치할 수 없습니다")));
		break;
	case EPlacementInvalidReason::OutOfBounds:
		Txt_Message->SetText(FText::FromString(TEXT("배치 가능 영역을 벗어났습니다")));
		break;
	default:
		break;
	}
}
