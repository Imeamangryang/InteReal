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

void UInteRealUISubSystem::NotifyWallMaterialChanged(UMaterialInterface* NewMaterial)
{
	UE_LOG(LogTemp, Warning, TEXT("[WallMaterialDebug] NotifyWallMaterialChanged: NewMaterial=%s ListenerCount=%d"),
		NewMaterial ? *NewMaterial->GetPathName() : TEXT("<null>"),
		OnWallMaterialChanged.IsBound() ? 1 : 0);

	OnWallMaterialChanged.Broadcast(NewMaterial);
}

void UInteRealUISubSystem::NotifyViewModeChanged(EHarnessViewMode NewMode)
{
	OnViewModeChanged.Broadcast(NewMode);
}

void UInteRealUISubSystem::NotifyIconClicked(FName Command)
{
	OnIconClicked.Broadcast(Command);
}
