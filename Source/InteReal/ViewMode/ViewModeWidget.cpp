#include "ViewModeWidget.h"
#include "Components/Button.h"
#include "ViewModePlayerController.h"

void UViewModeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_TopDown)
	{
		Btn_TopDown->OnClicked.AddDynamic(this, &UViewModeWidget::OnTopDownClicked);
	}

	if (Btn_Isometric)
	{
		Btn_Isometric->OnClicked.AddDynamic(this, &UViewModeWidget::OnIsometricClicked);
	}

	if (Btn_FirstPerson)
	{
		Btn_FirstPerson->OnClicked.AddDynamic(this, &UViewModeWidget::OnFirstPersonClicked);
	}
}

void UViewModeWidget::ChangeViewMode(EHarnessViewMode NewMode)
{
	if (AViewModePlayerController* PC = Cast<AViewModePlayerController>(GetOwningPlayer()))
	{
		PC->SetViewMode(NewMode);
	}
}

void UViewModeWidget::OnTopDownClicked()
{
	ChangeViewMode(EHarnessViewMode::TopDown);
}

void UViewModeWidget::OnIsometricClicked()
{
	ChangeViewMode(EHarnessViewMode::Isometric);
}

void UViewModeWidget::OnFirstPersonClicked()
{
	ChangeViewMode(EHarnessViewMode::FirstPerson);
}
