// Fill out your copyright notice in the Description page of Project Settings.

#include "InteriorPlacementManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

AInteriorPlacementManager::AInteriorPlacementManager()
{
	PrimaryActorTick.bCanEverTick = false;
	Grid = nullptr;
	PreviewFurniture = nullptr;
	PreviewFurnitureData = nullptr;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	GridDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("GridDecal"));
	GridDecal->SetupAttachment(RootComponent);

	GridDecal->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	GridDecal->SetFadeScreenSize(0.0f);
	GridDecal->SetVisibility(false);
}

void AInteriorPlacementManager::BeginPlay()
{
	Super::BeginPlay();
	// InitializeFromFloorData가 HarnessTestActor에서 호출되므로 여기서 초기화하지 않음
	// 수동 테스트가 필요하면 에디터에서 직접 InitializeGrid를 호출하거나 bManualInit 플래그 추가
}

void AInteriorPlacementManager::InitializeFromFloorData(const FHarnessFloorData& FloorData, float Cell)
{
	if (FloorData.vertices.IsEmpty())
	{
		return;
	}

	float MinX = TNumericLimits<float>::Max();
	float MaxX = TNumericLimits<float>::Lowest();
	float MinY = TNumericLimits<float>::Max();
	float MaxY = TNumericLimits<float>::Lowest();

	for (const FTopologyVertex& V : FloorData.vertices)
	{
		MinX = FMath::Min(MinX, V.x);
		MaxX = FMath::Max(MaxX, V.x);
		MinY = FMath::Min(MinY, V.y);
		MaxY = FMath::Max(MaxY, V.y);
	}

	float TotalWidth  = MaxX - MinX;
	float TotalHeight = MaxY - MinY;
	float CenterX = (MinX + MaxX) * 0.5f;
	float CenterY = (MinY + MaxY) * 0.5f;
	SetActorLocation(FVector(CenterX, CenterY, 1.0f));

	int Length  = FMath::CeilToInt(TotalWidth  / Cell);
	int Breadth = FMath::CeilToInt(TotalHeight / Cell);
	InitializeGrid(Length, Breadth, Cell);

	// 90도 회전된 데칼은 로컬 Y→월드 Y, 로컬 Z→월드 X로 매핑됨
	// TotalHeight(월드 Y) → DecalSize.Y, TotalWidth(월드 X) → DecalSize.Z
	GridDecal->DecalSize = FVector(500.0f, TotalHeight * 0.5f + Cell, TotalWidth * 0.5f + Cell);

	if (Grid)
	{
		Grid->SetOrigin(FVector2D(CenterX, CenterY));
	}
}

void AInteriorPlacementManager::InitializeGrid(int Length, int Breadth, float Cell)
{
	if (Grid)
	{
		Grid->Destroy();
		Grid = nullptr;
	}

	Grid = GetWorld()->SpawnActor<AGridSpaceManager>(AGridSpaceManager::StaticClass());
	Grid->Initialize(Length, Breadth, Cell);

	// InitializeFromFloorData에서 도면 절대 크기 기준으로 덮어쓰므로 여기선 임시값만 설정
	GridDecal->DecalSize = FVector(500.0f, 100.0f, 100.0f);

	if (GridMaterial)
	{
		UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(GridMaterial, this);
		DynMat->SetScalarParameterValue(TEXT("CellSize"), Cell);
		GridDecal->SetDecalMaterial(DynMat);
	}
}

void AInteriorPlacementManager::SetGridVisible(bool bVisible)
{
	GridDecal->SetVisibility(bVisible);
}

bool AInteriorPlacementManager::HasActivePreview() const
{
	return PreviewFurniture != nullptr;
}

bool AInteriorPlacementManager::IsPreviewLotEmpty()
{
	if (!PreviewFurniture || !PreviewFurnitureData || !Grid)
	{
		return false;
	}

	int L = (int)CurrentDimensions.X;
	int B = (int)CurrentDimensions.Y;

	for (int i = 0; i < L; i++)
	{
		for (int j = 0; j < B; j++)
		{
			FVector2D Cell(PreviewGridAnchor.X + i, PreviewGridAnchor.Y + j);

			if (Cell.X < 0 || Cell.X >= Grid->GetLength() ||
				Cell.Y < 0 || Cell.Y >= Grid->GetBreadth())
			{
				return false;
			}

			if (Grid->GetFurniture(Cell) != nullptr)
			{
				return false;
			}
		}
	}
	return true;
}

