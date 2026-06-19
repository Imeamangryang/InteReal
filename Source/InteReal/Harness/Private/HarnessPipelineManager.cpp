#include "Public/HarnessPipelineManager.h"
#include "Public/HarnessGeneratorComponent.h"
#include "Public/HarnessJsonParser.h"
#include "Public/HarnessSaveManagerComponent.h"
#include "InteReal/EditMode/Managers/InteriorPlacementManager.h"
#include "InteReal/EditMode/Furniture/Furniture.h"
#include "InteReal/Network/InteRealNetworkSubsystem.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

void UHarnessPipelineManager::InitializePipeline(UHarnessSaveManagerComponent* InSaveManager, UHarnessGeneratorComponent* InGenerator)
{
	SaveManagerComp = InSaveManager;
	GeneratorComp = InGenerator;
}

void UHarnessPipelineManager::ClearWorld()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(AutoSaveTimerHandle);
	}
	
	if (GeneratorComp) GeneratorComp->ClearHarness();
	if (SaveManagerComp) {
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), TEXT("InteriorFurniture"), FoundActors);
		for (AActor* A : FoundActors) A->Destroy();
	}
}

void UHarnessPipelineManager::LoadProject(int32 PlanId)
{
	CurrentPlanId = PlanId;
	ClearWorld();

	if (GetWorld())
	{
		// Auto-save every 3 minutes (180 seconds)
		GetWorld()->GetTimerManager().SetTimer(AutoSaveTimerHandle, this, &UHarnessPipelineManager::SaveCurrentProject, 180.0f, true);
	}
}

void UHarnessPipelineManager::SaveCurrentProject()
{
	if (CurrentPlanId == 0 || !SaveManagerComp) return;

	UGameInstance* GI = GetWorld()->GetGameInstance();
	if (!GI) return;

	UInteRealNetworkSubsystem* Network = GI->GetSubsystem<UInteRealNetworkSubsystem>();
	if (!Network) return;

	FString DeltaJson = SaveManagerComp->SaveInteriorState();
	
	FOnDeltaSaved Delegate;
	Delegate.BindDynamic(this, &UHarnessPipelineManager::HandleDeltaSaved);
	Network->SaveDelta(CurrentPlanId, DeltaJson, Delegate);
	
	UE_LOG(LogTemp, Log, TEXT("[Harness] PipelineManager: Saving Current Project %d"), CurrentPlanId);
}

void UHarnessPipelineManager::HandleDeltaSaved(bool bSuccess, const FUnrealOkResponse& Response)
{
	OnPipelineSaveFinished.Broadcast(bSuccess && Response.ok, Response);

	if (bSuccess && Response.ok)
	{
		UE_LOG(LogTemp, Log, TEXT("[Harness] PipelineManager: Save complete. Version: %d"), Response.version);
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("[Harness] PipelineManager: Save failed."));
}

void UHarnessPipelineManager::AssembleBase(const FString& BaseJson)
{
	if (!GeneratorComp) return;

	FHarnessFloorData FloorData;
	FString Error;
	if (FHarnessJsonParser::ParseFloorDataFromJsonString(BaseJson, FloorData, Error))
	{
		GeneratorComp->BuildHarness(FloorData);
		OnFloorPlanDataReady.Broadcast(FloorData);
		
		for (TActorIterator<AInteriorPlacementManager> It(GetWorld()); It; ++It)
		{
			(*It)->InitializeFromFloorData(FloorData, (*It)->GridCellSize);
		}
	}
}

void UHarnessPipelineManager::ApplyDelta(const FString& DeltaJson)
{
	if (!SaveManagerComp) return;

	SaveManagerComp->LoadInteriorState(DeltaJson);
	OnPipelineLoadFinished.Broadcast();
}
