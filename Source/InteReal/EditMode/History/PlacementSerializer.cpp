#include "PlacementSerializer.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "InteReal/EditMode/Furniture/Furniture.h"
#include "InteReal/EditMode/Furniture/LightFixture.h"
#include "InteReal/EditMode/Managers/GridSpaceManager.h"
#include "InteReal/EditMode/Visualization/PlacementVisualizerActor.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"

namespace
{
bool PlacementSerializer_IsDoorLikeKind(EPlacementAssetKind Kind)
{
	return Kind == EPlacementAssetKind::Door ||
	       Kind == EPlacementAssetKind::EntranceDoor ||
	       Kind == EPlacementAssetKind::SlidingDoor;
}

bool PlacementSerializer_IsWindowLikeName(const FString& Label)
{
	return Label.Contains(TEXT("window")) ||
	       Label.Contains(TEXT("\ucc3d\ubb38")) ||
	       Label.Contains(TEXT("\ucc3d\ud638")) ||
	       Label.Contains(TEXT("\ucc3d"));
}

bool PlacementSerializer_IsDoorLikeName(const FString& Label)
{
	return Label.Contains(TEXT("door")) ||
	       Label.Contains(TEXT("\ubb38")) ||
	       Label.Contains(TEXT("\ud604\uad00")) ||
	       Label.Contains(TEXT("\ubbf8\ub2eb")) ||
	       Label.Contains(TEXT("\uc2ac\ub77c\uc774\ub529"));
}

bool PlacementSerializer_IsOpeningFurnitureRow(const FFurnitureDataRow& Row)
{
	if (Row.AssetKind != EPlacementAssetKind::Generic)
	{
		return true;
	}

	const FString Label = Row.DisplayName.ToString().ToLower();
	return PlacementSerializer_IsDoorLikeName(Label) || PlacementSerializer_IsWindowLikeName(Label);
}

bool PlacementSerializer_IsWindowFurnitureRow(const FFurnitureDataRow& Row)
{
	if (Row.AssetKind == EPlacementAssetKind::Window)
	{
		return true;
	}
	if (PlacementSerializer_IsDoorLikeKind(Row.AssetKind))
	{
		return false;
	}
	return PlacementSerializer_IsWindowLikeName(Row.DisplayName.ToString().ToLower());
}

void PlacementSerializer_RestoreOpeningFurnitureTags(AFurniture* Furniture, const FFurnitureDataRow& Row)
{
	if (!Furniture || !PlacementSerializer_IsOpeningFurnitureRow(Row))
	{
		return;
	}

	Furniture->Tags.AddUnique(FName(TEXT("OpeningAsset")));
	Furniture->Tags.AddUnique(FName(PlacementSerializer_IsWindowFurnitureRow(Row) ? TEXT("WindowAsset") : TEXT("DoorAsset")));
}

uint8 PlacementSerializer_AllLightPlacementTypes()
{
	return static_cast<uint8>(EPlacementSurfaceType::Floor)
	     | static_cast<uint8>(EPlacementSurfaceType::Wall)
	     | static_cast<uint8>(EPlacementSurfaceType::Ceiling);
}

EFurnitureAssetCategory PlacementSerializer_GetAssetCategory(const AFurniture* Furniture)
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

FLightAttributes PlacementSerializer_GetLightAttributes(const AFurniture* Furniture)
{
	if (const ALightFixture* LightFixture = Cast<ALightFixture>(Furniture))
	{
		return LightFixture->GetLightAttributes();
	}
	return Furniture && Furniture->HasFurnitureDataRow()
		? Furniture->GetFurnitureDataRow().LightAttributes
		: FLightAttributes();
}

void PlacementSerializer_WriteLightAttributes(const TSharedPtr<FJsonObject>& Obj, const FLightAttributes& Attributes)
{
	if (!Obj.IsValid())
	{
		return;
	}

	Obj->SetBoolField(TEXT("lightEmits"), Attributes.bEmitsLight);
	Obj->SetNumberField(TEXT("lightFixtureType"), static_cast<uint8>(Attributes.LightFixtureType));
	Obj->SetNumberField(TEXT("lightColorR"), Attributes.LightColor.R);
	Obj->SetNumberField(TEXT("lightColorG"), Attributes.LightColor.G);
	Obj->SetNumberField(TEXT("lightColorB"), Attributes.LightColor.B);
	Obj->SetNumberField(TEXT("lightColorA"), Attributes.LightColor.A);
	Obj->SetNumberField(TEXT("lightIntensity"), Attributes.LightIntensity);
	Obj->SetNumberField(TEXT("lightAttenuationRadius"), Attributes.AttenuationRadius);
}

FLightAttributes PlacementSerializer_ReadLightAttributes(const TSharedPtr<FJsonObject>& Obj)
{
	FLightAttributes Attributes;
	if (!Obj.IsValid())
	{
		return Attributes;
	}

	bool bEmitsLight = Attributes.bEmitsLight;
	if (Obj->TryGetBoolField(TEXT("lightEmits"), bEmitsLight))
	{
		Attributes.bEmitsLight = bEmitsLight;
	}
	if (Obj->HasField(TEXT("lightFixtureType")))
	{
		Attributes.LightFixtureType = static_cast<ELightFixtureType>((uint8)Obj->GetIntegerField(TEXT("lightFixtureType")));
	}
	if (Obj->HasField(TEXT("lightColorR")))
	{
		Attributes.LightColor.R = (float)Obj->GetNumberField(TEXT("lightColorR"));
	}
	if (Obj->HasField(TEXT("lightColorG")))
	{
		Attributes.LightColor.G = (float)Obj->GetNumberField(TEXT("lightColorG"));
	}
	if (Obj->HasField(TEXT("lightColorB")))
	{
		Attributes.LightColor.B = (float)Obj->GetNumberField(TEXT("lightColorB"));
	}
	if (Obj->HasField(TEXT("lightColorA")))
	{
		Attributes.LightColor.A = (float)Obj->GetNumberField(TEXT("lightColorA"));
	}
	if (Obj->HasField(TEXT("lightIntensity")))
	{
		Attributes.LightIntensity = (float)Obj->GetNumberField(TEXT("lightIntensity"));
	}
	if (Obj->HasField(TEXT("lightAttenuationRadius")))
	{
		Attributes.AttenuationRadius = (float)Obj->GetNumberField(TEXT("lightAttenuationRadius"));
	}

	return Attributes;
}

FFurnitureDataRow PlacementSerializer_MakeLightRowFromJson(int32 FurnitureID, const TSharedPtr<FJsonObject>& Obj, const FFurnitureDataRow* ExistingRow)
{
	FFurnitureDataRow Row = ExistingRow ? *ExistingRow : FFurnitureDataRow();
	Row.ID = FurnitureID;
	Row.Category = EFurnitureAssetCategory::Lighting;
	Row.LightAttributes = PlacementSerializer_ReadLightAttributes(Obj);
	Row.AllowedPlacementTypes = PlacementSerializer_AllLightPlacementTypes();
	return Row;
}
}

