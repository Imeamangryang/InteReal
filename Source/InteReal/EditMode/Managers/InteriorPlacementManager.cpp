// Fill out your copyright notice in the Description page of Project Settings.

#include "InteriorPlacementManager.h"
#include "InteReal/Harness/Public/HarnessPipelineManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Components/SceneCaptureComponent2D.h"
#include "EngineUtils.h"

AInteriorPlacementManager::AInteriorPlacementManager()
{
	PrimaryActorTick.bCanEverTick = false;
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

	GridMeshComp = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("GridMeshComp"));
	GridMeshComp->SetupAttachment(RootComponent);
	GridMeshComp->SetVisibility(false);
	GridMeshComp->SetCastShadow(false);
	GridMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GridMeshComp->bVisibleInSceneCaptureOnly = false;  

	PlacementVizValid = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("PlacementVizValid"));
	PlacementVizValid->SetupAttachment(RootComponent);
	PlacementVizValid->SetCastShadow(false);
	PlacementVizValid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlacementVizValid->SetReceivesDecals(false);
	PlacementVizValid->SetVisibility(false);

	PlacementVizInvalid = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("PlacementVizInvalid"));
	PlacementVizInvalid->SetupAttachment(RootComponent);
	PlacementVizInvalid->SetCastShadow(false);
	PlacementVizInvalid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlacementVizInvalid->SetReceivesDecals(false);
	PlacementVizInvalid->SetVisibility(false);
}

void AInteriorPlacementManager::BeginPlay()
{
	Super::BeginPlay();

	if (ValidCellMaterial)
	{
		PlacementVizValid->SetMaterial(0, ValidCellMaterial);
	}
		
	if (InvalidCellMaterial)
	{
		PlacementVizInvalid->SetMaterial(0, InvalidCellMaterial);
	}
	
	// 미니맵 SceneCapture에서 그리드/배치 시각화가 보이지 않도록 이 액터를 숨김
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TArray<USceneCaptureComponent2D*> Captures;
		It->GetComponents<USceneCaptureComponent2D>(Captures);
		for (USceneCaptureComponent2D* Cap : Captures)
		{
			Cap->HiddenActors.AddUnique(this);
		}
	}
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
		// BuildTopologyCaches와 동일한 축 매핑: 도면 Y → 월드 X, 도면 X → 월드 Y
		MinX = FMath::Min(MinX, V.y);
		MaxX = FMath::Max(MaxX, V.y);
		MinY = FMath::Min(MinY, V.x);
		MaxY = FMath::Max(MaxY, V.x);
	}

	float TotalWidth = MaxX - MinX;
	float TotalHeight = MaxY - MinY;
	float CenterX = (MinX + MaxX) * 0.5f;
	float CenterY = (MinY + MaxY) * 0.5f;
	SetActorLocation(FVector(CenterX, CenterY, 1.0f));

	int Length = FMath::CeilToInt(TotalWidth / Cell);
	int Breadth = FMath::CeilToInt(TotalHeight / Cell);
	InitializeGrid(Length, Breadth, Cell);

	// 90도 회전된 데칼은 로컬 Y→월드 Y, 로컬 Z→월드 X로 매핑됨
	// TotalHeight(월드 Y) → DecalSize.Y, TotalWidth(월드 X) → DecalSize.Z
	GridDecal->DecalSize = FVector(500.0f, TotalHeight * 0.5f + Cell, TotalWidth * 0.5f + Cell);

	if (Grid)
	{
		Grid->SetOrigin(FVector2D(CenterX, CenterY));
	}

	BuildFloorPolygon(FloorData);
	BuildWallSegments(FloorData);
	MarkOutOfBoundsTiles();
	RebuildGridMesh();
}

