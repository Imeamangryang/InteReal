#include "Public/HarnessMainHUD.h"

#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Public/HarnessNetworkComponent.h"
#include "Public/HarnessPipelineManager.h"


void UHarnessMainHUD::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_LoadProjectList)
	{
		Btn_LoadProjectList->OnClicked.AddDynamic(this, &UHarnessMainHUD::OnLoadProjectListClicked);
	}
}

void UHarnessMainHUD::SetupHUD(UHarnessNetworkComponent* InNetwork, UHarnessPipelineManager* InPipeline)
{
	NetworkComp = InNetwork;
	PipelineManager = InPipeline;

	if (NetworkComp)
	{
		NetworkComp->OnPlanListReceived.AddDynamic(this, &UHarnessMainHUD::OnPlanListReceived);
	}
}

void UHarnessMainHUD::OnLoadProjectListClicked()
{
	if (NetworkComp)
	{
		NetworkComp->RequestFloorPlanList();
	}
}

void UHarnessMainHUD::OnPlanListReceived(const TArray<FFloorPlanInfo>& PlanList)
{
	if (!ScrollBox_ProjectList) return;

	ScrollBox_ProjectList->ClearChildren();
	ProjectItemWrappers.Empty();

	for (const FFloorPlanInfo& Info : PlanList)
	{
		UButton* NewButton = NewObject<UButton>(this);
		UTextBlock* ButtonText = NewObject<UTextBlock>(this);
		
		ButtonText->SetText(FText::FromString(Info.PlanName));
		NewButton->AddChild(ButtonText);
		
		// Wrapper 객체 생성 및 바인딩
		UProjectItemWrapper* Wrapper = NewObject<UProjectItemWrapper>(this);
		Wrapper->PlanId = Info.PlanId;
		Wrapper->OwnerHUD = this;
		ProjectItemWrappers.Add(Wrapper);

		NewButton->OnClicked.AddDynamic(Wrapper, &UProjectItemWrapper::OnClick);

		ScrollBox_ProjectList->AddChild(NewButton);
	}
}

void UHarnessMainHUD::OnProjectButtonClicked(FString PlanId)
{
	if (PipelineManager)
	{
		PipelineManager->LoadProject(PlanId);
	}
}

void UProjectItemWrapper::OnClick()
{
	if (OwnerHUD)
	{
		OwnerHUD->OnProjectButtonClicked(PlanId);
	}
}
