#include "InteRealUISubSystem.h"
#include "Materials/MaterialInterface.h"

void UInteRealUISubSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UInteRealUISubSystem::Deinitialize()
{
	Super::Deinitialize();
}

void UInteRealUISubSystem::NotifyModeChanged(bool bIsEditMode)
{
	OnModeChanged.Broadcast(bIsEditMode);
}

void UInteRealUISubSystem::NotifyFurnitureSpawn(const FFurnitureDataRow& FurnitureData)
{
	OnFurnitureSpawn.Broadcast(FurnitureData);
}

void UInteRealUISubSystem::NotifyViewModeChanged(EHarnessViewMode NewMode)
{
	OnViewModeChanged.Broadcast(NewMode);
}

void UInteRealUISubSystem::NotifyIconClicked(FName Command)
{
	OnIconClicked.Broadcast(Command);
}

void UInteRealUISubSystem::NotifyWallMaterialDataChanged(const FMaterialDataRow& MaterialData)
{
	UE_LOG(LogTemp, Log, TEXT("[Material] NotifyWallMaterialDataChanged: Name=%s DisplayImage=%s"),
		*MaterialData.DisplayName.ToString(),
		MaterialData.DisplayImage ? *MaterialData.DisplayImage->GetPathName() : TEXT("<null>"));

	OnWallMaterialDataChanged.Broadcast(MaterialData);
}

void UInteRealUISubSystem::NotifyOpeningAssetSelected(const FOpeningAssetDataRow& OpeningData)
{
	OnOpeningAssetSelected.Broadcast(OpeningData);
}
