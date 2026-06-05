#include "InteRealUISubSystem.h"

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

void UInteRealUISubSystem::NotifyWallMaterialChanged(UMaterialInterface* NewMaterial)
{
	OnWallMaterialChanged.Broadcast(NewMaterial);
}
