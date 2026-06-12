#include "Public/HarnessMainHUD.h"

#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "InteReal/UI/InteRealPlanViewModel.h"
#include "InteReal/Network/InteRealNetworkSubsystem.h"
#include "Kismet/GameplayStatics.h"

void UHarnessMainHUD::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_LoadProjectList)
	{
		Btn_LoadProjectList->OnClicked.AddDynamic(this, &UHarnessMainHUD::OnLoadProjectListClicked);
	}
}

void UHarnessMainHUD::SetupHUD(UInteRealPlanViewModel* InViewModel)
{
	PlanViewModel = InViewModel;

    if (PlanViewModel)
    {
        // ViewModel의 데이터 업데이트 이벤트를 HUD의 함수에 바인딩
        PlanViewModel->OnPlanListUpdated.AddDynamic(this, &UHarnessMainHUD::OnPlanListReceived);
        UE_LOG(LogTemp, Log, TEXT("[InteReal] HUD: ViewModel event bound successfully."));
    }
}

void UHarnessMainHUD::OnLoadProjectListClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[InteReal] HUD: Load Project List button clicked."));

	if (PlanViewModel)
	{
		FUnrealPlanSearchParams Params;
		PlanViewModel->FetchPlanList(Params);
	}
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[InteReal] HUD: PlanViewModel is null! Check AHarnessManager initialization."));
    }
}

void UHarnessMainHUD::OnPlanListReceived(bool bSuccess, const FUnrealPlanListResponse& Response)
{
	UE_LOG(LogTemp, Log, TEXT("[InteReal] HUD: OnPlanListReceived called. Success: %d, Item Count: %d"), bSuccess, Response.items.Num());

	if (!bSuccess || !ScrollBox_ProjectList) return;

	ScrollBox_ProjectList->ClearChildren();
	ProjectItemWrappers.Empty();

	for (const FUnrealPlanItem& Info : Response.items)
	{
		UButton* NewButton = NewObject<UButton>(this);
		UTextBlock* ButtonText = NewObject<UTextBlock>(this);
		
		ButtonText->SetText(FText::FromString(Info.name));
		NewButton->AddChild(ButtonText);
		
		// Wrapper 객체 생성 및 바인딩
		UProjectItemWrapper* Wrapper = NewObject<UProjectItemWrapper>(this);
		Wrapper->PlanId = Info.id;
		Wrapper->OwnerHUD = this;
		ProjectItemWrappers.Add(Wrapper);

		NewButton->OnClicked.AddDynamic(Wrapper, &UProjectItemWrapper::OnClick);

		ScrollBox_ProjectList->AddChild(NewButton);
	}
    
    UE_LOG(LogTemp, Log, TEXT("[InteReal] HUD: Project list UI populated with %d items."), Response.items.Num());
}

void UHarnessMainHUD::OnProjectButtonClicked(int32 PlanId)
{
	UE_LOG(LogTemp, Log, TEXT("[InteReal] HUD: Project item %d clicked. Starting load..."), PlanId);

	if (PlanViewModel)
	{
		for (const FUnrealPlanItem& Item : PlanViewModel->GetPlanList().items)
		{
			if (Item.id == PlanId)
			{
				PlanViewModel->LoadPlanProject(Item);
				break;
			}
		}
	}
}

void UProjectItemWrapper::OnClick()
{
	if (OwnerHUD)
	{
		OwnerHUD->OnProjectButtonClicked(PlanId);
	}
}