bool AInteriorPlacementManager::IsPreviewBoundsEmpty() const
{
	if (!PreviewFurniture)
	{
		return false;
	}

	FBox PreviewBox = PreviewFurniture->GetComponentsBoundingBox().ExpandBy(-1.0f);

	for (AFurniture* Placed : PlacedFurnitures)
	{
		if (!IsValid(Placed))
		{
			continue;
		}

		if (PreviewBox.Intersect(Placed->GetComponentsBoundingBox()))
		{
			return false;
		}
	}
	return true;
}

void AInteriorPlacementManager::ConfirmFurniture()
{
	if (!PreviewFurniture || !IsPreviewBoundsEmpty() || !IsPreviewLotEmpty())
	{
		return;
	}

	int L = (int)CurrentDimensions.X;
	int B = (int)CurrentDimensions.Y;

	for (int i = 0; i < L; i++)
	{
		for (int j = 0; j < B; j++)
		{
			Grid->SetFurniture(FVector2D(PreviewGridAnchor.X + i, PreviewGridAnchor.Y + j), PreviewFurniture);
		}
	}

	PreviewFurniture->PlacedGridAnchor = PreviewGridAnchor;
	PreviewFurniture->PlacedDimensions = CurrentDimensions;
	PreviewFurniture->SetPlacementState(EPlacementState::Placed);
	PlacedFurnitures.Add(PreviewFurniture);
	PreviewFurniture = nullptr;
	PreviewFurnitureData = nullptr;
}

void AInteriorPlacementManager::CreatePreviewFurnitureFromData(FVector RayPosition, FRotator Rotation, UFurnitureData* InFurnitureData)
{
	if (PreviewFurniture)
	{
		PreviewFurniture->Destroy();
		PreviewFurniture = nullptr;
	}

	if (!InFurnitureData || !InFurnitureData->FurnitureBP)
	{
		return;
	}

	PreviewFurnitureData = InFurnitureData;
	CurrentDimensions = InFurnitureData->Dimensions;  // 원본에서 복사, 이후 원본은 건드리지 않음
	PreviewRotation = Rotation;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PreviewFurniture = GetWorld()->SpawnActor<AFurniture>(PreviewFurnitureData->FurnitureBP, RayPosition, Rotation, Params);
	if (!PreviewFurniture)
	{
		return;
	}

	PreviewFurniture->ApplyFurnitureData(PreviewFurnitureData);
	UpdatePreviewLocation(RayPosition);
}

void AInteriorPlacementManager::RotatePreview(float AngleDeg)
{
	if (!PreviewFurniture || !PreviewFurnitureData)
	{
		return;
	}

	PreviewRotation.Yaw = FRotator::NormalizeAxis(PreviewRotation.Yaw + AngleDeg);
	PreviewFurniture->SetActorRotation(PreviewRotation);

	// DataAsset 원본(PreviewFurnitureData->Dimensions)은 건드리지 않고 런타임 복사본만 Swap
	Swap(CurrentDimensions.X, CurrentDimensions.Y);

	UpdatePreviewLocation(LastRayPosition);
}

void AInteriorPlacementManager::UpdatePreviewLocation(FVector RayPosition)
{
	if (!PreviewFurniture || !PreviewFurnitureData || !Grid)
	{
		return;
	}

	LastRayPosition = RayPosition;

	FVector2D GridPos = Grid->ToGridPosition(RayPosition);
	int SnapX = (int)GridPos.X;
	int SnapY = (int)GridPos.Y;

	int L = (int)CurrentDimensions.X;
	int B = (int)CurrentDimensions.Y;
	PreviewGridAnchor = FVector2D(SnapX - L / 2, SnapY - B / 2);

	FVector SnappedWorld = Grid->ToWorldPosition(FVector2D(SnapX, SnapY));
	PreviewFurniture->SetActorLocation(SnappedWorld);

	EPlacementState NewState = (IsPreviewBoundsEmpty() && IsPreviewLotEmpty()) ? EPlacementState::Preview : EPlacementState::Invalid;
	PreviewFurniture->SetPlacementState(NewState);
}

void AInteriorPlacementManager::RemoveFurniture(AFurniture* Target)
{
	if (!Target || !Grid)
	{
		return;
	}

	int L = (int)Target->PlacedDimensions.X;
	int B = (int)Target->PlacedDimensions.Y;

	for (int i = 0; i < L; i++)
	{
		for (int j = 0; j < B; j++)
		{
			FVector2D Cell(Target->PlacedGridAnchor.X + i, Target->PlacedGridAnchor.Y + j);
			if (Grid->GetFurniture(Cell) == Target)
			{
				Grid->SetFurniture(Cell, nullptr);
			}
		}
	}

	PlacedFurnitures.Remove(Target);
	Target->Destroy();
}