void UPlacementSerializer::Initialize(UInteriorPlacementSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
}

FString UPlacementSerializer::ExportPlacedFurnituresJson() const
{
	if (!Subsystem)
	{
		return FString();
	}

	TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject());
	TArray<TSharedPtr<FJsonValue>> Array;

	TMap<const AFurniture*, int32> FurnitureToIndex;
	for (const AFurniture* Placed : Subsystem->GetPlacedFurnitures())
	{
		if (IsValid(Placed))
		{
			FurnitureToIndex.Add(Placed, FurnitureToIndex.Num());
		}
	}

	for (const AFurniture* Placed : Subsystem->GetPlacedFurnitures())
	{
		if (!IsValid(Placed))
		{
			continue;
		}

		TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject());
		const FVector Location = Placed->GetActorLocation();
		const FRotator Rotation = Placed->GetActorRotation();
		const FVector Scale = Placed->GetActorScale3D();
		const EFurnitureAssetCategory AssetCategory = PlacementSerializer_GetAssetCategory(Placed);

		Obj->SetNumberField(TEXT("furnitureId"), Placed->FurnitureID);
		Obj->SetNumberField(TEXT("assetCategory"), static_cast<uint8>(AssetCategory));
		if (AssetCategory == EFurnitureAssetCategory::Lighting)
		{
			PlacementSerializer_WriteLightAttributes(Obj, PlacementSerializer_GetLightAttributes(Placed));
		}
		Obj->SetNumberField(TEXT("surfaceType"), static_cast<uint8>(Placed->GetPlacedSurfaceType()));
		Obj->SetNumberField(TEXT("locationX"), Location.X);
		Obj->SetNumberField(TEXT("locationY"), Location.Y);
		Obj->SetNumberField(TEXT("locationZ"), Location.Z);
		Obj->SetNumberField(TEXT("rotationPitch"), Rotation.Pitch);
		Obj->SetNumberField(TEXT("rotationYaw"), Rotation.Yaw);
		Obj->SetNumberField(TEXT("rotationRoll"), Rotation.Roll);
		Obj->SetNumberField(TEXT("scaleX"), Scale.X);
		Obj->SetNumberField(TEXT("scaleY"), Scale.Y);
		Obj->SetNumberField(TEXT("scaleZ"), Scale.Z);
		Obj->SetNumberField(TEXT("gridX"), Placed->PlacedGridAnchor.X);
		Obj->SetNumberField(TEXT("gridY"), Placed->PlacedGridAnchor.Y);
		Obj->SetNumberField(TEXT("gridW"), Placed->PlacedDimensions.X);
		Obj->SetNumberField(TEXT("gridH"), Placed->PlacedDimensions.Y);
		Obj->SetNumberField(TEXT("wallNormalX"), Placed->WallNormalAtPlacement.X);
		Obj->SetNumberField(TEXT("wallNormalY"), Placed->WallNormalAtPlacement.Y);
		Obj->SetNumberField(TEXT("wallNormalZ"), Placed->WallNormalAtPlacement.Z);
		Obj->SetNumberField(TEXT("parentIndex"), FurnitureToIndex.Contains(Placed->ParentFurniture) ? FurnitureToIndex[Placed->ParentFurniture] : -1);

		Array.Add(MakeShareable(new FJsonValueObject(Obj)));
	}

	Root->SetArrayField(TEXT("placedFurnitures"), Array);
	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Out;
}

