#include "TopBarWidget.h"
#include "InteReal/Master/InteRealPlayerController.h"
#include "InteReal/Master/UI/Components/BaseComboBox.h"
#include "Components/ComboBoxString.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "InteReal/Master/UI/Components/IconTextButtonWidget.h"

void UTopBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogTemp, Log, TEXT("[TopBar] NativeConstruct: ComboBox_PlanList is %s"), ComboBox_PlanList ? TEXT("Valid") : TEXT("NULL"));
	UE_LOG(LogTemp, Log, TEXT("[TopBar] NativeConstruct: ComboBox_VersionList is %s"), ComboBox_VersionList ? TEXT("Valid") : TEXT("NULL"));
	UE_LOG(LogTemp, Log, TEXT("[TopBar] NativeConstruct: IconTextButton_Capture is %s"), IconTextButton_Capture ? TEXT("Valid") : TEXT("NULL"));

	if (ComboBox_PlanList)
	{
		ComboBox_PlanList->OnBaseSelectionChanged.AddUniqueDynamic(this, &UTopBarWidget::OnPlanSelected);
	}

	if (ComboBox_VersionList)
	{
		ComboBox_VersionList->OnBaseSelectionChanged.AddUniqueDynamic(this, &UTopBarWidget::OnVersionSelected);
	}

	if (IconTextButton_Capture)
	{
		IconTextButton_Capture->OnIconTextButtonClicked.AddUniqueDynamic(this, &UTopBarWidget::HandleCaptureClicked);
	}

	if (UInteRealPlanViewModel* VM = GetPlanViewModel())
	{
		UE_LOG(LogTemp, Log, TEXT("[TopBar] ViewModel found/created. Fetching plan list..."));
		VM->OnPlanListUpdated.AddUniqueDynamic(this, &UTopBarWidget::OnPlanListUpdated);
		
		FUnrealPlanSearchParams Params;
		VM->FetchPlanList(Params);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[TopBar] Failed to get PlanViewModel!"));
	}
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

void UTopBarWidget::OnPlanListUpdated(bool bSuccess, const FUnrealPlanListResponse& Response)
{
	UE_LOG(LogTemp, Log, TEXT("[TopBar] OnPlanListUpdated: Success=%d, ItemCount=%d"), bSuccess, Response.items.Num());

	if (!bSuccess || !ComboBox_PlanList || !ComboBox_PlanList->ComboBox_Main) 
	{
		UE_LOG(LogTemp, Warning, TEXT("[TopBar] OnPlanListUpdated failed: Success=%d, ComboBox_PlanList=%s"), bSuccess, ComboBox_PlanList ? TEXT("Valid") : TEXT("NULL"));
		return;
	}

	ComboBox_PlanList->ComboBox_Main->ClearOptions();
	PlanMap.Empty();

	for (const FUnrealPlanItem& Plan : Response.items)
	{
		ComboBox_PlanList->ComboBox_Main->AddOption(Plan.name);
		PlanMap.Add(Plan.name, Plan);
	}

	if (Response.items.Num() > 0)
	{
		ComboBox_PlanList->ComboBox_Main->SetSelectedIndex(0);
	}
}

void UTopBarWidget::OnPlanSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	UE_LOG(LogTemp, Log, TEXT("[TopBar] OnPlanSelected: %s"), *SelectedItem);
	if (UInteRealPlanViewModel* VM = GetPlanViewModel())
	{
		if (FUnrealPlanItem* FoundPlan = PlanMap.Find(SelectedItem))
		{
			CurrentSelectedPlanId = FoundPlan->id;
			VM->LoadPlanProject(*FoundPlan);

			// Count local mock versions
			int32 MaxVersion = 0;
			while (true)
			{
				FString VersionPath = FPaths::ProjectContentDir() / TEXT("TestData") / FString::Printf(TEXT("test%d_delta_v%d.json"), CurrentSelectedPlanId, MaxVersion + 1);
				if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*VersionPath))
				{
					MaxVersion++;
				}
				else
				{
					break;
				}
			}
			UE_LOG(LogTemp, Log, TEXT("[TopBar] Found %d mock versions for Plan %d"), MaxVersion, CurrentSelectedPlanId);
			RefreshVersionList(MaxVersion);
		}
	}
}

void UTopBarWidget::RefreshVersionList(int32 MaxVersion)
{
	if (!ComboBox_VersionList || !ComboBox_VersionList->ComboBox_Main) return;

	ComboBox_VersionList->ComboBox_Main->ClearOptions();
	ComboBox_VersionList->ComboBox_Main->AddOption(TEXT("Latest"));

	for (int32 i = 1; i <= MaxVersion; ++i)
	{
		ComboBox_VersionList->ComboBox_Main->AddOption(FString::Printf(TEXT("Version %d"), i));
	}

	ComboBox_VersionList->ComboBox_Main->SetSelectedIndex(0);
}

void UTopBarWidget::OnVersionSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (UInteRealPlanViewModel* VM = GetPlanViewModel())
	{
		if (SelectedItem == TEXT("Latest"))
		{
			VM->RefreshLatestDelta();
		}
		else if (SelectedItem.StartsWith(TEXT("Version ")))
		{
			FString NumStr = SelectedItem.RightChop(8);
			int32 Version = FCString::Atoi(*NumStr);
			if (Version > 0)
			{
				VM->LoadDeltaByVersion(Version);
			}
		}
	}
}

void UTopBarWidget::HandleCaptureClicked(FName ButtonId, UIconTextButtonWidget* ButtonWidget)
{
	FString FileName = FString::Printf(TEXT("InteReal_Capture_%s.png"), *FDateTime::Now().ToString());
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
	
	UE_LOG(LogTemp, Log, TEXT("[TopBar] Screenshot requested: %s"), *FileName);
}