void AInteriorPlacementManager::RebuildGridMesh()
{
	if (!GridMeshComp || FloorPolygon.Num() < 3) return;

	FVector ActorLoc = GetActorLocation();

	// 무게중심 (이미 BuildFloorPolygon에서 정렬 기준으로 사용한 값)
	FVector2D Centroid(0, 0);
	for (const FVector2D& P : FloorPolygon) Centroid += P;
	Centroid /= (float)FloorPolygon.Num();

	UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GridMeshComp);
	UE::Geometry::FDynamicMesh3& Mesh = *DynMesh->GetMeshPtr();

	// 무게중심 정점 (액터 로컬 좌표)
	int32 CenterIdx = Mesh.AppendVertex(
		FVector3d(Centroid.X - ActorLoc.X, Centroid.Y - ActorLoc.Y, 0.0));

	// 폴리곤 정점
	TArray<int32> VertIds;
	for (const FVector2D& P : FloorPolygon)
	{
		VertIds.Add(Mesh.AppendVertex(
			FVector3d(P.X - ActorLoc.X, P.Y - ActorLoc.Y, 0.0)));
	}

	double SignedArea = 0.0;
	int32 N = FloorPolygon.Num();
	for (int32 i = 0; i < N; i++)
	{
		FVector2D Pi = FloorPolygon[i];
		FVector2D Pj = FloorPolygon[(i + 1) % N];
		SignedArea += Pi.X * Pj.Y - Pj.X * Pi.Y;
	}
	bool bCCW = SignedArea > 0.0;

	// 팬 삼각분할
	for (int32 i = 0; i < N; i++)
	{
		int32 Va = VertIds[i];
		int32 Vb = VertIds[(i + 1) % N];

		Mesh.AppendTriangle(bCCW ? CenterIdx : Va, bCCW ? Vb : CenterIdx, bCCW ? Va : Vb);
	}

	GridMeshComp->SetDynamicMesh(DynMesh);

	if (GridMaterial)
	{
		UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(GridMaterial, this);
		DynMat->SetScalarParameterValue(TEXT("CellSize"), GridCellSize);
		GridMeshComp->SetMaterial(0, DynMat);
	}
}

void AInteriorPlacementManager::BuildFloorPolygon(const FHarnessFloorData& FloorData)
{
	FloorPolygon.Empty();

	// WallOuter 엣지에 속한 정점 ID 수집
	TSet<FString> OuterIds;
	for (const FTopologyHalfEdge& Edge : FloorData.half_edges)
	{
		if (Edge.type == TEXT("WallOuter"))
		{
			OuterIds.Add(Edge.vertex_start);
			OuterIds.Add(Edge.vertex_end);
		}
	}

	// WallOuter가 없으면 전체 정점 사용
	if (OuterIds.IsEmpty())
	{
		for (const FTopologyVertex& V : FloorData.vertices)
		{
			OuterIds.Add(V.id);
		}
	}

	// 정점 좌표 맵 구성 (BuildTopologyCaches와 동일한 축 매핑)
	TMap<FString, FVector2D> VMap;
	for (const FTopologyVertex& V : FloorData.vertices)
	{
		VMap.Add(V.id, FVector2D(V.y, V.x));
	}

	TArray<FVector2D> Points;
	for (const FString& Id : OuterIds)
	{
		if (const FVector2D* Pos = VMap.Find(Id))
		{
			Points.Add(*Pos);
		}
	}

	if (Points.IsEmpty()) return;

	// 무게중심 계산
	FVector2D Centroid(0, 0);
	for (const FVector2D& P : Points)
	{
		Centroid += P;
	}
	Centroid /= (float)Points.Num();

	// 무게중심 기준 각도순 정렬 -> 별 모양 폴리곤 구성 (일반 아파트 평면에 유효)
	Points.Sort([&Centroid](const FVector2D& A, const FVector2D& B)
	{
		return FMath::Atan2(A.Y - Centroid.Y, A.X - Centroid.X) <
			FMath::Atan2(B.Y - Centroid.Y, B.X - Centroid.X);
	});

	FloorPolygon = Points;
}