void UPlacementSerializer::ImportPlacedFurnituresJson(const FString& JsonString)
{
	if (!Subsystem)
	{
		return;
	}

	TArray<AFurniture*> ToRemove = Subsystem->GetPlacedFurnituresMutable();
	for (AFurniture* F : ToRemove)
	{
		if (IsValid(F))
		{
			F->Destroy();
		}
	}
	Subsystem->GetPlacedFurnituresMutable().Empty();

	AGridSpaceManager* Grid = Subsystem->GetGrid();
	if (Grid)
	{
		Grid->ClearFurnitureOccupancy();
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* FurnitureArray;
	if (!Root->TryGetArrayField(TEXT("placedFurnitures"), FurnitureArray))
	{
		return;
	}

	const APlacementVisualizerActor* Viz = Subsystem->GetVisualizer();
	if (!Viz || !Viz->FurnitureClass)
	{
		return;
	}

	TArray<AFurniture*> ImportedFurnitures;
	TArray<int32> PendingParentIndices;

	for (const TSharedPtr<FJsonValue>& Value : *FurnitureArray)
	{
		TSharedPtr<FJsonObject> Obj = Value->AsObject();
		if (!Obj.IsValid())
		{
			continue;
		}

		const int32 FurnID = Obj->GetIntegerField(TEXT("furnitureId"));
		const int32 GridX = Obj->HasField(TEXT("gridX")) ? Obj->GetIntegerField(TEXT("gridX")) : 0;
		const int32 GridY = Obj->HasField(TEXT("gridY")) ? Obj->GetIntegerField(TEXT("gridY")) : 0;
		const float Pitch = Obj->HasField(TEXT("rotationPitch")) ? (float)Obj->GetNumberField(TEXT("rotationPitch")) : 0.0f;
		const float Yaw = Obj->HasField(TEXT("rotationYaw")) ? (float)Obj->GetNumberField(TEXT("rotationYaw")) : 0.0f;
		const float Roll = Obj->HasField(TEXT("rotationRoll")) ? (float)Obj->GetNumberField(TEXT("rotationRoll")) : 0.0f;
		const FVector Scale(
			Obj->HasField(TEXT("scaleX")) ? (float)Obj->GetNumberField(TEXT("scaleX")) : 1.0f,
			Obj->HasField(TEXT("scaleY")) ? (float)Obj->GetNumberField(TEXT("scaleY")) : 1.0f,
			Obj->HasField(TEXT("scaleZ")) ? (float)Obj->GetNumberField(TEXT("scaleZ")) : 1.0f);
		const EPlacementSurfaceType SurfaceType = Obj->HasField(TEXT("surfaceType"))
			                                          ? static_cast<EPlacementSurfaceType>((uint8)Obj->GetIntegerField(TEXT("surfaceType")))
			                                          : EPlacementSurfaceType::Floor;

		FFurnitureDataRow ResolvedLightRow;
		const FFurnitureDataRow* Row = Subsystem->FindFurnitureRowByID(FurnID);
		const EFurnitureAssetCategory AssetCategory = Obj->HasField(TEXT("assetCategory"))
			                                               ? static_cast<EFurnitureAssetCategory>((uint8)Obj->GetIntegerField(TEXT("assetCategory")))
			                                               : (Row ? Row->Category : EFurnitureAssetCategory::None);
		if (AssetCategory == EFurnitureAssetCategory::Lighting)
		{
			const FFurnitureDataRow* LightSourceRow = Row && Row->Category == EFurnitureAssetCategory::Lighting
				? Row
				: nullptr;
			ResolvedLightRow = PlacementSerializer_MakeLightRowFromJson(FurnID, Obj, LightSourceRow);
			Row = &ResolvedLightRow;
		}
		if (!Row)
		{
			continue;
		}

		FVector2D Dims(Row->Dimensions.X, Row->Dimensions.Y);
		const float NormYaw = FRotator::NormalizeAxis(Yaw);
		if (FMath::IsNearlyEqual(FMath::Abs(NormYaw), 90.0f, 1.0f))
		{
			Swap(Dims.X, Dims.Y);
		}

		FVector SpawnLoc = FVector::ZeroVector;
		if (Obj->HasField(TEXT("locationX")) && Obj->HasField(TEXT("locationY")) && Obj->HasField(TEXT("locationZ")))
		{
			SpawnLoc.X = (float)Obj->GetNumberField(TEXT("locationX"));
			SpawnLoc.Y = (float)Obj->GetNumberField(TEXT("locationY"));
			SpawnLoc.Z = (float)Obj->GetNumberField(TEXT("locationZ"));
		}
		else if (Grid)
		{
			const float CenterGridX = (float)GridX + Dims.X * 0.5f;
			const float CenterGridY = (float)GridY + Dims.Y * 0.5f;
			SpawnLoc = Grid->ToWorldPosition(FVector2D(CenterGridX - 0.5f, CenterGridY - 0.5f));
			SpawnLoc.Z = Subsystem->GetFloorZ();
		}
		else
		{
			continue;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const TSubclassOf<AFurniture> SpawnClass = AFurniture::ResolveSpawnClass(*Row, Viz->FurnitureClass);
		AFurniture* NewFurniture = Subsystem->GetWorld()->SpawnActor<AFurniture>(
			SpawnClass, SpawnLoc, FRotator(Pitch, Yaw, Roll), Params);
		if (!NewFurniture)
		{
			continue;
		}

		NewFurniture->ApplyFurnitureRow(*Row);
		NewFurniture->SetActorScale3D(Scale);
		NewFurniture->PlacedGridAnchor = FVector2D(GridX, GridY);
		NewFurniture->PlacedDimensions = Obj->HasField(TEXT("gridW")) && Obj->HasField(TEXT("gridH"))
			                                 ? FVector2D((float)Obj->GetNumberField(TEXT("gridW")), (float)Obj->GetNumberField(TEXT("gridH")))
			                                 : Dims;
		NewFurniture->SetPlacedSurfaceType(SurfaceType);
		NewFurniture->WallNormalAtPlacement = FVector(
			Obj->HasField(TEXT("wallNormalX")) ? (float)Obj->GetNumberField(TEXT("wallNormalX")) : 0.0f,
			Obj->HasField(TEXT("wallNormalY")) ? (float)Obj->GetNumberField(TEXT("wallNormalY")) : 0.0f,
			Obj->HasField(TEXT("wallNormalZ")) ? (float)Obj->GetNumberField(TEXT("wallNormalZ")) : 0.0f);
		NewFurniture->Tags.AddUnique(FName(TEXT("InteriorFurniture")));
		NewFurniture->Tags.AddUnique(FName(FString::Printf(TEXT("ID_%d"), NewFurniture->FurnitureID)));
		PlacementSerializer_RestoreOpeningFurnitureTags(NewFurniture, *Row);
		NewFurniture->SetPlacementState(EPlacementState::Placed);

		if (SurfaceType == EPlacementSurfaceType::Floor && Grid)
		{
			NewFurniture->GetOccupiedGridCells(Grid, NewFurniture->PlacedGridAnchor,
				NewFurniture->PlacedDimensions, NewFurniture->PlacedOccupiedCells);
			for (const FIntPoint& Cell : NewFurniture->PlacedOccupiedCells)
			{
				Grid->SetFurniture(FVector2D(Cell.X, Cell.Y), NewFurniture);
			}
		}

		Subsystem->GetPlacedFurnituresMutable().Add(NewFurniture);
		ImportedFurnitures.Add(NewFurniture);
		PendingParentIndices.Add(Obj->HasField(TEXT("parentIndex")) ? Obj->GetIntegerField(TEXT("parentIndex")) : -1);
	}

	for (int32 Index = 0; Index < ImportedFurnitures.Num(); Index++)
	{
		AFurniture* Child = ImportedFurnitures[Index];
		const int32 ParentIndex = PendingParentIndices.IsValidIndex(Index) ? PendingParentIndices[Index] : -1;
		if (IsValid(Child) && Child->GetPlacedSurfaceType() == EPlacementSurfaceType::Surface && ImportedFurnitures.IsValidIndex(ParentIndex))
		{
			AFurniture* Parent = ImportedFurnitures[ParentIndex];
			if (IsValid(Parent))
			{
				Child->ParentFurniture = Parent;
				Child->AttachToActor(Parent, FAttachmentTransformRules::KeepWorldTransform);
			}
		}
	}

	// 저장된 경고 상태를 그대로 믿지 않고, 불러온 시점의 실제 도면/그리드 기준으로 다시 판정한다.
	Subsystem->RevalidatePlacedFurnitureWarnings();
}

FString UPlacementSerializer::ExportEditStateJson() const
{
	TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject());
	Root->SetStringField(TEXT("furnitureJson"), ExportPlacedFurnituresJson());

	TArray<TSharedPtr<FJsonValue>> SurfaceArray;
	ExportSurfaceMaterials(SurfaceArray);
	Root->SetArrayField(TEXT("surfaces"), SurfaceArray);

	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Out;
}

void UPlacementSerializer::ImportEditStateJson(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return;
	}

	FString FurnitureJson;
	if (Root->TryGetStringField(TEXT("furnitureJson"), FurnitureJson))
	{
		ImportPlacedFurnituresJson(FurnitureJson);
	}

	const TArray<TSharedPtr<FJsonValue>>* SurfaceArray = nullptr;
	if (Root->TryGetArrayField(TEXT("surfaces"), SurfaceArray))
	{
		ImportSurfaceMaterials(*SurfaceArray);
	}
}

