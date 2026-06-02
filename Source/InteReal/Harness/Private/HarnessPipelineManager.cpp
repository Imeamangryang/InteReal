#include "Public/HarnessPipelineManager.h"

#include "Public/HarnessGeneratorComponent.h"
#include "Public/HarnessJsonParser.h"
#include "Public/HarnessNetworkComponent.h"
#include "Public/HarnessSaveManagerComponent.h"


UHarnessPipelineManager::UHarnessPipelineManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHarnessPipelineManager::BeginPlay()
{
	Super::BeginPlay();
}

void UHarnessPipelineManager::InitializePipeline(UHarnessNetworkComponent* InNetwork, UHarnessSaveManagerComponent* InSaveManager, UHarnessGeneratorComponent* InGenerator)
{
	NetworkComp = InNetwork;
	SaveManagerComp = InSaveManager;
	GeneratorComp = InGenerator;

	if (NetworkComp)
	{
		NetworkComp->OnPlanBaseDownloaded.AddDynamic(this, &UHarnessPipelineManager::OnBaseDownloaded);
		NetworkComp->OnPlanDeltaDownloaded.AddDynamic(this, &UHarnessPipelineManager::OnDeltaDownloaded);
	}
}

void UHarnessPipelineManager::LoadProject(const FString& PlanId)
{
	if (!NetworkComp || !GeneratorComp || !SaveManagerComp) return;

	CurrentPlanId = PlanId;
	
	GeneratorComp->ClearHarness(); 

	NetworkComp->DownloadFloorPlanBase(CurrentPlanId);
}

void UHarnessPipelineManager::SaveCurrentProject()
{
	if (!SaveManagerComp || !NetworkComp || CurrentPlanId.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Harness] 저장 실패: SaveManager가 없거나 현재 도면 ID가 유효하지 않습니다."));
		return;
	}

	// 1. 현재 씬의 가구 상태를 JSON 문자열로 추출
	FString InteriorJson = SaveManagerComp->SaveInteriorState();

	// 2. 네트워크 컴포넌트를 통해 서버(또는 Mock)로 업로드
	NetworkComp->UploadFloorPlanDelta(CurrentPlanId, InteriorJson);

	UE_LOG(LogTemp, Log, TEXT("[Harness] 프로젝트 '%s' 저장 요청 완료"), *CurrentPlanId);
}

void UHarnessPipelineManager::OnBaseDownloaded(const FString& BaseJson)
{
	if (!GeneratorComp || !NetworkComp) return;

	FHarnessFloorData FloorData;
	FString OutError;
	if (FHarnessJsonParser::ParseFloorDataFromJsonString(BaseJson, FloorData, OutError))
	{
		GeneratorComp->BuildHarness(FloorData);
		NetworkComp->DownloadFloorPlanDelta(CurrentPlanId);
	}
}

void UHarnessPipelineManager::OnDeltaDownloaded(const FString& DeltaJson)
{
	if (!SaveManagerComp) return;

	SaveManagerComp->LoadInteriorState(DeltaJson);
	OnPipelineLoadFinished.Broadcast();
}
