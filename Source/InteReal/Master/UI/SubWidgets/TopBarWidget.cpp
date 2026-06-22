#include "TopBarWidget.h"
#include "InteReal/Master/InteRealPlayerController.h"
#include "InteReal/Master/UI/Components/BaseComboBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Button.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "InteReal/Master/UI/Components/IconTextButtonWidget.h"
#include "InteReal/Harness/Public/HarnessPipelineManager.h"

namespace
{
	FString GetMockTestDataFilePath(const FString& FileName)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("TestData") / FileName);
	}
}

void UTopBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UInteRealPlanViewModel* VM = GetPlanViewModel();
	if (!VM) return;

	if (SearchList_Project)
	{
		ProjectController = NewObject<UInteRealProjectListController>(this);
		ProjectController->InitializeController(VM, SearchList_Project);
		ProjectController->OnProjectSelected.AddUniqueDynamic(this, &UTopBarWidget::OnProjectSelected);
	}

	if (SearchList_Plan)
	{
		PlanController = NewObject<UInteRealPlanListController>(this);
		PlanController->InitializeController(VM, SearchList_Plan);
		PlanController->OnPlanSelected.AddUniqueDynamic(this, &UTopBarWidget::OnPlanSelected);
	}

	if (SearchList_Version)
	{
		VersionController = NewObject<UInteRealVersionListController>(this);
		VersionController->InitializeController(VM, SearchList_Version);
		VersionController->OnVersionSelected.AddUniqueDynamic(this, &UTopBarWidget::OnVersionSelected);
	}

	if (IconTextButton_Capture)
	{
		IconTextButton_Capture->OnIconTextButtonClicked.AddUniqueDynamic(this, &UTopBarWidget::HandleCaptureClicked);
	}

	if (Btn_Save)
	{
		Btn_Save->OnClicked.AddUniqueDynamic(this, &UTopBarWidget::HandleSaveClicked);
	}

	if (Btn_SaveAsNewVersion)
	{
		Btn_SaveAsNewVersion->OnClicked.AddUniqueDynamic(this, &UTopBarWidget::HandleSaveAsNewVersionClicked);
	}

	if (GetWorld())
	{
		if (UHarnessPipelineManager* Pipeline = GetWorld()->GetSubsystem<UHarnessPipelineManager>())
		{
			Pipeline->OnPipelineSaveFinished.AddUniqueDynamic(this, &UTopBarWidget::HandlePipelineSaveFinished);
		}
	}

	// Trigger initial fetch
	if (ProjectController)
	{
		ProjectController->Refresh();
	}
}

void UTopBarWidget::NativeDestruct()
{
	if (GetWorld())
	{
		if (UHarnessPipelineManager* Pipeline = GetWorld()->GetSubsystem<UHarnessPipelineManager>())
		{
			Pipeline->OnPipelineSaveFinished.RemoveDynamic(this, &UTopBarWidget::HandlePipelineSaveFinished);
		}
	}

	if (ProjectController) ProjectController->DeinitializeController();
	if (PlanController) PlanController->DeinitializeController();
	if (VersionController) VersionController->DeinitializeController();
	
	Super::NativeDestruct();
}

UInteRealPlanViewModel* UTopBarWidget::GetPlanViewModel()
{
	if (PlanViewModel) return PlanViewModel;

	if (GetWorld())
	{
		for (TObjectIterator<UInteRealPlanViewModel> It; It; ++It)
		{
			if (It->GetWorld() == GetWorld())
			{
				PlanViewModel = *It;
				UE_LOG(LogTemp, Log, TEXT("[TopBar] Found existing ViewModel in world."));
				return PlanViewModel;
			}
		}
	}

	PlanViewModel = NewObject<UInteRealPlanViewModel>(this);
	UE_LOG(LogTemp, Log, TEXT("[TopBar] Created new ViewModel instance."));
	return PlanViewModel;
}

void UTopBarWidget::ChangeViewMode(EHarnessViewMode NewMode)
{
	if (AInteRealPlayerController* PC = Cast<AInteRealPlayerController>(GetOwningPlayer()))
	{
		PC->SetViewMode(NewMode);
	}
}

void UTopBarWidget::OnProjectSelected(const FUnrealProjectItem& ProjectItem)
{
	UE_LOG(LogTemp, Log, TEXT("[TopBar] OnProjectSelected: %s"), *ProjectItem.name);
	
	if (PlanController)
	{
		PlanController->SetProjectIdFilter(ProjectItem.id);
	}

	if (VersionController)
	{
		VersionController->SetPlanIdFilter(0);
	}
}

void UTopBarWidget::OnPlanSelected(const FUnrealPlanItem& PlanItem)
{
	UE_LOG(LogTemp, Log, TEXT("[TopBar] OnPlanSelected: %s"), *PlanItem.name);
	
	if (VersionController)
	{
		VersionController->SetPlanIdFilter(PlanItem.id);
	}
}

void UTopBarWidget::OnVersionSelected(const FUnrealDeltaVersionItem& VersionItem)
{
	UE_LOG(LogTemp, Log, TEXT("[TopBar] OnVersionSelected: Version %d"), VersionItem.version);

	if (UHarnessPipelineManager* Pipeline = GetWorld()->GetSubsystem<UHarnessPipelineManager>())
	{
		Pipeline->SetCurrentDeltaVersion(FMath::Max(VersionItem.version, 1));
	}
}

void UTopBarWidget::HandleCaptureClicked(FName ButtonId, UIconTextButtonWidget* ButtonWidget)
{
	FString FileName = FString::Printf(TEXT("InteReal_Capture_%s.png"), *FDateTime::Now().ToString());
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
	
	UE_LOG(LogTemp, Log, TEXT("[TopBar] Screenshot requested: %s"), *FileName);
}

void UTopBarWidget::HandleSaveClicked()
{
	if (UHarnessPipelineManager* Pipeline = GetWorld()->GetSubsystem<UHarnessPipelineManager>())
	{
		bLastSaveRequestedNewVersion = false;
		Pipeline->SaveCurrentProject();
	}
}

void UTopBarWidget::HandleSaveAsNewVersionClicked()
{
	if (UHarnessPipelineManager* Pipeline = GetWorld()->GetSubsystem<UHarnessPipelineManager>())
	{
		bLastSaveRequestedNewVersion = true;
		Pipeline->SaveCurrentProjectAsNewVersion();
	}
}

void UTopBarWidget::HandlePipelineSaveFinished(bool bSuccess, const FUnrealOkResponse& Response)
{
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TopBar] Save failed."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[TopBar] Save finished. Refreshing versions. Version: %d"), Response.version);

	if (VersionController)
	{
		if (bLastSaveRequestedNewVersion)
		{
			VersionController->RefreshAndSelectLatest();
		}
		else if (Response.version > 0)
		{
			VersionController->RefreshAndSelectVersion(Response.version);
		}
		else
		{
			VersionController->Refresh();
		}
	}

	bLastSaveRequestedNewVersion = false;
}