bool UPlacementSerializer::IsEditableSurfaceComponent(const UMeshComponent* MeshComp) const
{
	if (!MeshComp)
	{
		return false;
	}
	return MeshComp->ComponentHasTag(TEXT("EditableWall")) ||
	       MeshComp->ComponentHasTag(TEXT("EditableFloor")) ||
	       MeshComp->ComponentHasTag(TEXT("Floor"));
}

void UPlacementSerializer::ExportSurfaceMaterials(TArray<TSharedPtr<FJsonValue>>& OutArray) const
{
	if (!Subsystem || !Subsystem->GetWorld())
	{
		return;
	}

	for (TActorIterator<AActor> It(Subsystem->GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor))
		{
			continue;
		}

		TArray<UMeshComponent*> MeshComponents;
		Actor->GetComponents<UMeshComponent>(MeshComponents);

		for (UMeshComponent* MeshComp : MeshComponents)
		{
			if (!IsValid(MeshComp) || !IsEditableSurfaceComponent(MeshComp))
			{
				continue;
			}

			const int32 MatCount = MeshComp->GetNumMaterials();
			for (int32 SlotIdx = 0; SlotIdx < MatCount; SlotIdx++)
			{
				UMaterialInterface* Mat = MeshComp->GetMaterial(SlotIdx);
				TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject());
				Obj->SetStringField(TEXT("componentPath"), MeshComp->GetPathName());
				Obj->SetNumberField(TEXT("materialSlot"), SlotIdx);
				Obj->SetStringField(TEXT("materialPath"), Mat ? Mat->GetPathName() : FString());
				OutArray.Add(MakeShareable(new FJsonValueObject(Obj)));
			}
		}
	}
}

