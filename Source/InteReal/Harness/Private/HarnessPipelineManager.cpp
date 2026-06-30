#include "Public/HarnessPipelineManager.h"
#include "Public/HarnessGeneratorComponent.h"
#include "Public/HarnessJsonParser.h"
#include "Public/HarnessSaveManagerComponent.h"
#include "InteReal/EditMode/Managers/InteriorPlacementManager.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "InteReal/EditMode/Furniture/Furniture.h"
#include "InteReal/Master/InteRealPlayerController.h"
#include "InteReal/Network/InteRealNetworkSubsystem.h"
#include "InteReal/Network/ViewModel/InteRealPlanViewModel.h"
#include "UObject/UObjectIterator.h"
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
	CurrentDeltaVersion = 1;
	ClearWorld();

	if (GetWorld())
	{
		// Auto-save every 3 minutes (180 seconds)
		GetWorld()->GetTimerManager().SetTimer(AutoSaveTimerHandle, this, &UHarnessPipelineManager::SaveCurrentProject, 180.0f, true);
	}
}

void UHarnessPipelineManager::SaveCurrentProject()
{
	SaveCurrentProjectInternal(false);
}

void UHarnessPipelineManager::SaveCurrentProjectAsNewVersion()
{
	SaveCurrentProjectInternal(true);
}

void UHarnessPipelineManager::SetCurrentDeltaVersion(int32 Version)
{
	CurrentDeltaVersion = FMath::Max(Version, 1);
}

void UHarnessPipelineManager::SaveCurrentProjectInternal(bool bCreateNewVersion)
{
	if (CurrentPlanId == 0 || !SaveManagerComp) return;

	UGameInstance* GI = GetWorld()->GetGameInstance();
	if (!GI) return;

	UInteRealNetworkSubsystem* Network = GI->GetSubsystem<UInteRealNetworkSubsystem>();
	if (!Network) return;

	bSkipCameraFocusOnNextLoad = true; // 저장 후 뒤이어 발생할 리로드 때 카메라 포커싱 스킵!

	FString DeltaJson = SaveManagerComp->SaveInteriorState();
	
	FOnDeltaSaved Delegate;
	Delegate.BindDynamic(this, &UHarnessPipelineManager::HandleDeltaSaved);
	
	int32 RequestVersion = FMath::Max(CurrentDeltaVersion, 1);
	if (bCreateNewVersion)
	{
		int32 MaxVersion = 0;
		if (GetWorld())
		{
			for (TObjectIterator<UInteRealPlanViewModel> It; It; ++It)
			{
				if (It->GetWorld() == GetWorld())
				{
					for (const FUnrealDeltaVersionItem& Item : It->GetDeltaVersionList().items)
					{
						if (Item.version > MaxVersion)
						{
							MaxVersion = Item.version;
						}
					}
					break;
				}
			}
		}
		RequestVersion = FMath::Max(MaxVersion + 1, RequestVersion + 1);
	}

	Network->SaveDelta(CurrentPlanId, DeltaJson, Delegate, RequestVersion, bCreateNewVersion);
	
	UE_LOG(LogTemp, Log, TEXT("[Harness] PipelineManager: Saving Current Project %d at version %d (CreateNewVersion: %s)"),
		CurrentPlanId,
		RequestVersion,
		bCreateNewVersion ? TEXT("TRUE") : TEXT("FALSE"));
}

void UHarnessPipelineManager::HandleDeltaSaved(bool bSuccess, const FUnrealOkResponse& Response)
{
	OnPipelineSaveFinished.Broadcast(bSuccess && Response.ok, Response);

	if (bSuccess && Response.ok)
	{
		if (Response.version > 0)
		{
			CurrentDeltaVersion = Response.version;
		}
		UE_LOG(LogTemp, Log, TEXT("[Harness] PipelineManager: Save complete. Version: %d"), Response.version);
		return;
	}

	bSkipCameraFocusOnNextLoad = false; // 저장 실패 시 리로드 스킵용 플래그 해제
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
		const FHarnessFloorData& RuntimeFloorData = GeneratorComp->GetCachedFloorData();
		OnFloorPlanDataReady.Broadcast(RuntimeFloorData);
		
		if (Cast<AInteRealPlayerController>(GetWorld()->GetFirstPlayerController()))
		{
			if (UInteriorPlacementSubsystem* PlacementSubsystem = GetWorld()->GetSubsystem<UInteriorPlacementSubsystem>())
			{
				PlacementSubsystem->InitializeFromFloorData(
					RuntimeFloorData, PlacementSubsystem->GetGridCellSize());
			}
		}
		else
		{
			for (TActorIterator<AInteriorPlacementManager> It(GetWorld()); It; ++It)
			{
				(*It)->InitializeFromFloorData(RuntimeFloorData, (*It)->GridCellSize);
			}
		}
	}
}

void UHarnessPipelineManager::ApplyDelta(const FString& DeltaJson)
{
	if (!SaveManagerComp) return;

	SaveManagerComp->LoadInteriorState(DeltaJson);
	OnPipelineLoadFinished.Broadcast();
}
