#include "TopBarWidget.h"
#include "InteReal/Master/InteRealPlayerController.h"

void UTopBarWidget::ChangeViewMode(EHarnessViewMode NewMode)
{
	if (AInteRealPlayerController* PC = Cast<AInteRealPlayerController>(GetOwningPlayer()))
	{
		PC->SetViewMode(NewMode);
	}
}