void UPlacementSerializer::ImportSurfaceMaterials(const TArray<TSharedPtr<FJsonValue>>& SurfaceArray)
{
	if (!Subsystem || !Subsystem->GetWorld())
	{
		return;
	}

	TMap<FString, UMeshComponent*> ComponentMap;
	for (TActorIterator<AActor> It(Subsystem->GetWorld()); It; ++It)
	{
		TArray<UMeshComponent*> MeshComponents;
		It->GetComponents<UMeshComponent>(MeshComponents);
		for (UMeshComponent* MeshComp : MeshComponents)
		{
			if (IsValid(MeshComp) && IsEditableSurfaceComponent(MeshComp))
			{
				ComponentMap.Add(MeshComp->GetPathName(), MeshComp);
			}
		}
	}

	for (const TSharedPtr<FJsonValue>& Value : SurfaceArray)
	{
		TSharedPtr<FJsonObject> Obj = Value->AsObject();
		if (!Obj.IsValid())
		{
			continue;
		}

		FString ComponentPath, MaterialPath;
		int32 MaterialSlot = 0;

		if (!Obj->TryGetStringField(TEXT("componentPath"), ComponentPath))
		{
			continue;
		}
		Obj->TryGetNumberField(TEXT("materialSlot"), MaterialSlot);
		Obj->TryGetStringField(TEXT("materialPath"), MaterialPath);

		UMeshComponent** Found = ComponentMap.Find(ComponentPath);
		if (!Found || !IsValid(*Found))
		{
			continue;
		}

		UMaterialInterface* LoadedMat = MaterialPath.IsEmpty() ? nullptr :
			Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MaterialPath));

		(*Found)->SetMaterial(MaterialSlot, LoadedMat);
	}
}