void AInteriorPlacementManager::BuildWallSegments(const FHarnessFloorData& FloorData)
{
	InnerWallSegments.Empty();

	TSet<FString> OpeningEdgeIds;
	for (const FTopologyOpening& Opening : FloorData.openings)
	{
		OpeningEdgeIds.Add(Opening.target_edge_id);
		OpeningEdgeIds.Add(Opening.target_edge_id + TEXT("_twin"));
	}

	TMap<FString, FVector2D> VMap;
	for (const FTopologyVertex& V : FloorData.vertices)
	{
		VMap.Add(V.id, FVector2D(V.y, V.x));
	}

	TSet<FString> ProcessedTwinIds;
	for (const FTopologyHalfEdge& Edge : FloorData.half_edges)
	{
		if (Edge.type != TEXT("WallInner")) continue;
		if (ProcessedTwinIds.Contains(Edge.id)) continue;
		if (OpeningEdgeIds.Contains(Edge.id)) continue;

		ProcessedTwinIds.Add(Edge.twin_id);

		const FVector2D* StartPos = VMap.Find(Edge.vertex_start);
		const FVector2D* EndPos = VMap.Find(Edge.vertex_end);
		if (!StartPos || !EndPos) continue;
		if (FVector2D::DistSquared(*StartPos, *EndPos) < 1.0f) continue;

		InnerWallSegments.Add(TPair<FVector2D, FVector2D>(*StartPos, *EndPos));
	}
}

// Liang-Barsky 세그먼트-AABB 교차 판정
static bool SegmentIntersectsAABB(FVector2D P1, FVector2D P2, FVector2D BoxMin, FVector2D BoxMax)
{
	float tMin = 0.0f, tMax = 1.0f;
	const float dx = P2.X - P1.X;
	const float dy = P2.Y - P1.Y;

	auto Clip = [&](float p, float q) -> bool
	{
		if (FMath::Abs(p) < 1e-8f) return q >= 0.0f;
		const float r = q / p;
		if (p < 0.0f)
		{
			if (r > tMax)
			{
				return false;
			}
			if (r > tMin) tMin = r;
		}
		else
		{
			if (r < tMin)
			{
				return false;
			}
			if (r < tMax) tMax = r;
		}
		return true;
	};

	return Clip(-dx, P1.X - BoxMin.X) && Clip(dx, BoxMax.X - P1.X) &&
		Clip(-dy, P1.Y - BoxMin.Y) && Clip(dy, BoxMax.Y - P1.Y);
}

bool AInteriorPlacementManager::FurnitureIntersectsWalls(AFurniture* Target) const
{
	if (!Target || InnerWallSegments.IsEmpty())
	{
		return false;
	}

	FBox FurnWorldBox = Target->GetCollisionBounds().ExpandBy(-1.0f);
	FVector2D FurnMin(FurnWorldBox.Min.X, FurnWorldBox.Min.Y);
	FVector2D FurnMax(FurnWorldBox.Max.X, FurnWorldBox.Max.Y);

	for (int32 i = 0; i < InnerWallSegments.Num(); i++)
	{
		// 벽 세그먼트 자체가 가구 AABB를 관통하는지 검사
		// AABB-AABB 대신 세그먼트-AABB를 쓰면 10cm 얇은 벽도 50cm 셀을 넘어 감지 가능
		if (SegmentIntersectsAABB(InnerWallSegments[i].Key, InnerWallSegments[i].Value, FurnMin, FurnMax))
			return true;
	}
	return false;
}

bool AInteriorPlacementManager::IsFurnitureCornersInsideFloor(AFurniture* Target) const
{
	if (!Target || FloorPolygon.Num() < 3)
	{
		return true;
	}
	// AABB 네 모서리를 1cm 안쪽으로 줄여서 벽 경계선 오차 허용
	FBox Box = Target->GetCollisionBounds().ExpandBy(-1.0f);

	const FVector2D Corners[4] = {
		FVector2D(Box.Min.X, Box.Min.Y),
		FVector2D(Box.Max.X, Box.Min.Y),
		FVector2D(Box.Max.X, Box.Max.Y),
		FVector2D(Box.Min.X, Box.Max.Y),
	};

	for (const FVector2D& Corner : Corners)
	{
		if (!IsPointInPolygon(Corner, FloorPolygon))
		{
			return false;
		}
	}
	return true;
}

