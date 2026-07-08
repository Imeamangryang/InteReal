#include "Public/HarnessSaveManagerComponent.h"

#include "Public/HarnessPipelineManager.h"
#include "Public/HarnessGeneratorComponent.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "JsonObjectConverter.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DataTable.h"
#include "InteReal/EditMode/Furniture/Furniture.h"
#include "InteReal/EditMode/Furniture/LightFixture.h"
#include "InteReal/EditMode/Managers/InteriorPlacementManager.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "InteReal/EditMode/Managers/GridSpaceManager.h"
#include "InteReal/EditMode/Visualization/PlacementVisualizerActor.h"
#include "InteReal/Master/InteRealPlayerController.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	bool HarnessSave_IsDoorLikeKind(EPlacementAssetKind Kind)
	{
		return Kind == EPlacementAssetKind::Door ||
		       Kind == EPlacementAssetKind::EntranceDoor ||
		       Kind == EPlacementAssetKind::SlidingDoor;
	}

	bool HarnessSave_IsWindowLikeName(const FString& Label)
	{
		return Label.Contains(TEXT("window")) ||
		       Label.Contains(TEXT("\ucc3d\ubb38")) ||
		       Label.Contains(TEXT("\ucc3d\ud638")) ||
		       Label.Contains(TEXT("\ucc3d"));
	}

	bool HarnessSave_IsDoorLikeName(const FString& Label)
	{
		return Label.Contains(TEXT("door")) ||
		       Label.Contains(TEXT("\ubb38")) ||
		       Label.Contains(TEXT("\ud604\uad00")) ||
		       Label.Contains(TEXT("\ubbf8\ub2eb")) ||
		       Label.Contains(TEXT("\uc2ac\ub77c\uc774\ub529"));
	}

	bool HarnessSave_IsOpeningFurnitureRow(const FFurnitureDataRow& Row)
	{
		if (Row.AssetKind != EPlacementAssetKind::Generic)
		{
			return true;
		}

		const FString Label = Row.DisplayName.ToString().ToLower();
		return HarnessSave_IsDoorLikeName(Label) || HarnessSave_IsWindowLikeName(Label);
	}

	bool HarnessSave_IsWindowFurnitureRow(const FFurnitureDataRow& Row)
	{
		if (Row.AssetKind == EPlacementAssetKind::Window)
		{
			return true;
		}
		if (HarnessSave_IsDoorLikeKind(Row.AssetKind))
		{
			return false;
		}
		return HarnessSave_IsWindowLikeName(Row.DisplayName.ToString().ToLower());
	}

	void HarnessSave_RestoreOpeningFurnitureTags(AFurniture* Furniture, const FFurnitureDataRow& Row)
	{
		if (!Furniture || !HarnessSave_IsOpeningFurnitureRow(Row))
		{
			return;
		}

		Furniture->Tags.AddUnique(FName(TEXT("OpeningAsset")));
		Furniture->Tags.AddUnique(FName(HarnessSave_IsWindowFurnitureRow(Row) ? TEXT("WindowAsset") : TEXT("DoorAsset")));
	}

	uint8 HarnessSave_AllLightPlacementTypes()
	{
		return static_cast<uint8>(EPlacementSurfaceType::Floor)
		     | static_cast<uint8>(EPlacementSurfaceType::Wall)
		     | static_cast<uint8>(EPlacementSurfaceType::Ceiling);
	}

	EFurnitureAssetCategory HarnessSave_GetAssetCategory(const AFurniture* Furniture)
	{
		if (!Furniture)
		{
			return EFurnitureAssetCategory::None;
		}
		if (Cast<ALightFixture>(Furniture))
		{
			return EFurnitureAssetCategory::Lighting;
		}
		return Furniture->HasFurnitureDataRow()
			? Furniture->GetFurnitureDataRow().Category
			: EFurnitureAssetCategory::None;
	}

	FLightAttributes HarnessSave_GetLightAttributes(const AFurniture* Furniture)
	{
		if (const ALightFixture* LightFixture = Cast<ALightFixture>(Furniture))
		{
			return LightFixture->GetLightAttributes();
		}
		return Furniture && Furniture->HasFurnitureDataRow()
			? Furniture->GetFurnitureDataRow().LightAttributes
			: FLightAttributes();
	}

	FFurnitureDataRow HarnessSave_MakeLightRowFromDelta(int32 FurnitureID, const FFurnitureDelta& Delta, const FFurnitureDataRow* ExistingRow)
	{
		FFurnitureDataRow Row = ExistingRow ? *ExistingRow : FFurnitureDataRow();
		Row.ID = FurnitureID;
		Row.Category = EFurnitureAssetCategory::Lighting;
		Row.LightAttributes = Delta.LightAttributes;
		Row.AllowedPlacementTypes = HarnessSave_AllLightPlacementTypes();
		return Row;
	}

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
			TEXT("WallEdge_"),
			TEXT("WallCore_"),
			TEXT("Opening_")
		};

		for (const TCHAR* Prefix : PreferredPrefixes)
		{
			for (const FName& Tag : MeshComp->ComponentTags)
			{
				const FString TagStr = Tag.ToString();
				if (TagStr.StartsWith(Prefix))
				{
					if (TagStr == TEXT("WallCore_Left") || TagStr == TEXT("WallCore_Right"))
					{
						continue;
					}
					return TagStr;
				}
			}
		}

		return FString();
	}

	void HarnessSave_CaptureSurfaceMaterial(UMeshComponent* MeshComp, FSurfaceMaterialDelta& MatDelta)
	{
		if (!MeshComp)
		{
			return;
		}

		UMaterialInterface* Material = MeshComp->GetMaterial(0);
		if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Material))
		{
			if (MID->Parent)
			{
				MatDelta.MaterialPath = MID->Parent->GetPathName();
			}

			if (UTexture* BaseColorTexture = MID->K2_GetTextureParameterValue(TEXT("BaseColorTexture")))
			{
				MatDelta.BaseColorTexturePath = BaseColorTexture->GetPathName();
			}

			MatDelta.bHasMaterialAttributes = true;
			MatDelta.Metallic = MID->K2_GetScalarParameterValue(TEXT("Metallic"));
			MatDelta.Specular = MID->K2_GetScalarParameterValue(TEXT("Specular"));
			MatDelta.Roughness = MID->K2_GetScalarParameterValue(TEXT("Roughness"));
			MatDelta.Emissive = MID->K2_GetScalarParameterValue(TEXT("Emissive"));
			MatDelta.TextureTiling = FMath::Max(MID->K2_GetScalarParameterValue(TEXT("TextureTiling")), 0.01f);
			return;
		}

		if (Material)
		{
			MatDelta.MaterialPath = Material->GetPathName();
		}
	}

	void HarnessSave_ApplySurfaceMaterial(UMeshComponent* MeshComp, const FSurfaceMaterialDelta& MatDelta)
	{
		if (!MeshComp || MatDelta.MaterialPath.IsEmpty())
		{
			return;
		}

		UMaterialInterface* LoadedMat = Cast<UMaterialInterface>(
			StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MatDelta.MaterialPath));
		if (!LoadedMat)
		{
			return;
		}

		if (!MatDelta.bHasMaterialAttributes && MatDelta.BaseColorTexturePath.IsEmpty())
		{
			MeshComp->SetMaterial(0, LoadedMat);
			return;
		}

		UMaterialInstanceDynamic* SurfaceMID = UMaterialInstanceDynamic::Create(LoadedMat, MeshComp);
		if (!SurfaceMID)
		{
			MeshComp->SetMaterial(0, LoadedMat);
			return;
		}

		if (!MatDelta.BaseColorTexturePath.IsEmpty())
		{
			if (UTexture* BaseColorTexture = Cast<UTexture>(
				StaticLoadObject(UTexture::StaticClass(), nullptr, *MatDelta.BaseColorTexturePath)))
			{
				SurfaceMID->SetTextureParameterValue(TEXT("BaseColorTexture"), BaseColorTexture);
			}
		}

		if (MatDelta.bHasMaterialAttributes)
		{
			SurfaceMID->SetScalarParameterValue(TEXT("Metallic"), MatDelta.Metallic);
			SurfaceMID->SetScalarParameterValue(TEXT("Specular"), MatDelta.Specular);
			SurfaceMID->SetScalarParameterValue(TEXT("Roughness"), MatDelta.Roughness);
			SurfaceMID->SetScalarParameterValue(TEXT("Emissive"), MatDelta.Emissive);
			SurfaceMID->SetScalarParameterValue(TEXT("TextureTiling"), FMath::Max(MatDelta.TextureTiling, 0.01f));
		}

		MeshComp->SetMaterial(0, SurfaceMID);
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
	TArray<AFurniture*> PlacedFurnitures;
	for (AActor* Actor : FoundActors)
	{
		AFurniture* Furniture = Cast<AFurniture>(Actor);
		if (Furniture && Furniture->GetPlacementState() == EPlacementState::Placed)
		{
			PlacedFurnitures.Add(Furniture);
			continue;
		}
		if (Furniture)
		{
			UE_LOG(LogTemp, Warning, TEXT("[SaveManager] Skipping furniture %d (State: %d)"), Furniture->FurnitureID, (int32)Furniture->GetPlacementState());
		}
	}

	PlacedFurnitures.Sort([](const AFurniture& A, const AFurniture& B)
	{
		const FVector ALocation = A.GetActorLocation();
		const FVector BLocation = B.GetActorLocation();
		const FRotator ARotation = A.GetActorRotation();
		const FRotator BRotation = B.GetActorRotation();
		const FString AKey = FString::Printf(TEXT("%08d|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%s"),
			A.FurnitureID, ALocation.X, ALocation.Y, ALocation.Z, ARotation.Pitch, ARotation.Yaw, ARotation.Roll, *A.GetName());
		const FString BKey = FString::Printf(TEXT("%08d|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%s"),
			B.FurnitureID, BLocation.X, BLocation.Y, BLocation.Z, BRotation.Pitch, BRotation.Yaw, BRotation.Roll, *B.GetName());
		return AKey < BKey;
	});

	TMap<const AFurniture*, int32> FurnitureIndices;
	for (const AFurniture* Furniture : PlacedFurnitures)
	{
		FurnitureIndices.Add(Furniture, FurnitureIndices.Num());
	}

	for (AFurniture* Furn : PlacedFurnitures)
	{
		FFurnitureDelta Delta;
		Delta.Transform = Furn->GetActorTransform();
		Delta.FurnitureID = FName(FString::FromInt(Furn->FurnitureID));
		Delta.AssetCategory = HarnessSave_GetAssetCategory(Furn);
		Delta.LightAttributes = HarnessSave_GetLightAttributes(Furn);
		Delta.SurfaceType = static_cast<uint8>(Furn->GetPlacedSurfaceType());
		Delta.GridAnchor = Furn->PlacedGridAnchor;
		Delta.Dimensions = Furn->PlacedDimensions;
		Delta.WallNormal = Furn->WallNormalAtPlacement;
		Delta.ParentIndex = FurnitureIndices.Contains(Furn->ParentFurniture)
			? FurnitureIndices[Furn->ParentFurniture]
			: INDEX_NONE;
		
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
					const FString SurfaceID = FindHarnessSurfaceId(MeshComp);
					if (!SurfaceID.IsEmpty())
					{
						FSurfaceMaterialDelta MatDelta;
						MatDelta.SurfaceID = SurfaceID;
						HarnessSave_CaptureSurfaceMaterial(MeshComp, MatDelta);
						if (const UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(MeshComp))
						{
							if (StaticMeshComp->ComponentHasTag(TEXT("EditableOpening")))
							{
								if (UStaticMesh* StaticMesh = StaticMeshComp->GetStaticMesh())
								{
									MatDelta.MeshPath = StaticMesh->GetPathName();
									MatDelta.RelativeScale = StaticMeshComp->GetRelativeScale3D();
								}
							}
						}
						if (!MatDelta.MaterialPath.IsEmpty() ||
							!MatDelta.BaseColorTexturePath.IsEmpty() ||
							MatDelta.bHasMaterialAttributes ||
							!MatDelta.MeshPath.IsEmpty())
						{
							DeltaList.SurfaceMaterials.Add(MatDelta);
							SurfaceCount++;
						}
					}
				}
			}
		}
	}

	DeltaList.SurfaceMaterials.Sort([](const FSurfaceMaterialDelta& A, const FSurfaceMaterialDelta& B)
	{
		return A.SurfaceID < B.SurfaceID;
	});

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
		bool bLoadedIntoSubsystem = false;
		if (Cast<AInteRealPlayerController>(GetWorld()->GetFirstPlayerController()))
		{
			UInteriorPlacementSubsystem* PlacementSubsystem = GetWorld()->GetSubsystem<UInteriorPlacementSubsystem>();
			APlacementVisualizerActor* Visualizer = PlacementSubsystem ? PlacementSubsystem->GetVisualizer() : nullptr;
			if (PlacementSubsystem && Visualizer && Visualizer->FurnitureClass)
			{
				TMap<int32, AFurniture*> LoadedBySourceIndex;
				for (int32 SourceIndex = 0; SourceIndex < DeltaList.FurnitureItems.Num(); ++SourceIndex)
				{
					const FFurnitureDelta& Delta = DeltaList.FurnitureItems[SourceIndex];
					const int32 FurnitureID = FCString::Atoi(*Delta.FurnitureID.ToString());
					FFurnitureDataRow ResolvedLightRow;
					const FFurnitureDataRow* Row = PlacementSubsystem->FindFurnitureRowByID(FurnitureID);
					if (Delta.AssetCategory == EFurnitureAssetCategory::Lighting)
					{
						const FFurnitureDataRow* LightSourceRow = Row && Row->Category == EFurnitureAssetCategory::Lighting
							? Row
							: nullptr;
						ResolvedLightRow = HarnessSave_MakeLightRowFromDelta(FurnitureID, Delta, LightSourceRow);
						Row = &ResolvedLightRow;
					}
					if (!Row)
					{
						continue;
					}

					FActorSpawnParameters SpawnParams;
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					const TSubclassOf<AFurniture> SpawnClass = AFurniture::ResolveSpawnClass(
						*Row, Visualizer->FurnitureClass, Visualizer->LightFixtureClass);
					AFurniture* SpawnedActor = GetWorld()->SpawnActor<AFurniture>(
						SpawnClass, Delta.Transform, SpawnParams);
					if (!SpawnedActor)
					{
						continue;
					}

					SpawnedActor->ApplyFurnitureRow(*Row);
					// 💡 [수정 사항] 저장된 가구의 스케일(기본/수정된 값 모두 포함)을 복원합니다. 
					// ApplyFurnitureRow가 스케일을 덮어쓰기 때문에 다시 적용해야 합니다.
					SpawnedActor->SetActorScale3D(Delta.Transform.GetScale3D());
					
					SpawnedActor->PlacedGridAnchor = Delta.GridAnchor;
					SpawnedActor->PlacedDimensions = Delta.Dimensions.IsNearlyZero()
						? FVector2D(Row->Dimensions.X, Row->Dimensions.Y)
						: Delta.Dimensions;
					SpawnedActor->SetPlacedSurfaceType(static_cast<EPlacementSurfaceType>(Delta.SurfaceType));
					SpawnedActor->WallNormalAtPlacement = Delta.WallNormal;
					SpawnedActor->Tags.AddUnique(FName(TEXT("InteriorFurniture")));
					SpawnedActor->Tags.AddUnique(FName(FString::Printf(TEXT("ID_%d"), FurnitureID)));
					HarnessSave_RestoreOpeningFurnitureTags(SpawnedActor, *Row);
					SpawnedActor->SetPlacementState(EPlacementState::Placed);
					PlacementSubsystem->GetPlacedFurnituresMutable().Add(SpawnedActor);

					if (SpawnedActor->GetPlacedSurfaceType() == EPlacementSurfaceType::Floor)
					{
						if (AGridSpaceManager* Grid = PlacementSubsystem->GetGrid())
						{
							for (int32 X = 0; X < (int32)SpawnedActor->PlacedDimensions.X; ++X)
							{
								for (int32 Y = 0; Y < (int32)SpawnedActor->PlacedDimensions.Y; ++Y)
								{
									Grid->SetFurniture(SpawnedActor->PlacedGridAnchor + FVector2D(X, Y), SpawnedActor);
								}
							}
						}
					}

					LoadedBySourceIndex.Add(SourceIndex, SpawnedActor);
				}

				for (int32 SourceIndex = 0; SourceIndex < DeltaList.FurnitureItems.Num(); ++SourceIndex)
				{
					AFurniture* const* ChildPtr = LoadedBySourceIndex.Find(SourceIndex);
					const int32 ParentIndex = DeltaList.FurnitureItems[SourceIndex].ParentIndex;
					AFurniture* const* ParentPtr = LoadedBySourceIndex.Find(ParentIndex);
					if (ChildPtr && ParentPtr &&
						(*ChildPtr)->GetPlacedSurfaceType() == EPlacementSurfaceType::Surface)
					{
						(*ChildPtr)->ParentFurniture = *ParentPtr;
						(*ChildPtr)->AttachToActor(*ParentPtr, FAttachmentTransformRules::KeepWorldTransform);
					}
				}
				bLoadedIntoSubsystem = true;
			}
		}

		// Legacy maps still use the actor-based placement manager.
		AInteriorPlacementManager* PlacementManager = nullptr;
		if (!bLoadedIntoSubsystem)
		{
			TArray<AActor*> FoundManagers;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), AInteriorPlacementManager::StaticClass(), FoundManagers);
			if (FoundManagers.Num() > 0) PlacementManager = Cast<AInteriorPlacementManager>(FoundManagers[0]);
		}

		for (const FFurnitureDelta& Delta : DeltaList.FurnitureItems)
		{
			if (PlacementManager)
			{
				// ID를 숫자로 변환 (AInteriorPlacementManager가 int32 ID를 사용하므로)
				int32 FurnID = FCString::Atoi(*Delta.FurnitureID.ToString());
				FFurnitureDataRow ResolvedLightRow;
				const FFurnitureDataRow* Row = PlacementManager->FindFurnitureRowByID(FurnID);
				if (Delta.AssetCategory == EFurnitureAssetCategory::Lighting)
				{
					const FFurnitureDataRow* LightSourceRow = Row && Row->Category == EFurnitureAssetCategory::Lighting
						? Row
						: nullptr;
					ResolvedLightRow = HarnessSave_MakeLightRowFromDelta(FurnID, Delta, LightSourceRow);
					Row = &ResolvedLightRow;
				}

				if (Row && PlacementManager->FurnitureClass)
				{
					FActorSpawnParameters SpawnParams;
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					
					const TSubclassOf<AFurniture> SpawnClass = AFurniture::ResolveSpawnClass(*Row, PlacementManager->FurnitureClass);
					AFurniture* SpawnedActor = GetWorld()->SpawnActor<AFurniture>(SpawnClass, Delta.Transform, SpawnParams);
					if (SpawnedActor)
					{
						SpawnedActor->ApplyFurnitureRow(*Row);
						// 💡 [수정 사항] 저장된 가구의 스케일(기본/수정된 값 모두 포함)을 복원합니다.
						SpawnedActor->SetActorScale3D(Delta.Transform.GetScale3D());

						SpawnedActor->FurnitureID = FurnID;
						SpawnedActor->SetPlacementState(EPlacementState::Placed);
						
						// 저장용 태그 복구
						SpawnedActor->Tags.AddUnique(FName(TEXT("InteriorFurniture")));
						SpawnedActor->Tags.AddUnique(FName(FString::Printf(TEXT("ID_%d"), FurnID)));
						HarnessSave_RestoreOpeningFurnitureTags(SpawnedActor, *Row);
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
								HarnessSave_ApplySurfaceMaterial(MeshComp, MatDelta);
								if (!MatDelta.MeshPath.IsEmpty())
								{
									if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(MeshComp))
									{
										UStaticMesh* LoadedMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *MatDelta.MeshPath));
										if (LoadedMesh)
										{
											StaticMeshComp->SetStaticMesh(LoadedMesh);
											StaticMeshComp->SetRelativeScale3D(MatDelta.RelativeScale);
										}
									}
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
	if (UInteriorPlacementSubsystem* PlacementSubsystem = GetWorld()->GetSubsystem<UInteriorPlacementSubsystem>())
	{
		PlacementSubsystem->ClearAllFurniture();
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), TEXT("InteriorFurniture"), FoundActors);
	for (AActor* Actor : FoundActors)
	{
		Actor->Destroy();
	}
}
