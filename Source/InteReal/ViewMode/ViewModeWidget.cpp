#include "ViewModeWidget.h"

#include "ViewModeManager.h"
#include "Components/Button.h"
#include "ViewModePlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Public/HarnessPipelineManager.h"

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

	if (Btn_RotateCanvas)
	{
		Btn_RotateCanvas->OnClicked.AddDynamic(this, &UViewModeWidget::OnRotateCanvasClicked);
	}

	if (Btn_Save)
	{
		Btn_Save->OnClicked.AddDynamic(this, &UViewModeWidget::OnSaveClicked);
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

void UViewModeWidget::OnRotateCanvasClicked()
{
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AViewModeManager::StaticClass(), FoundActors);
	if (FoundActors.Num() > 0)
	{
		if (AViewModeManager* Manager = Cast<AViewModeManager>(FoundActors[0]))
		{
			Manager->ToggleCanvasRotation();
		}
	}
}

void UViewModeWidget::OnSaveClicked()
{
	// PipelineManager를 찾아 저장 명령 전달
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), FoundActors);
	for (AActor* Actor : FoundActors)
	{
		if (UHarnessPipelineManager* Pipeline = Actor->FindComponentByClass<UHarnessPipelineManager>())
		{
			Pipeline->SaveCurrentProject();
			break;
		}
	}
}