bool AInteriorPlacementManager::IsPointInPolygon(FVector2D Point, const TArray<FVector2D>& Polygon)
{
	if (Polygon.Num() < 3)
	{
		return true;
	}

	bool bInside = false;
	int32 N = Polygon.Num();
	for (int32 i = 0, j = N - 1; i < N; j = i++)
	{
		if (((Polygon[i].Y > Point.Y) != (Polygon[j].Y > Point.Y)) &&
			(Point.X < (Polygon[j].X - Polygon[i].X) * (Point.Y - Polygon[i].Y) /
				(Polygon[j].Y - Polygon[i].Y) + Polygon[i].X))
		{
			bInside = !bInside;
		}
	}
	return bInside;
}


void AInteriorPlacementManager::RefreshPlacementCellViz(AFurniture* Target, bool bInvalid)
{
	if (!PlacementVizValid || !PlacementVizInvalid || !Target) return;

	FBox Bounds = Target->GetCollisionBounds();
	FVector ActorLoc = GetActorLocation();
	constexpr float LocalZ = 0.5f;

	float X0 = Bounds.Min.X - ActorLoc.X;
	float Y0 = Bounds.Min.Y - ActorLoc.Y;
	float X1 = Bounds.Max.X - ActorLoc.X;
	float Y1 = Bounds.Max.Y - ActorLoc.Y;

	auto BuildRect = [&](UDynamicMeshComponent* Comp)
	{
		UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(Comp);
		UE::Geometry::FDynamicMesh3& Mesh = *DynMesh->GetMeshPtr();
		int32 v0 = Mesh.AppendVertex(FVector3d(X0, Y0, LocalZ));
		int32 v1 = Mesh.AppendVertex(FVector3d(X1, Y0, LocalZ));
		int32 v2 = Mesh.AppendVertex(FVector3d(X1, Y1, LocalZ));
		int32 v3 = Mesh.AppendVertex(FVector3d(X0, Y1, LocalZ));
		Mesh.AppendTriangle(v0, v1, v2);
		Mesh.AppendTriangle(v0, v2, v3);
		Comp->SetDynamicMesh(DynMesh);
		Comp->SetVisibility(true);
	};

	if (bInvalid)
	{
		BuildRect(PlacementVizInvalid);
		PlacementVizValid->SetVisibility(false);
	}
	else
	{
		BuildRect(PlacementVizValid);
		PlacementVizInvalid->SetVisibility(false);
	}
}

void AInteriorPlacementManager::ClearPlacementCellViz()
{
	if (PlacementVizValid)
	{
		PlacementVizValid->SetVisibility(false);
	}
	if (PlacementVizInvalid)
	{
		PlacementVizInvalid->SetVisibility(false);
	}
}

void AInteriorPlacementManager::MarkOutOfBoundsTiles()
{
	if (!Grid || FloorPolygon.Num() < 3) return;

	int L = Grid->GetLength();
	int B = Grid->GetBreadth();

	for (int x = 0; x < L; x++)
	{
		for (int y = 0; y < B; y++)
		{
			FVector WorldPos = Grid->ToWorldPosition(FVector2D(x, y));
			FVector2D WorldPos2D(WorldPos.X, WorldPos.Y);

			EGridTileState State = IsPointInPolygon(WorldPos2D, FloorPolygon)
				                       ? EGridTileState::Walkable
				                       : EGridTileState::None;

			Grid->SetTileState(FVector2D(x, y), State);
		}
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
	// 폴리곤 메시 준비돼 있으면 사용, 없으면 데칼 폴백
	if (GridMeshComp && FloorPolygon.Num() >= 3)
	{
		GridMeshComp->SetVisibility(bVisible);
		GridDecal->SetVisibility(false);
	}
	else
	{
		GridDecal->SetVisibility(bVisible);
	}
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

			// 도면 외부 타일이면 배치 불가
			if (Grid->GetTileState(Cell) == EGridTileState::None)
			{
				return false;
			}

			// 타 가구 충돌 체크
			AActor* ExistingFurniture = Grid->GetFurniture(Cell);
			if (ExistingFurniture != nullptr && ExistingFurniture != PreviewFurniture)
			{
				return false;
			}

			/*if (Grid->GetFurniture(Cell) != nullptr)
			{
				return false;
			}*/
		}
	}
	return true;
}