void AInteriorPlacementManager::CancelPreview()
{
	if (PreviewFurniture)
	{
		PreviewFurniture->Destroy();
		PreviewFurniture = nullptr;
	}
	PreviewFurnitureData = nullptr;
}

UFurnitureData* AInteriorPlacementManager::FindFurnitureDataByID(int32 TargetID)
{
	for (UFurnitureData* Data : FurnitureDataList)
	{
		if (Data && Data->ID == TargetID)
		{
			return Data;
		}
	}
	return nullptr;
}

FString AInteriorPlacementManager::ExportPlacedFurnituresJson()
{
	TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject());
	TArray<TSharedPtr<FJsonValue>> Array;

	for (AFurniture* Placed : PlacedFurnitures)
	{
		if (!IsValid(Placed) || !Placed->FurnitureData)
		{
			continue;
		}

		TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject());
		Obj->SetNumberField(TEXT("furnitureId"), Placed->FurnitureData->ID);
		Obj->SetNumberField(TEXT("gridX"), Placed->PlacedGridAnchor.X);
		Obj->SetNumberField(TEXT("gridY"), Placed->PlacedGridAnchor.Y);
		Obj->SetNumberField(TEXT("rotationYaw"), Placed->GetActorRotation().Yaw);
		Array.Add(MakeShareable(new FJsonValueObject(Obj)));
	}

	Root->SetArrayField(TEXT("placedFurnitures"), Array);

	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Out;
}

void AInteriorPlacementManager::ImportPlacedFurnituresJson(const FString& JsonString)
{
	for (AFurniture* Placed : PlacedFurnitures)
	{
		if (IsValid(Placed))
		{
			Placed->Destroy();
		}
	}
	PlacedFurnitures.Empty();

	if (Grid)
	{
		InitializeGrid(Grid->GetLength(), Grid->GetBreadth(), GridCellSize);
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

	for (const TSharedPtr<FJsonValue>& Value : *FurnitureArray)
	{
		TSharedPtr<FJsonObject> Obj = Value->AsObject();
		if (!Obj.IsValid())
		{
			continue;
		}

		int32 FurnID = Obj->GetIntegerField(TEXT("furnitureId"));
		int32 GridX  = Obj->GetIntegerField(TEXT("gridX"));
		int32 GridY  = Obj->GetIntegerField(TEXT("gridY"));
		float Yaw    = (float)Obj->GetNumberField(TEXT("rotationYaw"));

		UFurnitureData* Data = FindFurnitureDataByID(FurnID);
		if (!Data || !Data->FurnitureBP || !Grid)
		{
			continue;
		}

		// 회전 여부에 따라 확정 시점의 실제 점유 크기 복원
		FVector2D Dims = Data->Dimensions;
		float NormYaw = FRotator::NormalizeAxis(Yaw);
		if (FMath::Abs(FMath::Abs(NormYaw) - 90.0f) < 1.0f || FMath::Abs(FMath::Abs(NormYaw) - 270.0f) < 1.0f)
		{
			Swap(Dims.X, Dims.Y);
		}

		// GridX/GridY는 좌상단 앵커 인덱스. 스폰 위치는 풋프린트 중심으로 역산
		int CenterIdxX = GridX + (int)Dims.X / 2;
		int CenterIdxY = GridY + (int)Dims.Y / 2;
		FVector SpawnLoc = Grid->ToWorldPosition(FVector2D(CenterIdxX, CenterIdxY));
		SpawnLoc.Z = GetActorLocation().Z;

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AFurniture* NewFurniture = GetWorld()->SpawnActor<AFurniture>(Data->FurnitureBP, SpawnLoc, FRotator(0.0f, Yaw, 0.0f), Params);
		if (!NewFurniture)
		{
			continue;
		}

		NewFurniture->ApplyFurnitureData(Data);
		NewFurniture->PlacedGridAnchor = FVector2D(GridX, GridY);
		NewFurniture->PlacedDimensions = Dims;
		NewFurniture->SetPlacementState(EPlacementState::Placed);

		int L = (int)Dims.X;
		int B = (int)Dims.Y;
		for (int i = 0; i < L; i++)
		{
			for (int j = 0; j < B; j++)
			{
				Grid->SetFurniture(FVector2D(GridX + i, GridY + j), NewFurniture);
			}
		}

		PlacedFurnitures.Add(NewFurniture);
	}
}
