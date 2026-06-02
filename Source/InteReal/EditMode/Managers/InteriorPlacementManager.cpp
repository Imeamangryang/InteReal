// Fill out your copyright notice in the Description page of Project Settings.

#include "InteriorPlacementManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/OverlapResult.h"   
#include "CollisionQueryParams.h"    
#include "Engine/World.h"               
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

AInteriorPlacementManager::AInteriorPlacementManager()
{
	PrimaryActorTick.bCanEverTick = true;
	Grid = nullptr;
	PreviewFurniture = nullptr;
	FurnitureDataTable = nullptr;

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
}

void AInteriorPlacementManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!PreviewFurniture || !bHasTargetLocation) return;

	FVector Current = PreviewFurniture->GetActorLocation();
	FVector Interpolated = FMath::VInterpTo(Current, TargetPreviewLocation, DeltaTime, PreviewInterpSpeed);
	PreviewFurniture->SetActorLocation(Interpolated);
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
	GridCellSize = Cell;

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
	if (!PreviewFurniture || !Grid)
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

	// GetComponentsBoundingBox는 기즈모 링까지 포함해서 너무 커짐
	// 콜리전 박스만 사용해서 정확한 범위로 판정
	FBox PreviewBox = PreviewFurniture->GetCollisionBounds().ExpandBy(-1.0f);

	for (AFurniture* Placed : PlacedFurnitures)
	{
		if (!IsValid(Placed))
		{
			continue;
		}

		if (PreviewBox.Intersect(Placed->GetCollisionBounds()))
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

	// 💡 [저장 시스템 연동] SaveManager가 인식할 수 있도록 태그 추가
	PreviewFurniture->Tags.Add(TEXT("InteriorFurniture"));
	PreviewFurniture->Tags.Add(FName(FString::Printf(TEXT("ID_%d"), PreviewFurniture->FurnitureID)));

	PreviewFurniture->PlacedGridAnchor = PreviewGridAnchor;
	PreviewFurniture->PlacedDimensions = CurrentDimensions;
	PreviewFurniture->SetPlacementState(EPlacementState::Placed);
	PlacedFurnitures.Add(PreviewFurniture);
	PreviewFurniture = nullptr;
}

void AInteriorPlacementManager::CreatePreviewFurnitureFromRow(FVector RayPosition, FRotator Rotation, const FFurnitureDataRow& InFurnitureRow)
{
	if (PreviewFurniture)
	{
		PreviewFurniture->Destroy();
		PreviewFurniture = nullptr;
	}

	if (!FurnitureClass)
	{
		return;
	}

	CurrentDimensions = FVector2D(InFurnitureRow.Dimensions.X, InFurnitureRow.Dimensions.Y);
	PreviewRotation = Rotation;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PreviewFurniture = GetWorld()->SpawnActor<AFurniture>(FurnitureClass, RayPosition, Rotation, Params);
	if (!PreviewFurniture)
	{
		return;
	}

	PreviewFurniture->ApplyFurnitureRow(InFurnitureRow);
	UpdatePreviewLocation(RayPosition);
}

void AInteriorPlacementManager::RotatePreview(float AngleDeg)
{
	if (!PreviewFurniture)
	{
		return;
	}

	PreviewRotation.Yaw = FRotator::NormalizeAxis(PreviewRotation.Yaw + AngleDeg);
	PreviewFurniture->SetActorRotation(PreviewRotation);

	Swap(CurrentDimensions.X, CurrentDimensions.Y);

	UpdatePreviewLocation(LastRayPosition);
}

void AInteriorPlacementManager::UpdatePreviewLocation(FVector RayPosition)
{
	if (!PreviewFurniture || !Grid)
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
	SnappedWorld.Z = GetActorLocation().Z;

	// 직접 이동 대신 타겟 위치 저장 — Tick에서 VInterpTo로 부드럽게 이동
	TargetPreviewLocation = SnappedWorld;
	bHasTargetLocation    = true;

	PreviewFurniture->SetActorRotation(PreviewRotation);

	// Invalid 이유 판정
	bool bBoundsOk = IsPreviewBoundsEmpty();
	bool bLotOk    = IsPreviewLotEmpty();

	if (bBoundsOk && bLotOk)
	{
		InvalidReason = EPlacementInvalidReason::None;
		PreviewFurniture->SetPlacementState(EPlacementState::Preview);
	}
	else
	{
		// 그리드 범위 벗어남 여부 — IsPreviewLotEmpty에서 이미 범위 체크 포함
		bool bOutOfBounds = (PreviewGridAnchor.X < 0 || PreviewGridAnchor.Y < 0 ||
		                     PreviewGridAnchor.X + L > Grid->GetLength() ||
		                     PreviewGridAnchor.Y + B > Grid->GetBreadth());

		InvalidReason = bOutOfBounds
			? EPlacementInvalidReason::OutOfBounds
			: EPlacementInvalidReason::Overlapping;

		PreviewFurniture->SetPlacementState(EPlacementState::Invalid);
	}
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
	bHasTargetLocation = false;
	InvalidReason = EPlacementInvalidReason::None;
}

const FFurnitureDataRow* AInteriorPlacementManager::FindFurnitureRowByID(int32 TargetID) const
{
	if (!FurnitureDataTable)
	{
		return nullptr;
	}

	static const FString ContextString(TEXT("FindFurnitureRowByID"));
	TArray<FFurnitureDataRow*> AllRows;
	FurnitureDataTable->GetAllRows<FFurnitureDataRow>(ContextString, AllRows);

	for (const FFurnitureDataRow* Row : AllRows)
	{
		if (Row && Row->ID == TargetID)
		{
			return Row;
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
		if (!IsValid(Placed))
		{
			continue;
		}

		TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject());
		Obj->SetNumberField(TEXT("furnitureId"), Placed->FurnitureID);
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

		const FFurnitureDataRow* Row = FindFurnitureRowByID(FurnID);
		if (!Row || !FurnitureClass || !Grid)
		{
			continue;
		}

		FVector2D Dims = FVector2D(Row->Dimensions.X, Row->Dimensions.Y);
		float NormYaw = FRotator::NormalizeAxis(Yaw);
		if (FMath::Abs(FMath::Abs(NormYaw) - 90.0f) < 1.0f || FMath::Abs(FMath::Abs(NormYaw) - 270.0f) < 1.0f)
		{
			Swap(Dims.X, Dims.Y);
		}

		int CenterIdxX = GridX + (int)Dims.X / 2;
		int CenterIdxY = GridY + (int)Dims.Y / 2;
		FVector SpawnLoc = Grid->ToWorldPosition(FVector2D(CenterIdxX, CenterIdxY));
		SpawnLoc.Z = GetActorLocation().Z;

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AFurniture* NewFurniture = GetWorld()->SpawnActor<AFurniture>(FurnitureClass, SpawnLoc, FRotator(0.0f, Yaw, 0.0f), Params);
		if (!NewFurniture)
		{
			continue;
		}

		NewFurniture->ApplyFurnitureRow(*Row);
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