void AInteriorPlacementManager::ConfirmFurniture()
{
	if (!PreviewFurniture || !IsPreviewLotEmpty() || InvalidReason != EPlacementInvalidReason::None)
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

	PreviewFurniture->Tags.Add(TEXT("InteriorFurniture"));
	PreviewFurniture->Tags.Add(FName(FString::Printf(TEXT("ID_%d"), PreviewFurniture->FurnitureID)));

	PreviewFurniture->PlacedGridAnchor = PreviewGridAnchor;
	PreviewFurniture->PlacedDimensions = CurrentDimensions;

	PreviewFurniture->SetPlacementState(EPlacementState::Placed);
	PlacedFurnitures.Add(PreviewFurniture);
	PreviewFurniture = nullptr;
	ClearPlacementCellViz();

	// 월드 상태 변경 알림 (Subsystem 사용)
	if (UHarnessPipelineManager* PipelineManager = GetWorld()->GetSubsystem<UHarnessPipelineManager>())
	{
		PipelineManager->BroadcastWorldStateChanged();
	}
}

void AInteriorPlacementManager::CreatePreviewFurnitureFromRow(FVector RayPosition,
                                                              FRotator Rotation,
                                                              const FFurnitureDataRow& InFurnitureRow)
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
	if (!PreviewFurniture || !Grid) return;

	LastRayPosition = RayPosition;

	FVector2D GridPos = Grid->ToGridPosition(RayPosition);
	int SnapX = FMath::FloorToInt(GridPos.X);
	int SnapY = FMath::FloorToInt(GridPos.Y);

	int L = (int)CurrentDimensions.X;
	int B = (int)CurrentDimensions.Y;

	// 앵커는 int 나눗셈 — 홀수(L=1,3)·짝수(L=2,4) 모두 커서 셀 기준 올바른 중앙 확보
	PreviewGridAnchor.X = SnapX - L / 2;
	PreviewGridAnchor.Y = SnapY - B / 2;

	// 시각 중심은 앵커 + 크기/2 - 0.5 → 짝수 크기 가구도 점유 영역 정중앙에 렌더링
	FVector SnappedWorld = Grid->ToWorldPosition(FVector2D(
		(float)PreviewGridAnchor.X + ((float)L / 2.0f) - 0.5f,
		(float)PreviewGridAnchor.Y + ((float)B / 2.0f) - 0.5f
	));
	SnappedWorld.Z = GetActorLocation().Z;

	PreviewFurniture->SetActorLocation(SnappedWorld);
	PreviewFurniture->SetActorRotation(PreviewRotation);

	// 영역 이탈 검사
	bool bOutOfBounds = (PreviewGridAnchor.X < 0 || PreviewGridAnchor.Y < 0 ||
		(PreviewGridAnchor.X + L) > Grid->GetLength() ||
		(PreviewGridAnchor.Y + B) > Grid->GetBreadth());

	// 실시간 가구 겹침 + 도면 외부 검사
	bool bOverlapping = false;
	if (!bOutOfBounds)
	{
		for (int i = 0; i < L; i++)
		{
			for (int j = 0; j < B; j++)
			{
				FVector2D Cell(PreviewGridAnchor.X + i, PreviewGridAnchor.Y + j);

				if (Grid->GetTileState(Cell) == EGridTileState::None)
				{
					bOutOfBounds = true;
					break;
				}

				AActor* ExistingFurniture = Grid->GetFurniture(Cell);
				if (ExistingFurniture != nullptr && ExistingFurniture != PreviewFurniture)
				{
					bOverlapping = true;
					break;
				}
			}
			if (bOutOfBounds || bOverlapping) break;
		}
	}

	// AABB 실제 겹침 검사 — 그리드 미등록 가구(로드된 가구)까지 포함
	if (!bOutOfBounds && !bOverlapping)
	{
		FBox PreviewBox = PreviewFurniture->GetCollisionBounds().ExpandBy(-1.0f);
		for (AFurniture* Placed : PlacedFurnitures)
		{
			if (!IsValid(Placed)) continue;
			if (PreviewBox.Intersect(Placed->GetCollisionBounds()))
			{
				bOverlapping = true;
				break;
			}
		}
	}

	// 가구 AABB 네 모서리가 도면 다각형 안에 완전히 들어오는지 검사
	if (!bOutOfBounds && !IsFurnitureCornersInsideFloor(PreviewFurniture))
	{
		bOutOfBounds = true;
	}

	// 내벽 교차 검사
	if (!bOutOfBounds && FurnitureIntersectsWalls(PreviewFurniture))
	{
		bOutOfBounds = true;
	}

	if (!bOutOfBounds && !bOverlapping)
	{
		InvalidReason = EPlacementInvalidReason::None;
		PreviewFurniture->SetPlacementState(EPlacementState::Preview);
	}
	else
	{
		InvalidReason = bOutOfBounds
			                ? EPlacementInvalidReason::OutOfBounds
			                : EPlacementInvalidReason::Overlapping;

		PreviewFurniture->SetPlacementState(EPlacementState::Invalid);
	}

	RefreshPlacementCellViz(PreviewFurniture, bOutOfBounds || bOverlapping);
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

	// 월드 상태 변경 알림 (Subsystem 사용)
	if (UHarnessPipelineManager* PipelineManager = GetWorld()->GetSubsystem<UHarnessPipelineManager>())
	{
		PipelineManager->BroadcastWorldStateChanged();
	}
}

