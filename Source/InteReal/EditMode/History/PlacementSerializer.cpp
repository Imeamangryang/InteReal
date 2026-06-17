#include "PlacementSerializer.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "InteReal/EditMode/Furnitures/Furniture.h"
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

		Obj->SetNumberField(TEXT("furnitureId"), Placed->FurnitureID);
		Obj->SetNumberField(TEXT("surfaceType"), static_cast<uint8>(Placed->GetPlacedSurfaceType()));
		Obj->SetNumberField(TEXT("locationX"), Location.X);
		Obj->SetNumberField(TEXT("locationY"), Location.Y);
		Obj->SetNumberField(TEXT("locationZ"), Location.Z);
		Obj->SetNumberField(TEXT("rotationPitch"), Rotation.Pitch);
		Obj->SetNumberField(TEXT("rotationYaw"), Rotation.Yaw);
		Obj->SetNumberField(TEXT("rotationRoll"), Rotation.Roll);
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
		const EPlacementSurfaceType SurfaceType = Obj->HasField(TEXT("surfaceType"))
			                                          ? static_cast<EPlacementSurfaceType>((uint8)Obj->GetIntegerField(TEXT("surfaceType")))
			                                          : EPlacementSurfaceType::Floor;

		const FFurnitureDataRow* Row = Subsystem->FindFurnitureRowByID(FurnID);
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
		AFurniture* NewFurniture = Subsystem->GetWorld()->SpawnActor<AFurniture>(
			Viz->FurnitureClass, SpawnLoc, FRotator(Pitch, Yaw, Roll), Params);
		if (!NewFurniture)
		{
			continue;
		}

		NewFurniture->ApplyFurnitureRow(*Row);
		NewFurniture->PlacedGridAnchor = FVector2D(GridX, GridY);
		NewFurniture->PlacedDimensions = Obj->HasField(TEXT("gridW")) && Obj->HasField(TEXT("gridH"))
			                                 ? FVector2D((float)Obj->GetNumberField(TEXT("gridW")), (float)Obj->GetNumberField(TEXT("gridH")))
			                                 : Dims;
		NewFurniture->SetPlacedSurfaceType(SurfaceType);
		NewFurniture->WallNormalAtPlacement = FVector(
			Obj->HasField(TEXT("wallNormalX")) ? (float)Obj->GetNumberField(TEXT("wallNormalX")) : 0.0f,
			Obj->HasField(TEXT("wallNormalY")) ? (float)Obj->GetNumberField(TEXT("wallNormalY")) : 0.0f,
			Obj->HasField(TEXT("wallNormalZ")) ? (float)Obj->GetNumberField(TEXT("wallNormalZ")) : 0.0f);
		NewFurniture->Tags.Add(TEXT("InteriorFurniture"));
		NewFurniture->Tags.Add(FName(FString::Printf(TEXT("ID_%d"), NewFurniture->FurnitureID)));
		NewFurniture->SetPlacementState(EPlacementState::Placed);

		if (SurfaceType == EPlacementSurfaceType::Floor && Grid)
		{
			const int32 L = (int32)NewFurniture->PlacedDimensions.X;
			const int32 B = (int32)NewFurniture->PlacedDimensions.Y;
			for (int32 i = 0; i < L; i++)
			{
				for (int32 j = 0; j < B; j++)
				{
					Grid->SetFurniture(FVector2D(GridX + i, GridY + j), NewFurniture);
				}
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
