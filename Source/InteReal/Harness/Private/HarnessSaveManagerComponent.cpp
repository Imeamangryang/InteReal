#include "Public/HarnessSaveManagerComponent.h"

#include "Kismet/GameplayStatics.h"
#include "JsonObjectConverter.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DataTable.h"
#include "InteReal/EditMode/Furnitures/Furniture.h"
#include "InteReal/EditMode/Managers/InteriorPlacementManager.h"

UHarnessSaveManagerComponent::UHarnessSaveManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FString UHarnessSaveManagerComponent::SaveInteriorState()
{
	FFurnitureDeltaList DeltaList;
	
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), TEXT("InteriorFurniture"), FoundActors);

	for (AActor* Actor : FoundActors)
	{
		FFurnitureDelta Delta;
		Delta.Transform = Actor->GetActorTransform();
		
		for (const FName& Tag : Actor->Tags)
		{
			FString TagStr = Tag.ToString();
			if (TagStr.StartsWith(TEXT("ID_")))
			{
				Delta.FurnitureID = FName(*TagStr.RightChop(3));
				break;
			}
		}
		
		DeltaList.FurnitureItems.Add(Delta);
	}

	FString OutputString;
	FJsonObjectConverter::UStructToJsonObjectString(DeltaList, OutputString, 0, 0);
	return OutputString;
}

void UHarnessSaveManagerComponent::LoadInteriorState(const FString& JsonString)
{
	if (JsonString.IsEmpty() || JsonString == TEXT("{}")) return;

	ClearInterior();

	FFurnitureDeltaList DeltaList;
	if (FJsonObjectConverter::JsonObjectStringToUStruct(JsonString, &DeltaList, 0, 0))
	{
		// InteriorPlacementManager를 찾아 가구 스폰 지원 요청
		AInteriorPlacementManager* PlacementManager = nullptr;
		TArray<AActor*> FoundManagers;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AInteriorPlacementManager::StaticClass(), FoundManagers);
		if (FoundManagers.Num() > 0) PlacementManager = Cast<AInteriorPlacementManager>(FoundManagers[0]);

		for (const FFurnitureDelta& Delta : DeltaList.FurnitureItems)
		{
			if (PlacementManager)
			{
				// ID를 숫자로 변환 (AInteriorPlacementManager가 int32 ID를 사용하므로)
				int32 FurnID = FCString::Atoi(*Delta.FurnitureID.ToString());
				const FFurnitureDataRow* Row = PlacementManager->FindFurnitureRowByID(FurnID);

				if (Row && PlacementManager->FurnitureClass)
				{
					FActorSpawnParameters SpawnParams;
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					
					AFurniture* SpawnedActor = GetWorld()->SpawnActor<AFurniture>(PlacementManager->FurnitureClass, Delta.Transform, SpawnParams);
					if (SpawnedActor)
					{
						SpawnedActor->ApplyFurnitureRow(*Row);
						SpawnedActor->FurnitureID = FurnID;
						SpawnedActor->SetPlacementState(EPlacementState::Placed);
						
						// 저장용 태그 복구
						SpawnedActor->Tags.Add(TEXT("InteriorFurniture"));
						SpawnedActor->Tags.Add(FName(FString::Printf(TEXT("ID_%d"), FurnID)));
					}
				}
			}
		}
	}
}

void UHarnessSaveManagerComponent::ClearInterior()
{
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), TEXT("InteriorFurniture"), FoundActors);
	for (AActor* Actor : FoundActors)
	{
		Actor->Destroy();
	}
}