void AInteriorPlacementManager::CancelPreview()
{
	if (PreviewFurniture)
	{
		PreviewFurniture->Destroy();
		PreviewFurniture = nullptr;
	}

	InvalidReason = EPlacementInvalidReason::None;
	ClearPlacementCellViz();
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

void AInteriorPlacementManager::BeginGizmoMove(AFurniture* Target)
{
	if (!Target || !Grid) return;

	GizmoDragOriginalAnchor = Target->PlacedGridAnchor;
	GizmoDragStartLocation = Target->GetActorLocation();

	int L = (int)Target->PlacedDimensions.X;
	int B = (int)Target->PlacedDimensions.Y;

	// 드래그 중 자기 자신이 겹침 판정에 걸리지 않도록 그리드 셀에서 제거
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

	Target->SetPlacementState(EPlacementState::Preview);
	InvalidReason = EPlacementInvalidReason::None;
}

void AInteriorPlacementManager::UpdateGizmoMoveLocation(FVector CursorOnGround, AFurniture* Target, const FString& Axis)
{
	if (!Target || !Grid) return;
	
	FVector NewLoc = GizmoDragStartLocation;
	if (Axis == TEXT("MoveX")) NewLoc.X = CursorOnGround.X;
	else if (Axis == TEXT("MoveY")) NewLoc.Y = CursorOnGround.Y;

	Target->SetActorLocation(NewLoc);
	
	int L = (int)Target->PlacedDimensions.X;
	int B = (int)Target->PlacedDimensions.Y;

	FVector2D GridPos = Grid->ToGridPosition(NewLoc);
	int SnapX = FMath::RoundToInt(GridPos.X);
	int SnapY = FMath::RoundToInt(GridPos.Y);

	FVector2D NewAnchor(SnapX - L / 2, SnapY - B / 2);
	Target->PlacedGridAnchor = NewAnchor;
	
	bool bOutOfBounds = (NewAnchor.X < 0 || NewAnchor.Y < 0 ||
		(NewAnchor.X + L) > Grid->GetLength() ||
		(NewAnchor.Y + B) > Grid->GetBreadth());

	bool bOverlapping = false;
	if (!bOutOfBounds)
	{
		for (int i = 0; i < L && !bOverlapping; i++)
		{
			for (int j = 0; j < B && !bOverlapping; j++)
			{
				FVector2D Cell(NewAnchor.X + i, NewAnchor.Y + j);
				if (Grid->GetTileState(Cell) == EGridTileState::None)
				{
					bOutOfBounds = true;
					break;
				}
				if (Grid->GetFurniture(Cell) != nullptr)
				{
					bOverlapping = true;
				}
			}
		}
	}

	// AABB 실제 겹침 검사 — 자기 자신 제외
	if (!bOutOfBounds && !bOverlapping)
	{
		FBox TargetBox = Target->GetCollisionBounds().ExpandBy(-1.0f);
		for (AFurniture* Placed : PlacedFurnitures)
		{
			if (!IsValid(Placed) || Placed == Target) continue;
			if (TargetBox.Intersect(Placed->GetCollisionBounds()))
			{
				bOverlapping = true;
				break;
			}
		}
	}

	if (!bOutOfBounds && !IsFurnitureCornersInsideFloor(Target))
	{
		bOutOfBounds = true;
	}

	if (!bOutOfBounds && FurnitureIntersectsWalls(Target))
	{
		bOutOfBounds = true;
	}

	if (!bOutOfBounds && !bOverlapping)
	{
		InvalidReason = EPlacementInvalidReason::None;
		Target->SetPlacementState(EPlacementState::Preview);
	}
	else
	{
		InvalidReason = bOutOfBounds ? EPlacementInvalidReason::OutOfBounds : EPlacementInvalidReason::Overlapping;
		Target->SetPlacementState(EPlacementState::Invalid);
	}

	RefreshPlacementCellViz(Target, bOutOfBounds || bOverlapping);
}

void AInteriorPlacementManager::UpdateGizmoMoveFree(FVector TargetWorldLocation, AFurniture* Target)
{
	if (!Target || !Grid) return;

	FVector NewLoc = TargetWorldLocation;
	NewLoc.Z = GetActorLocation().Z;
	Target->SetActorLocation(NewLoc);

	int L = (int)Target->PlacedDimensions.X;
	int B = (int)Target->PlacedDimensions.Y;

	FVector2D GridPos = Grid->ToGridPosition(NewLoc);
	int SnapX = FMath::RoundToInt(GridPos.X);
	int SnapY = FMath::RoundToInt(GridPos.Y);

	FVector2D NewAnchor(SnapX - L / 2, SnapY - B / 2);
	Target->PlacedGridAnchor = NewAnchor;

	bool bOutOfBounds = (NewAnchor.X < 0 || NewAnchor.Y < 0 ||
		(NewAnchor.X + L) > Grid->GetLength() ||
		(NewAnchor.Y + B) > Grid->GetBreadth());

	bool bOverlapping = false;
	if (!bOutOfBounds)
	{
		for (int i = 0; i < L && !bOverlapping; i++)
		{
			for (int j = 0; j < B && !bOverlapping; j++)
			{
				FVector2D Cell(NewAnchor.X + i, NewAnchor.Y + j);
				if (Grid->GetTileState(Cell) == EGridTileState::None)
				{
					bOutOfBounds = true;
					break;
				}
				if (Grid->GetFurniture(Cell) != nullptr)
				{
					bOverlapping = true;
				}
			}
		}
	}

	// AABB 실제 겹침 검사 — 자기 자신 제외
	if (!bOutOfBounds && !bOverlapping)
	{
		FBox TargetBox = Target->GetCollisionBounds().ExpandBy(-1.0f);
		for (AFurniture* Placed : PlacedFurnitures)
		{
			if (!IsValid(Placed) || Placed == Target) continue;
			if (TargetBox.Intersect(Placed->GetCollisionBounds()))
			{
				bOverlapping = true;
				break;
			}
		}
	}

	if (!bOutOfBounds && !IsFurnitureCornersInsideFloor(Target))
	{
		bOutOfBounds = true;
	}

	if (!bOutOfBounds && FurnitureIntersectsWalls(Target))
	{
		bOutOfBounds = true;
	}

	if (!bOutOfBounds && !bOverlapping)
	{
		InvalidReason = EPlacementInvalidReason::None;
		Target->SetPlacementState(EPlacementState::Preview);
	}
	else
	{
		InvalidReason = bOutOfBounds ? EPlacementInvalidReason::OutOfBounds : EPlacementInvalidReason::Overlapping;
		Target->SetPlacementState(EPlacementState::Invalid);
	}

	RefreshPlacementCellViz(Target, bOutOfBounds || bOverlapping);
}

void AInteriorPlacementManager::AbortGizmoMove(AFurniture* Target)
{
	if (!Target || !Grid) return;

	// 드래그 시작 시 저장한 정확한 월드 위치로 복원 (스냅 없음)
	Target->SetActorLocation(GizmoDragStartLocation);
	Target->PlacedGridAnchor = GizmoDragOriginalAnchor;

	int L = (int)Target->PlacedDimensions.X;
	int B = (int)Target->PlacedDimensions.Y;
	for (int i = 0; i < L; i++)
	{
		for (int j = 0; j < B; j++)
		{
			Grid->SetFurniture(FVector2D(GizmoDragOriginalAnchor.X + i, GizmoDragOriginalAnchor.Y + j), Target);
		}
	}

	Target->SetPlacementState(EPlacementState::Placed);
	InvalidReason = EPlacementInvalidReason::None;
	ClearPlacementCellViz();
}

void AInteriorPlacementManager::FinalizeGizmoMove(AFurniture* Target)
{
	if (!Target || !Grid) return;

	int L = (int)Target->PlacedDimensions.X;
	int B = (int)Target->PlacedDimensions.Y;

	// 배치 불가 상태라면 드래그 시작 위치로 복귀
	FVector2D FinalAnchor = (InvalidReason == EPlacementInvalidReason::None)
		                        ? Target->PlacedGridAnchor
		                        : GizmoDragOriginalAnchor;

	Target->PlacedGridAnchor = FinalAnchor;

	// 그리드 타일 정중앙 월드 좌표로 탁! 스냅
	FVector SnappedLoc = Grid->ToWorldPosition(FVector2D(
		(float)FinalAnchor.X + ((float)L / 2.0f) - 0.5f,
		(float)FinalAnchor.Y + ((float)B / 2.0f) - 0.5f
	));
	SnappedLoc.Z = GetActorLocation().Z;

	Target->SetActorLocation(SnappedLoc);

	// 그리드 셀 재등록
	for (int i = 0; i < L; i++)
	{
		for (int j = 0; j < B; j++)
		{
			Grid->SetFurniture(FVector2D(FinalAnchor.X + i, FinalAnchor.Y + j), Target);
		}
	}

	Target->SetPlacementState(EPlacementState::Placed);
	InvalidReason = EPlacementInvalidReason::None;
	ClearPlacementCellViz();

	// 월드 상태 변경 알림 (Subsystem 사용)
	if (UHarnessPipelineManager* PipelineManager = GetWorld()->GetSubsystem<UHarnessPipelineManager>())
	{
		PipelineManager->BroadcastWorldStateChanged();
	}
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
		// InitializeGrid가 새 그리드를 생성하면서 GridOrigin이 (0,0)으로 리셋됨
		// 매니저 액터 위치를 기반으로 원점 복원
		FVector Loc = GetActorLocation();
		Grid->SetOrigin(FVector2D(Loc.X, Loc.Y));
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
		int32 GridX = Obj->GetIntegerField(TEXT("gridX"));
		int32 GridY = Obj->GetIntegerField(TEXT("gridY"));
		float Yaw = (float)Obj->GetNumberField(TEXT("rotationYaw"));

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

		float CenterGridX = (float)GridX + ((float)Dims.X / 2.0f);
		float CenterGridY = (float)GridY + ((float)Dims.Y / 2.0f);
		FVector SpawnLoc = Grid->ToWorldPosition(FVector2D(CenterGridX - 0.5f, CenterGridY - 0.5f));
		SpawnLoc.Z = GetActorLocation().Z;

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AFurniture* NewFurniture = GetWorld()->SpawnActor<AFurniture>(FurnitureClass,
		                                                              SpawnLoc,
		                                                              FRotator(0.0f, Yaw, 0.0f),
		                                                              Params);
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

	// 월드 상태 변경 알림 (Subsystem 사용)
	if (UHarnessPipelineManager* PipelineManager = GetWorld()->GetSubsystem<UHarnessPipelineManager>())
	{
		PipelineManager->BroadcastWorldStateChanged();
	}
}
