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
	ResetAutoSaveTracking();
}

void UHarnessPipelineManager::SaveCurrentProject()
{
	SaveCurrentProjectInternal(false);
}

void UHarnessPipelineManager::SaveCurrentProjectAsNewVersion()
{
	SaveCurrentProjectInternal(true);
}

void UHarnessPipelineManager::NotifyUserEditedProject()
{
	if (CurrentPlanId == 0)
	{
		return;
	}

	++ProjectChangeSerial;
	bHasPendingUserChanges = true;
	ScheduleAutoSave();
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

	PendingSavedDeltaJson = DeltaJson;
	PendingSaveChangeSerial = ProjectChangeSerial;

	Network->SaveDelta(CurrentPlanId, DeltaJson, Delegate, RequestVersion, bCreateNewVersion);
	
	UE_LOG(LogTemp, Log, TEXT("[Harness] PipelineManager: Saving Current Project %d at version %d (CreateNewVersion: %s)"),
		CurrentPlanId,
		RequestVersion,
		bCreateNewVersion ? TEXT("TRUE") : TEXT("FALSE"));
}

bool UHarnessPipelineManager::SaveCurrentProjectInternalWithDelta(bool bCreateNewVersion, const FString& DeltaJson)
{
	if (CurrentPlanId == 0 || !SaveManagerComp) return false;

	UGameInstance* GI = GetWorld()->GetGameInstance();
	if (!GI) return false;

	UInteRealNetworkSubsystem* Network = GI->GetSubsystem<UInteRealNetworkSubsystem>();
	if (!Network) return false;

	bSkipCameraFocusOnNextLoad = true;

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

	PendingSavedDeltaJson = DeltaJson;
	PendingSaveChangeSerial = ProjectChangeSerial;

	Network->SaveDelta(CurrentPlanId, DeltaJson, Delegate, RequestVersion, bCreateNewVersion);

	UE_LOG(LogTemp, Log, TEXT("[Harness] PipelineManager: Saving Current Project %d at version %d (CreateNewVersion: %s)"),
		CurrentPlanId,
		RequestVersion,
		bCreateNewVersion ? TEXT("TRUE") : TEXT("FALSE"));
	return true;
}

void UHarnessPipelineManager::HandleAutoSaveTimer()
{
	if (!bHasPendingUserChanges || CurrentPlanId == 0 || !SaveManagerComp)
	{
		return;
	}

	const FString CurrentDeltaJson = SaveManagerComp->SaveInteriorState();
	if (CurrentDeltaJson == LastSavedDeltaJson)
	{
		bHasPendingUserChanges = false;
		UE_LOG(LogTemp, Log, TEXT("[Harness] PipelineManager: Auto-save skipped; no state changes."));
		return;
	}

	if (!SaveCurrentProjectInternalWithDelta(false, CurrentDeltaJson))
	{
		ScheduleAutoSave();
	}
}

void UHarnessPipelineManager::ScheduleAutoSave()
{
	if (!GetWorld())
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(AutoSaveTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(
		AutoSaveTimerHandle,
		this,
		&UHarnessPipelineManager::HandleAutoSaveTimer,
		AutoSaveDelaySeconds,
		false);
}

void UHarnessPipelineManager::ResetAutoSaveTracking()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(AutoSaveTimerHandle);
	}
	LastSavedDeltaJson.Empty();
	PendingSavedDeltaJson.Empty();
	ProjectChangeSerial = 0;
	PendingSaveChangeSerial = 0;
	bHasPendingUserChanges = false;
}

void UHarnessPipelineManager::CaptureCurrentStateAsSavedBaseline()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(AutoSaveTimerHandle);
	}
	LastSavedDeltaJson = SaveManagerComp ? SaveManagerComp->SaveInteriorState() : FString();
	PendingSavedDeltaJson.Empty();
	ProjectChangeSerial = 0;
	PendingSaveChangeSerial = 0;
	bHasPendingUserChanges = false;
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
		LastSavedDeltaJson = PendingSavedDeltaJson;
		PendingSavedDeltaJson.Empty();
		if (PendingSaveChangeSerial == ProjectChangeSerial)
		{
			bHasPendingUserChanges = false;
			if (GetWorld())
			{
				GetWorld()->GetTimerManager().ClearTimer(AutoSaveTimerHandle);
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[Harness] PipelineManager: Save complete. Version: %d"), Response.version);
		return;
	}

	bSkipCameraFocusOnNextLoad = false; // 저장 실패 시 리로드 스킵용 플래그 해제
	PendingSavedDeltaJson.Empty();
	if (bHasPendingUserChanges)
	{
		ScheduleAutoSave();
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
	CaptureCurrentStateAsSavedBaseline();
	OnPipelineLoadFinished.Broadcast();
}
