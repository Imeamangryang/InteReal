#include "Public/HarnessSaveManagerComponent.h"

#include "Public/HarnessPipelineManager.h"
#include "Public/HarnessGeneratorComponent.h"
#include "Components/MeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "JsonObjectConverter.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DataTable.h"
#include "InteReal/EditMode/Furniture/Furniture.h"
#include "InteReal/EditMode/Managers/InteriorPlacementManager.h"

namespace
{
	FString FindHarnessSurfaceId(const UMeshComponent* MeshComp)
	{
		if (!MeshComp)
		{
			return FString();
		}

		static const TCHAR* PreferredPrefixes[] =
		{
			TEXT("WallSurface_"),
			TEXT("WallExterior_"),
			TEXT("FloorFace_"),
			TEXT("WallEdge_")
		};

		for (const TCHAR* Prefix : PreferredPrefixes)
		{
			for (const FName& Tag : MeshComp->ComponentTags)
			{
				const FString TagStr = Tag.ToString();
				if (TagStr.StartsWith(Prefix))
				{
					return TagStr;
				}
			}
		}

		return FString();
	}
}

UHarnessSaveManagerComponent::UHarnessSaveManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FString UHarnessSaveManagerComponent::SaveInteriorState()
{
	FInteriorDeltaList DeltaList;
	
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFurniture::StaticClass(), FoundActors);
	
	UE_LOG(LogTemp, Log, TEXT("[SaveManager] Found %d AFurniture actors."), FoundActors.Num());

	for (AActor* Actor : FoundActors)
	{
		AFurniture* Furn = Cast<AFurniture>(Actor);
		if (!Furn || Furn->GetPlacementState() != EPlacementState::Placed) 
		{
			if (Furn) UE_LOG(LogTemp, Warning, TEXT("[SaveManager] Skipping furniture %d (State: %d)"), Furn->FurnitureID, (int32)Furn->GetPlacementState());
			continue;
		}

		FFurnitureDelta Delta;
		Delta.Transform = Actor->GetActorTransform();
		Delta.FurnitureID = FName(FString::FromInt(Furn->FurnitureID));
		
		DeltaList.FurnitureItems.Add(Delta);
	}

	int32 SurfaceCount = 0;
	if (UHarnessPipelineManager* Pipeline = GetWorld()->GetSubsystem<UHarnessPipelineManager>())
	{
		if (UHarnessGeneratorComponent* GenComp = Pipeline->GetGeneratorComp())
		{
			if (AActor* HarnessOwner = GenComp->GetOwner())
			{
				TArray<UMeshComponent*> MeshComps;
				HarnessOwner->GetComponents<UMeshComponent>(MeshComps);
				for (UMeshComponent* MeshComp : MeshComps)
				{
					if (MeshComp->ComponentHasTag(TEXT("EditableWall")) || MeshComp->ComponentHasTag(TEXT("EditableFloor")))
					{
						UMaterialInterface* Mat = MeshComp->GetMaterial(0);
						if (Mat)
						{
							const FString SurfaceID = FindHarnessSurfaceId(MeshComp);
							if (!SurfaceID.IsEmpty())
							{
								FSurfaceMaterialDelta MatDelta;
								MatDelta.SurfaceID = SurfaceID;
								MatDelta.MaterialPath = Mat->GetPathName();
								DeltaList.SurfaceMaterials.Add(MatDelta);
								SurfaceCount++;
							}
						}
					}
				}
			}
		}
	}

	FString OutputString;
	FJsonObjectConverter::UStructToJsonObjectString(DeltaList, OutputString, 0, 0);
	
	UE_LOG(LogTemp, Log, TEXT("[SaveManager] Serialization complete. Furniture: %d, Surfaces: %d, JSON Length: %d"), 
		DeltaList.FurnitureItems.Num(), SurfaceCount, OutputString.Len());
		
	return OutputString;
}

void UHarnessSaveManagerComponent::LoadInteriorState(const FString& JsonString)
{
	if (JsonString.IsEmpty() || JsonString == TEXT("{}")) return;

	ClearInterior();

	FInteriorDeltaList DeltaList;
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

		if (UHarnessPipelineManager* Pipeline = GetWorld()->GetSubsystem<UHarnessPipelineManager>())
		{
			if (UHarnessGeneratorComponent* GenComp = Pipeline->GetGeneratorComp())
			{
				if (AActor* HarnessOwner = GenComp->GetOwner())
				{
					TArray<UMeshComponent*> MeshComps;
					HarnessOwner->GetComponents<UMeshComponent>(MeshComps);
					for (UMeshComponent* MeshComp : MeshComps)
					{
						for (const FSurfaceMaterialDelta& MatDelta : DeltaList.SurfaceMaterials)
						{
							if (MeshComp->ComponentHasTag(FName(*MatDelta.SurfaceID)))
							{
								UMaterialInterface* LoadedMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MatDelta.MaterialPath));
								if (LoadedMat)
								{
									MeshComp->SetMaterial(0, LoadedMat);
								}
								break;
							}
						}
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
