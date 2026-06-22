/*
// Fill out your copyright notice in the Description page of Project Settings.

#include "InteriorPlacementManager.h"
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
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"

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
	GridMeshComp->SetReceivesDecals(false);
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

	// 誘몃땲留SceneCapture먯꽌 洹몃━諛곗튂 쒓컖붽 蹂댁씠吏 딅룄濡≫꽣瑜④
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
		// BuildTopologyCaches 숈씪異留ㅽ븨: 꾨㈃ Y 붾뱶 X, 꾨㈃ X 붾뱶 Y
		MinX = FMath::Min(MinX, V.y);
		MaxX = FMath::Max(MaxX, V.y);
		MinY = FMath::Min(MinY, V.x);
		MaxY = FMath::Max(MaxY, V.x);
	}

	float TotalWidth = MaxX - MinX;
	float TotalHeight = MaxY - MinY;
	float CenterX = (MinX + MaxX) * 0.5f;
	float CenterY = (MinY + MaxY) * 0.5f;

	float FloorSurfaceZ = 0.0f;
	if (!FloorData.faces.IsEmpty())
	{
		FloorSurfaceZ = FloorData.faces[0].z_offset;
		for (const FTopologyFace& Face : FloorData.faces)
		{
			FloorSurfaceZ = FMath::Min(FloorSurfaceZ, Face.z_offset);
		}
	}
	SetActorLocation(FVector(CenterX, CenterY, FloorSurfaceZ + 1.0f));

	int Length = FMath::CeilToInt(TotalWidth / Cell);
	int Breadth = FMath::CeilToInt(TotalHeight / Cell);
	InitializeGrid(Length, Breadth, Cell);

	// 90뚯쟾곗뭡 濡쒖뺄 Y믪썡Y, 濡쒖뺄 Z믪썡X濡留ㅽ븨	// TotalHeight(붾뱶 Y) DecalSize.Y, TotalWidth(붾뱶 X) DecalSize.Z
	GridDecal->DecalSize = FVector(500.0f, TotalHeight * 0.5f + Cell, TotalWidth * 0.5f + Cell);

	if (Grid)
	{
		Grid->SetOrigin(FVector2D(CenterX, CenterY));
	}

	BuildFloorPolygon(FloorData);
	BuildWallSegments(FloorData);
	MarkOutOfBoundsTiles();
	RebuildGridMesh();
	ApplyWallTraceCollision();
}

void AInteriorPlacementManager::ApplyWallTraceCollision()
{
	// 踰媛援諛곗튂 媛먯몃젅댁뒪(ECC_GameTraceChannel1)媛 諛붾떏/泥쒖옣媛ㅼ吏 딅룄濡	// 섎땲ㅺ 앹꽦踰諛붾떏/泥쒖옣 硫붿떆肄쒕━묐떟議곗젙쒕떎.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TArray<UPrimitiveComponent*> Components;
		It->GetComponents<UPrimitiveComponent>(Components);

		for (UPrimitiveComponent* Comp : Components)
		{
			// EditableWall 쒓렇媛 녿뒗 紐⑤뱺 而댄룷뚰듃(붿뿬 釉뚮윭ы븿)			// 踰몃젅댁뒪 梨꾨꼸臾댁떆섍쾶 섏뿬, 섎룄移딆 異⑸룎踰먯젙媛濡쒖콈吏 紐삵븯寃쒕떎.
			if (Comp->ComponentHasTag(TEXT("EditableWall")))
			{
				Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				Comp->SetCollisionObjectType(ECC_WorldStatic);
				Comp->SetCollisionResponseToAllChannels(ECR_Block);
				Comp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
				Comp->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
				Comp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
				Comp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			}
			else
			{
				Comp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
			}
		}
	}
}

void AInteriorPlacementManager::RebuildGridMesh()
{
	if (!GridMeshComp || FloorPolygon.Num() < 3) return;

	FVector ActorLoc = GetActorLocation();

	// 臾닿쾶以묒떖
	FVector2D Centroid(0, 0);
	for (const FVector2D& P : FloorPolygon) Centroid += P;
	Centroid /= (float)FloorPolygon.Num();

	UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GridMeshComp);
	UE::Geometry::FDynamicMesh3& Mesh = *DynMesh->GetMeshPtr();

	constexpr float GridZ = 0.0f;

	int32 CenterIdx = Mesh.AppendVertex(FVector3d(Centroid.X, Centroid.Y, GridZ));

	// 대━怨뺤젏
	TArray<int32> VertIds;
	for (const FVector2D& P : FloorPolygon)
	{
		VertIds.Add(Mesh.AppendVertex(FVector3d(P.X, P.Y, GridZ)));
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

	// 쇨컖遺꾪븷
	for (int32 i = 0; i < N; i++)
	{
		int32 Va = VertIds[i];
		int32 Vb = VertIds[(i + 1) % N];

		Mesh.AppendTriangle(bCCW ? CenterIdx : Va, bCCW ? Vb : CenterIdx, bCCW ? Va : Vb);
	}

	GridMeshComp->SetDynamicMesh(DynMesh);

	GridMeshComp->SetWorldLocation(FVector(0.0f, 0.0f, ActorLoc.Z));

	if (GridMaterial)
	{
		GridDynMat = UMaterialInstanceDynamic::Create(GridMaterial, this);
		GridDynMat->SetScalarParameterValue(TEXT("CellSize"), GridCellSize);
		GridMeshComp->SetMaterial(0, GridDynMat);
	}
}

/*
void AInteriorPlacementManager::RebuildGridMesh()
{
	if (!GridMeshComp || FloorPolygon.Num() < 3) return;

	FVector ActorLoc = GetActorLocation();

	// 臾닿쾶以묒떖 (대 BuildFloorPolygon먯꽌 뺣젹 湲곗쇰줈 ъ슜媛
	FVector2D Centroid(0, 0);
	for (const FVector2D& P : FloorPolygon) Centroid += P;
	Centroid /= (float)FloorPolygon.Num();

	UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GridMeshComp);
	UE::Geometry::FDynamicMesh3& Mesh = *DynMesh->GetMeshPtr();

	constexpr float GridZ = 0.0f; // Mesh Decal 諛붾떏ъ쁺섎땲源Z ㅽ봽遺덊븘
	// 臾닿쾶以묒떖 뺤젏 (≫꽣 濡쒖뺄 醫뚰몴)
	int32 CenterIdx = Mesh.AppendVertex(
		FVector3d(Centroid.X - ActorLoc.X, Centroid.Y - ActorLoc.Y, GridZ));

	// 대━怨뺤젏
	TArray<int32> VertIds;
	for (const FVector2D& P : FloorPolygon)
	{
		VertIds.Add(Mesh.AppendVertex(
			FVector3d(P.X - ActorLoc.X, P.Y - ActorLoc.Y, GridZ)));
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

	// 쇨컖遺꾪븷
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
#1#

void AInteriorPlacementManager::BuildFloorPolygon(const FHarnessFloorData& FloorData)
{
	FloorPolygon.Empty();

	// WallOuter ｌ랁븳 뺤젏 ID 섏쭛
	TSet<FString> OuterIds;
	for (const FTopologyHalfEdge& Edge : FloorData.half_edges)
	{
		if (Edge.type == TEXT("WallOuter"))
		{
			OuterIds.Add(Edge.vertex_start);
			OuterIds.Add(Edge.vertex_end);
		}
	}

	// WallOuter媛 놁쑝硫꾩껜 뺤젏 ъ슜
	if (OuterIds.IsEmpty())
	{
		for (const FTopologyVertex& V : FloorData.vertices)
		{
			OuterIds.Add(V.id);
		}
	}

	// 뺤젏 醫뚰몴 留援ъ꽦 (BuildTopologyCaches 숈씪異留ㅽ븨)
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

	// 臾닿쾶以묒떖 怨꾩궛
	FVector2D Centroid(0, 0);
	for (const FVector2D& P : Points)
	{
		Centroid += P;
	}
	Centroid /= (float)Points.Num();

	// 臾닿쾶以묒떖 湲곗 媛곷룄뺣젹 -> 蹂紐⑥뼇 대━怨援ъ꽦 (쇰컲 꾪뙆됰㈃좏슚)
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
		// 踰媛援≪옄, 肄섏꽱대꼍/몃꼍 紐⑤몢 遺李媛		// ImpactNormal긽 ㅻ궡 諛⑺뼢대씪 몃꼍대씪"ㅻ궡 履踰쎈㈃"遺숈쓬
		if (Edge.type != TEXT("WallInner") && Edge.type != TEXT("WallOuter")) continue;
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

// 먯쓣 멸렇癒쇳듃ъ쁺, OutT(0~1) ъ쁺諛섑솚
static FVector2D ProjectPointOnSegment(FVector2D Point, FVector2D SegStart, FVector2D SegEnd, float& OutT)
{
	const FVector2D SegDir = SegEnd - SegStart;
	const float SegLenSq = SegDir.SizeSquared();
	if (SegLenSq < 1e-6f)
	{
		OutT = 0.0f;
		return SegStart;
	}

	OutT = FMath::Clamp(FVector2D::DotProduct(Point - SegStart, SegDir) / SegLenSq, 0.0f, 1.0f);
	return SegStart + SegDir * OutT;
}

// Liang-Barsky 멸렇癒쇳듃-AABB 援먯감 먯젙
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
		// 踰멸렇癒쇳듃 먯껜媛 媛援AABB瑜愿듯븯붿 寃		// AABB-AABB 멸렇癒쇳듃-AABB瑜곕㈃ 10cm 뉗 踰쎈룄 50cm 섏뼱 媛먯 媛		if (SegmentIntersectsAABB(InnerWallSegments[i].Key, InnerWallSegments[i].Value, FurnMin, FurnMax))
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
	// AABB 紐⑥꽌由щ 1cm 덉そ쇰줈 以꾩뿬踰寃쎄퀎ㅼ감 덉슜
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

	// InitializeFromFloorData먯꽌 꾨㈃ 덈 ш린 湲곗쇰줈 뼱곕濡ш린꾩떆媛믩쭔 ㅼ젙
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
	// 대━怨硫붿떆 以鍮꾨뤌 덉쑝硫ъ슜, 놁쑝硫곗뭡 대갚
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

			// 꾨㈃ 몃 쇱씠硫諛곗튂 遺덇
			if (Grid->GetTileState(Cell) == EGridTileState::None)
			{
				return false;
			}

			//  媛援異⑸룎 泥댄겕
			AActor* ExistingFurniture = Grid->GetFurniture(Cell);
			if (ExistingFurniture != nullptr && ExistingFurniture != PreviewFurniture)
			{
				return false;
			}

			/*if (Grid->GetFurniture(Cell) != nullptr)
			{
				return false;
			}#1#
		}
	}
	return true;
}

void AInteriorPlacementManager::ConfirmFurniture(bool bContinuePlacement)
{
	if (!PreviewFurniture || !Grid)
	{
		return;
	}

	if (PreviewFurniture->GetPlacementState() == EPlacementState::Invalid)
	{
		return;
	}

	RecordUndoSnapshot();

	const bool bIsWallPlacement = (CurrentPreviewSurfaceType == EPlacementSurfaceType::Wall);
	const bool bIsCeilingPlacement = (CurrentPreviewSurfaceType == EPlacementSurfaceType::Ceiling);

	// Wall/Ceiling 諛곗튂洹몃━먯쑀섏 딆쑝誘濡IsPreviewLotEmpty(諛붾떏 洹몃━湲곗) 寃곸씠 꾨떂
	if (!PreviewFurniture || InvalidReason != EPlacementInvalidReason::None)
	{
		return;
	}

	if (!bIsWallPlacement && !bIsCeilingPlacement && !IsPreviewLotEmpty())
	{
		return;
	}

	if (bIsWallPlacement)
	{
		PreviewFurniture->SetPlacedSurfaceType(EPlacementSurfaceType::Wall);
	}
	else if (bIsCeilingPlacement)
	{
		PreviewFurniture->SetPlacedSurfaceType(EPlacementSurfaceType::Ceiling);
	}
	else
	{
		int L = (int)CurrentDimensions.X;
		int B = (int)CurrentDimensions.Y;

		// Shift+대┃ 쇱씤 梨꾩슦湲 댁쟾 諛곗튂~꾩옱 꾨━酉ъ씠瑜媛援ш린留뚰겮 梨꾩
		if (bContinuePlacement && LineFillAnchor != PreviewGridAnchor)
		{
			FVector2D Delta = PreviewGridAnchor - LineFillAnchor;
			bool bAlongX = FMath::Abs(Delta.X) >= FMath::Abs(Delta.Y);

			int32 Step = bAlongX ? FMath::Max(L, 1) : FMath::Max(B, 1);
			int32 AxisDelta = bAlongX ? (int32)Delta.X : (int32)Delta.Y;
			int32 Dir = AxisDelta > 0 ? 1 : -1;
			// 留덉留移몄 PreviewGridAnchor(꾩옱 諛곗튂섎뒗 媛援媛 李⑥섎濡移곴쾶 梨꾩썙			// 留덉留쇱씤 媛援ъ 寃뱀튂吏 딆쓬
			int32 Count = FMath::Max(0, (FMath::Abs(AxisDelta) / Step) - 1);

			for (int32 i = 1; i <= Count; i++)
			{
				FVector2D Anchor = LineFillAnchor;
				if (bAlongX) Anchor.X += Dir * Step * i;
				else Anchor.Y += Dir * Step * i;

				if (Anchor == PreviewGridAnchor) continue;

				PlaceFurnitureCopyAtGridAnchor(Anchor, CurrentDimensions, PreviewRotation, CurrentFurnitureRow);
			}
		}

		for (int i = 0; i < L; i++)
		{
			for (int j = 0; j < B; j++)
			{
				Grid->SetFurniture(FVector2D(PreviewGridAnchor.X + i, PreviewGridAnchor.Y + j), PreviewFurniture);
			}
		}

		PreviewFurniture->PlacedGridAnchor = PreviewGridAnchor;
		PreviewFurniture->PlacedDimensions = CurrentDimensions;
		PreviewFurniture->SetPlacedSurfaceType(EPlacementSurfaceType::Floor);
	}

	PreviewFurniture->Tags.Add(TEXT("InteriorFurniture"));
	PreviewFurniture->Tags.Add(FName(FString::Printf(TEXT("ID_%d"), PreviewFurniture->FurnitureID)));

	PreviewFurniture->SetPlacementState(EPlacementState::Placed);
	PlacedFurnitures.Add(PreviewFurniture);
	PreviewFurniture = nullptr;
	ClearPlacementCellViz();

	// Shift+대┃ 곗냽 諛곗튂: 媛숈 媛援щ줈 꾨━酉諛붾줈 ㅼ떆 앹꽦
	if (bContinuePlacement)
	{
		CreatePreviewFurnitureFromRow(LastRayPosition, PreviewRotation, CurrentFurnitureRow);
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
	CurrentFurnitureRow = InFurnitureRow;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PreviewFurniture = GetWorld()->SpawnActor<AFurniture>(FurnitureClass, RayPosition, Rotation, Params);
	if (!PreviewFurniture)
	{
		return;
	}

	PreviewFurniture->ApplyFurnitureRow(InFurnitureRow);

	FHitResult InitialHit;
	InitialHit.Location = RayPosition;
	InitialHit.ImpactPoint = RayPosition;
	InitialHit.ImpactNormal = (RayPosition.IsZero()) ? FVector::UpVector : FVector(-1.f, 0.f, 0.f);
	UpdatePreviewLocation(InitialHit);
	
	LineFillAnchor = PreviewGridAnchor;
}

void AInteriorPlacementManager::AutoFillFurnitureDimensions()
{
	if (!FurnitureDataTable) return;

	TArray<FFurnitureDataRow*> Rows;
	FurnitureDataTable->GetAllRows<FFurnitureDataRow>(TEXT("AutoFillFurnitureDimensions"), Rows);

	for (FFurnitureDataRow* Row : Rows)
	{
		if (!Row || !Row->FurnitureMesh) continue;

		const FBoxSphereBounds MeshBounds = Row->FurnitureMesh->GetBounds();
		const int32 DimX = FMath::Max(1, FMath::CeilToInt((MeshBounds.BoxExtent.X * 2.0f) / GridCellSize));
		const int32 DimY = FMath::Max(1, FMath::CeilToInt((MeshBounds.BoxExtent.Y * 2.0f) / GridCellSize));

		Row->Dimensions = FIntPoint(DimX, DimY);
	}

	FurnitureDataTable->MarkPackageDirty();
}

void AInteriorPlacementManager::PlaceFurnitureCopyAtGridAnchor(FVector2D GridAnchor, FVector2D Dimensions, FRotator Rotation, const FFurnitureDataRow& InFurnitureRow)
{
	if (!Grid || !FurnitureClass) return;

	int32 L = (int32)Dimensions.X;
	int32 B = (int32)Dimensions.Y;

	bool bOutOfBounds = (GridAnchor.X < 0 || GridAnchor.Y < 0 ||
		(GridAnchor.X + L) > Grid->GetLength() ||
		(GridAnchor.Y + B) > Grid->GetBreadth());

	bool bOverlapping = false;
	if (!bOutOfBounds)
	{
		for (int i = 0; i < L && !bOutOfBounds && !bOverlapping; i++)
		{
			for (int j = 0; j < B; j++)
			{
				FVector2D Cell(GridAnchor.X + i, GridAnchor.Y + j);

				if (Grid->GetTileState(Cell) == EGridTileState::None)
				{
					bOutOfBounds = true;
					break;
				}

				if (Grid->GetFurniture(Cell) != nullptr)
				{
					bOverlapping = true;
					break;
				}
			}
		}
	}

	if (bOutOfBounds || bOverlapping) return;

	FVector World = Grid->ToWorldPosition(FVector2D(
		(float)GridAnchor.X + ((float)L / 2.0f) - 0.5f,
		(float)GridAnchor.Y + ((float)B / 2.0f) - 0.5f
	));
	World.Z = GetActorLocation().Z;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFurniture* NewFurniture = GetWorld()->SpawnActor<AFurniture>(FurnitureClass, World, Rotation, Params);
	if (!NewFurniture) return;

	NewFurniture->ApplyFurnitureRow(InFurnitureRow);
	NewFurniture->SetActorLocation(World);
	NewFurniture->SetActorRotation(Rotation);

	// 꾨㈃ 멸낸/대꼍/ㅻⅨ 媛援ъ AABB 寃뱀묠 寃 쇱씤 以묎컙踰쎌씠媛援덉쑝硫洹移몄 ㅽ궢
	if (!IsFurnitureCornersInsideFloor(NewFurniture) || FurnitureIntersectsWalls(NewFurniture))
	{
		NewFurniture->Destroy();
		return;
	}

	FBox NewBounds = NewFurniture->GetCollisionBounds().ExpandBy(-1.0f);
	for (AFurniture* Placed : PlacedFurnitures)
	{
		if (!IsValid(Placed)) continue;
		if (NewBounds.Intersect(Placed->GetCollisionBounds()))
		{
			NewFurniture->Destroy();
			return;
		}
	}

	for (int i = 0; i < L; i++)
	{
		for (int j = 0; j < B; j++)
		{
			Grid->SetFurniture(FVector2D(GridAnchor.X + i, GridAnchor.Y + j), NewFurniture);
		}
	}

	NewFurniture->PlacedGridAnchor = GridAnchor;
	NewFurniture->PlacedDimensions = Dimensions;
	NewFurniture->SetPlacedSurfaceType(EPlacementSurfaceType::Floor);

	NewFurniture->Tags.Add(TEXT("InteriorFurniture"));
	NewFurniture->Tags.Add(FName(FString::Printf(TEXT("ID_%d"), NewFurniture->FurnitureID)));

	NewFurniture->SetPlacementState(EPlacementState::Placed);
	PlacedFurnitures.Add(NewFurniture);
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

	FHitResult RotateHit;
	RotateHit.Location = LastRayPosition;
	RotateHit.ImpactPoint = LastRayPosition;

	if (CurrentPreviewSurfaceType == EPlacementSurfaceType::Wall)
	{
		// 踰몃(諛李ㅽ봽湲곗) CurrentWallNormal蹂닿媛믪쓣 洹몃濡ъ슜
		UpdatePreviewLocationOnWall(RotateHit);
	}
	else if (CurrentPreviewSurfaceType == EPlacementSurfaceType::Ceiling)
	{
		UpdatePreviewLocationOnCeiling(RotateHit);
	}
	else
	{
		RotateHit.ImpactNormal = FVector::UpVector;
		UpdatePreviewLocationOnFloor(RotateHit);
	}
}

EPlacementSurfaceType AInteriorPlacementManager::DetermineHitSurfaceType(const FHitResult& CursorHit) const
{
	const UPrimitiveComponent* HitComp = CursorHit.GetComponent();
	if (HitComp && HitComp->ComponentHasTag(TEXT("Ceiling")))
	{
		return EPlacementSurfaceType::Ceiling;
	}

	float FloorThresholdZ = GetActorLocation().Z + 5.0f;

	// 泥쒖옣 諛곗튂 媛援щ뒗 ISO 쒖젏먯꽌 而ㅼ꽌 덉씠媛 泥쒖옣 諛묐㈃욧린 꾩뿉
	// 踰쀫遺꾩씠ㅻⅨ 而댄룷뚰듃瑜癒쇱 留욏엳寃쎌슦媛 덉쓬.
	// 諛붾떏 諛곗튂瑜吏먰븯吏 딅뒗(泥쒖옣 꾩슜) 媛援ш 믪 꾩튂먯꽌 ≫엳硫泥쒖옣쇰줈 媛꾩＜쒕떎.
	if (PreviewFurniture
		&& PreviewFurniture->SupportsPlacementType(EPlacementSurfaceType::Ceiling)
		&& !PreviewFurniture->SupportsPlacementType(EPlacementSurfaceType::Floor)
		&& CursorHit.Location.Z > FloorThresholdZ)
	{
		return EPlacementSurfaceType::Ceiling;
	}

	if (HitComp && HitComp->ComponentHasTag(TEXT("EditableWall")))
	{
		return EPlacementSurfaceType::Wall;
	}

	if (CursorHit.Location.Z > FloorThresholdZ)
	{
		return EPlacementSurfaceType::Wall;
	}

	// TODO: Surface(媛援쒕㈃) 먯젙
	return EPlacementSurfaceType::Floor;
}

void AInteriorPlacementManager::UpdatePreviewLocation(const FHitResult& CursorHit)
{
	if (!PreviewFurniture || !Grid) return;

	const EPlacementSurfaceType HitSurfaceType = DetermineHitSurfaceType(CursorHit);
	CurrentPreviewSurfaceType = HitSurfaceType;
	
	if (!PreviewFurniture->SupportsPlacementType(HitSurfaceType))
	{
		InvalidReason = EPlacementInvalidReason::UnsupportedSurface;
		PreviewFurniture->SetPlacementState(EPlacementState::Invalid);
		ClearPlacementCellViz();
		return;
	}

	if (HitSurfaceType == EPlacementSurfaceType::Wall)
	{
		// 몃젅댁뒪먯꽌 살 踰몃 (諛李ㅽ봽怨꾩궛 뚯쟾 以묒뿏 媛깆떊 
		CurrentWallNormal = CursorHit.ImpactNormal;
		UpdatePreviewLocationOnWall(CursorHit);
	}
	else if (HitSurfaceType == EPlacementSurfaceType::Ceiling)
	{
		UpdatePreviewLocationOnCeiling(CursorHit);
	}
	else
	{
		UpdatePreviewLocationOnFloor(CursorHit);
	}
}

void AInteriorPlacementManager::UpdatePreviewLocationOnFloor(const FHitResult& CursorHit)
{
	if (!PreviewFurniture || !Grid) return;

	const FVector RayPosition = CursorHit.Location;
	LastRayPosition = RayPosition;

	FVector2D GridPos = Grid->ToGridPosition(RayPosition);
	int SnapX = FMath::FloorToInt(GridPos.X);
	int SnapY = FMath::FloorToInt(GridPos.Y);

	int L = (int)CurrentDimensions.X;
	int B = (int)CurrentDimensions.Y;

	// 듭빱int 섎닓덉쑝濡怨꾩궛. L=1,3)쨌吏앹닔(L=2,4) 而ㅼ꽌  湲곗 以묒븰 留욎쓬
	PreviewGridAnchor.X = SnapX - L / 2;
	PreviewGridAnchor.Y = SnapY - B / 2;
	
	for (AFurniture* LoopPreview : LinePreviewFurnitures)
	{
		if (IsValid(LoopPreview))
		{
			LoopPreview->Destroy();
		}
	}
	LinePreviewFurnitures.Empty();

	// 쒓컖 以묒떖 듭빱 + ш린/2 - 0.5 吏앹닔 ш린 媛援щ룄 먯쑀 곸뿭 뺤쨷숈뿉 뚮뜑留	FVector SnappedWorld = Grid->ToWorldPosition(FVector2D(
		(float)PreviewGridAnchor.X + ((float)L / 2.0f) - 0.5f,
		(float)PreviewGridAnchor.Y + ((float)B / 2.0f) - 0.5f
	));
	SnappedWorld.Z = GetActorLocation().Z;

	PreviewFurniture->SetActorLocation(SnappedWorld);
	PreviewFurniture->SetActorRotation(PreviewRotation);

	// 곸뿭 댄깉 寃	bool bOutOfBounds = (PreviewGridAnchor.X < 0 || PreviewGridAnchor.Y < 0 ||
		(PreviewGridAnchor.X + L) > Grid->GetLength() ||
		(PreviewGridAnchor.Y + B) > Grid->GetBreadth());

	// ㅼ떆媛媛援寃뱀묠 + 꾨㈃ 몃 寃	bool bOverlapping = false;
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

	// AABB ㅼ젣 寃뱀묠 寃(洹몃━誘몃벑濡媛援 利濡쒕뱶媛援щ룄 ы븿)
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

	// 媛援AABB 紐⑥꽌由ш 꾨㈃ ㅺ컖덉뿉 꾩쟾ㅼ뼱ㅻ뒗吏 寃	if (!bOutOfBounds && !IsFurnitureCornersInsideFloor(PreviewFurniture))
	{
		bOutOfBounds = true;
	}

	// 대꼍 援먯감 寃	if (!bOutOfBounds && FurnitureIntersectsWalls(PreviewFurniture))
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

void AInteriorPlacementManager::UpdatePreviewLocationOnWall(const FHitResult& CursorHit)
{
	if (!PreviewFurniture || InnerWallSegments.IsEmpty()) return;

	const FVector2D HitPoint2D(CursorHit.Location.X, CursorHit.Location.Y);

	// 而ㅼ꽌 媛媛源뚯슫 踰멸렇癒쇳듃 먯깋
	float BestDistSq = TNumericLimits<float>::Max();
	FVector2D BestSegStart = FVector2D::ZeroVector;
	FVector2D BestSegEnd = FVector2D::ZeroVector;
	float BestT = 0.0f;

	for (const TPair<FVector2D, FVector2D>& Segment : InnerWallSegments)
	{
		float T;
		const FVector2D Projected = ProjectPointOnSegment(HitPoint2D, Segment.Key, Segment.Value, T);
		const float DistSq = FVector2D::DistSquared(HitPoint2D, Projected);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestSegStart = Segment.Key;
			BestSegEnd = Segment.Value;
			BestT = T;
		}
	}

	// 뚯쟾 먮룞 뺣젹 섍퀬 Rㅻ줈 ㅼ젙PreviewRotation 洹몃濡ъ슜
	const FVector2D SegDir = BestSegEnd - BestSegStart;
	const float SegLength = SegDir.Size();
	const FVector2D SegDirNorm = SegLength > KINDA_SMALL_NUMBER ? SegDir / SegLength : FVector2D(1.0f, 0.0f);

	// 뚯쟾癒쇱 곸슜댁빞 肄쒕━諛뺤뒪붾뱶 諛붿슫쒓 꾩옱 諛⑺뼢諛섏쁺	PreviewFurniture->SetActorRotation(PreviewRotation);

	const FBox CollisionBox = PreviewFurniture->GetCollisionBounds();
	const FVector BoxExtent = CollisionBox.GetExtent();
	const FVector BoxCenterOffset = CollisionBox.GetCenter() - PreviewFurniture->GetActorLocation();
	
	const float HalfFurnitureWidth = FMath::Abs(BoxExtent.X * SegDirNorm.X) + FMath::Abs(BoxExtent.Y * SegDirNorm.Y);
	float SnappedDist = FMath::RoundToFloat((BestT * SegLength) / GridCellSize) * GridCellSize;
	SnappedDist = FMath::Clamp(SnappedDist, HalfFurnitureWidth, SegLength - HalfFurnitureWidth);

	const FVector2D SnappedXY = BestSegStart + SegDirNorm * SnappedDist;

	// Z: 洹몃━⑥쐞 ㅻ깄, 諛붾떏 꾨옒濡쒕뒗 紐대젮媛寃대옩	const float FloorZ = GetActorLocation().Z;
	const float SnappedZ = FMath::Max(FMath::RoundToFloat(CursorHit.Location.Z / GridCellSize) * GridCellSize, FloorZ);

	// 肄쒕━諛뺤뒪 硫댁씠 踰쎌뿉 留욌떯꾨줉 諛뺤뒪 붾뱶 諛붿슫쒕 踰몃ъ쁺댁꽌 먭퍡 援ы븯怨	// WallOffset留뚰겮 踰몃 諛⑺뼢쇰줈 異붽濡꾩
	const FFurnitureDataRow* Row = FindFurnitureRowByID(PreviewFurniture->FurnitureID);
	const float WallOffset = Row ? Row->WallOffset : 0.0f;
	const FVector2D Normal2D(CurrentWallNormal.X, CurrentWallNormal.Y);

	// 肄쒕━諛뺤뒪瑜踰몃濡ъ쁺諛섑룺, 쇰쿁믩컯ㅼ쨷ㅽ봽뗭쓽 踰몃 깅텇
	const float ExtentAlongNormal = FMath::Abs(BoxExtent.X * Normal2D.X) + FMath::Abs(BoxExtent.Y * Normal2D.Y);
	const float OriginAlongNormal = BoxCenterOffset.X * Normal2D.X + BoxCenterOffset.Y * Normal2D.Y;

	// InnerWallSegments踰以묒떖좎씠ㅻ궡痢踰쎈㈃源뚯 踰먭퍡 덈컲留뚰겮 異붽濡諛대깂
	const float WallSurfaceOffset = WallThickness * 0.5f;

	const float PushOffset = WallSurfaceOffset + (ExtentAlongNormal - OriginAlongNormal) + WallOffset;
	const FVector2D FinalXY = SnappedXY + Normal2D * PushOffset;

	const FVector FinalLocation(FinalXY.X, FinalXY.Y, SnappedZ);
	LastRayPosition = FinalLocation;
	
	PreviewFurniture->SetActorLocation(FinalLocation);

	// 踰媛援щ뒗 洹몃━꾨㈃ 寃ㅻⅨ 諛곗튂 媛援ъAABB 寃뱀묠留寃	bool bOverlapping = false;
	const FBox PreviewBox = PreviewFurniture->GetCollisionBounds().ExpandBy(-2.0f);
	for (AFurniture* Placed : PlacedFurnitures)
	{
		if (!IsValid(Placed)) continue;
		if (PreviewBox.Intersect(Placed->GetCollisionBounds()))
		{
			bOverlapping = true;
			break;
		}
	}

	if (!bOverlapping)
	{
		InvalidReason = EPlacementInvalidReason::None;
		PreviewFurniture->SetPlacementState(EPlacementState::Preview);
	}
	else
	{
		InvalidReason = EPlacementInvalidReason::Overlapping;
		PreviewFurniture->SetPlacementState(EPlacementState::Invalid);
	}

	RefreshPlacementCellViz(PreviewFurniture, bOverlapping);
}

void AInteriorPlacementManager::UpdatePreviewLocationOnCeiling(const FHitResult& CursorHit)
{
	if (!PreviewFurniture) return;

	// 泥쒖옣留ㅻ떖由щ룄濡꾩븘섎 ㅼ쭛댁꽌 곸슜 (Rㅻ줈 ㅼ젙Yaw좎).
	// Roll 180꾨뒗 뺣㈃(濡쒖뺄 X) 諛⑺뼢 洹몃濡먭퀬 꾨옒留ㅼ쭛쇰濡	// 諛붾떏 媛援ъ 숈씪Yaw 媛믪씪 媛숈 諛⑺뼢蹂대㈃ㅼ쭛뚮떎.
	FRotator CeilingRotation = PreviewRotation;
	CeilingRotation.Roll = 180.0f;
	PreviewFurniture->SetActorRotation(CeilingRotation);

	// XY洹몃━⑥쐞濡ㅻ깄
	const float SnappedX = FMath::RoundToFloat(CursorHit.Location.X / GridCellSize) * GridCellSize;
	const float SnappedY = FMath::RoundToFloat(CursorHit.Location.Y / GridCellSize) * GridCellSize;

	// CursorHit踰ㅻⅨ 而댄룷뚰듃瑜留욏엺 寃쎌슦 洹Z媛믪 泥쒖옣 믪씠媛 꾨땲誘濡
	// ㅻ깄XY 꾩튂먯꽌 꾨줈 ㅼ떆 몃젅댁뒪ㅼ젣 泥쒖옣(Ceiling 쒓렇) 諛묐㈃ Z瑜援ы븳
	float CeilingZ = CursorHit.Location.Z;
	{
		FHitResult CeilHit;
		FCollisionQueryParams Params(NAME_None, true);
		Params.AddIgnoredActor(PreviewFurniture);
		Params.AddIgnoredActor(this);

		const FVector TraceStart(SnappedX, SnappedY, GetActorLocation().Z);
		const FVector TraceEnd(SnappedX, SnappedY, GetActorLocation().Z + 100000.0f);

		if (GetWorld()->LineTraceSingleByChannel(CeilHit, TraceStart, TraceEnd, ECC_Visibility, Params)
			&& CeilHit.GetComponent() && CeilHit.GetComponent()->ComponentHasTag(TEXT("Ceiling")))
		{
			CeilingZ = CeilHit.Location.Z;
		}
	}

	// Roll 180꾨줈 ㅼ쭛쇰㈃ 硫붿떆 섎떒꾨 ν븯誘濡 쇰쿁-諛붾떏 ㅽ봽뗭쓣 뷀빐
	// 硫붿떆 쀫㈃(ㅼ쭛섎떒)泥쒖옣 諛묐㈃뺥솗諛李⑸릺꾨줉 쒕떎.
	// (쇰쿁硫붿떆 以묒븰대㈃ ㅽ봽뗭씠 뚯닔媛 섏뼱 ≫꽣瑜泥쒖옣蹂대떎 꾨옒濡대┝)
	const FVector FinalLocation(SnappedX, SnappedY, CeilingZ + PreviewFurniture->GetPivotToBottomOffsetZ());
	LastRayPosition = FinalLocation;

	PreviewFurniture->SetActorLocation(FinalLocation);

	// 諛붾떏 媛援ъ 숈씪섍쾶 꾨㈃ 諛踰愿щ瑜寃(泥쒖옣 議곕챸踰쎌쓣 リ퀬 섍硫
	bool bOutOfBounds = false;
	if (!IsFurnitureCornersInsideFloor(PreviewFurniture))
	{
		bOutOfBounds = true;
	}
	if (!bOutOfBounds && FurnitureIntersectsWalls(PreviewFurniture))
	{
		bOutOfBounds = true;
	}

	// 泥쒖옣 媛援щ뒗 洹몃━먯쑀 寃ㅻⅨ 諛곗튂 媛援ъAABB 寃뱀묠留寃	bool bOverlapping = false;
	if (!bOutOfBounds)
	{
		const FBox PreviewBox = PreviewFurniture->GetCollisionBounds().ExpandBy(-2.0f);
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

	if (!bOutOfBounds && !bOverlapping)
	{
		InvalidReason = EPlacementInvalidReason::None;
		PreviewFurniture->SetPlacementState(EPlacementState::Preview);
	}
	else
	{
		InvalidReason = bOutOfBounds ? EPlacementInvalidReason::OutOfBounds : EPlacementInvalidReason::Overlapping;
		PreviewFurniture->SetPlacementState(EPlacementState::Invalid);
	}

	RefreshPlacementCellViz(PreviewFurniture, bOutOfBounds || bOverlapping);
}

bool AInteriorPlacementManager::FindNearestWallSegment(const FVector2D& Point2D, FVector2D& OutSegStart, FVector2D& OutSegEnd) const
{
	if (InnerWallSegments.IsEmpty()) return false;

	float BestDistSq = TNumericLimits<float>::Max();
	for (const TPair<FVector2D, FVector2D>& Segment : InnerWallSegments)
	{
		float T;
		const FVector2D Projected = ProjectPointOnSegment(Point2D, Segment.Key, Segment.Value, T);
		const float DistSq = FVector2D::DistSquared(Point2D, Projected);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			OutSegStart = Segment.Key;
			OutSegEnd = Segment.Value;
		}
	}
	return true;
}

// 踰媛援щ SegStart-SegEnd 踰멸렇癒쇳듃瑜곕씪 CursorXY媛媛源뚯슫 꾩튂濡ㅻ깄섍퀬,
// 踰쎈㈃諛李⑸릺꾨줉 踰몃 諛⑺뼢쇰줈 諛대궦 理쒖쥌 꾩튂瑜諛섑솚 (Z洹몃濡좎)
FVector AInteriorPlacementManager::ComputeWallSnappedLocation(AFurniture* Target, const FVector2D& CursorXY, const FVector2D& SegStart, const FVector2D& SegEnd, float Z) const
{
	float CursorT;
	ProjectPointOnSegment(CursorXY, SegStart, SegEnd, CursorT);

	const FVector2D SegDir = SegEnd - SegStart;
	const float SegLength = SegDir.Size();
	const FVector2D SegDirNorm = SegLength > KINDA_SMALL_NUMBER ? SegDir / SegLength : FVector2D(1.0f, 0.0f);

	const FBox CollisionBox = Target->GetCollisionBounds();
	const FVector BoxExtent = CollisionBox.GetExtent();
	const FVector BoxCenterOffset = CollisionBox.GetCenter() - Target->GetActorLocation();

	const float HalfFurnitureWidth = FMath::Abs(BoxExtent.X * SegDirNorm.X) + FMath::Abs(BoxExtent.Y * SegDirNorm.Y);
	float SnappedDist = FMath::RoundToFloat((CursorT * SegLength) / GridCellSize) * GridCellSize;
	SnappedDist = FMath::Clamp(SnappedDist, HalfFurnitureWidth, SegLength - HalfFurnitureWidth);

	const FVector2D SnappedXY = SegStart + SegDirNorm * SnappedDist;

	// 踰몃: 멸렇癒쇳듃섏쭅諛⑺뼢 以 媛援ъ쓽 꾩옱 꾩튂媛 덈뒗 履쎌쓣 좏깮
	FVector2D Normal2D(-SegDirNorm.Y, SegDirNorm.X);
	const FVector2D ActorXY(Target->GetActorLocation().X, Target->GetActorLocation().Y);
	float ActorT;
	const FVector2D ProjectedActor = ProjectPointOnSegment(ActorXY, SegStart, SegEnd, ActorT);
	if (FVector2D::DotProduct(Normal2D, ActorXY - ProjectedActor) < 0.0f)
	{
		Normal2D = -Normal2D;
	}

	const float ExtentAlongNormal = FMath::Abs(BoxExtent.X * Normal2D.X) + FMath::Abs(BoxExtent.Y * Normal2D.Y);
	const float OriginAlongNormal = BoxCenterOffset.X * Normal2D.X + BoxCenterOffset.Y * Normal2D.Y;

	const FFurnitureDataRow* Row = FindFurnitureRowByID(Target->FurnitureID);
	const float WallOffset = Row ? Row->WallOffset : 0.0f;
	const float WallSurfaceOffset = WallThickness * 0.5f;

	const float PushOffset = WallSurfaceOffset + (ExtentAlongNormal - OriginAlongNormal) + WallOffset;
	const FVector2D FinalXY = SnappedXY + Normal2D * PushOffset;

	return FVector(FinalXY.X, FinalXY.Y, Z);
}

void AInteriorPlacementManager::RemoveFurniture(AFurniture* Target)
{
	if (!Target || !Grid)
	{
		return;
	}
	
	RecordUndoSnapshot();

	if (Target->GetPlacedSurfaceType() == EPlacementSurfaceType::Floor)
	{
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

void AInteriorPlacementManager::PushUndoSnapshot(const FString& Snapshot)
{
	if (bRestoringHistory)
	{
		return;
	}

	if (Snapshot.IsEmpty())
	{
		return;
	}

	if (UndoStack.Num() > 0 && UndoStack.Last() == Snapshot)
	{
		return;
	}

	UndoStack.Add(Snapshot);
	RedoStack.Empty();

	if (UndoStack.Num() > MaxHistoryCount)
	{
		UndoStack.RemoveAt(0);
	}
}

void AInteriorPlacementManager::RecordUndoSnapshot()
{
	PushUndoSnapshot(ExportEditStateJson());
}

void AInteriorPlacementManager::Undo()
{
	if (UndoStack.Num() <= 0)
	{
		return;
	}

	const FString CurrentSnapshot = ExportEditStateJson();
	RedoStack.Add(CurrentSnapshot);

	const FString PreviousSnapshot = UndoStack.Pop();

	bRestoringHistory = true;
	ImportEditStateJson(PreviousSnapshot);
	bRestoringHistory = false;
}

void AInteriorPlacementManager::Redo()
{
	if (RedoStack.Num() <= 0)
	{
		return;
	}

	const FString CurrentSnapshot = ExportEditStateJson();
	UndoStack.Add(CurrentSnapshot);

	const FString NextSnapshot = RedoStack.Pop();

	bRestoringHistory = true;
	ImportEditStateJson(NextSnapshot);
	bRestoringHistory = false;
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

		// TODO: 踰泥쒖옣 媛援蹂듭썝
		if (Placed->GetPlacedSurfaceType() != EPlacementSurfaceType::Floor)
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

	PendingGizmoUndoSnapshot = ExportPlacedFurnituresJson();
	bHasPendingGizmoUndoSnapshot = true;

	GizmoDragStartLocation = Target->GetActorLocation();

	if (Target->GetPlacedSurfaceType() == EPlacementSurfaceType::Wall)
	{
		const FVector2D ActorXY(GizmoDragStartLocation.X, GizmoDragStartLocation.Y);
		FindNearestWallSegment(ActorXY, GizmoWallSegStart, GizmoWallSegEnd);

		Target->SetPlacementState(EPlacementState::Preview);
		InvalidReason = EPlacementInvalidReason::None;
		return;
	}

	if (Target->GetPlacedSurfaceType() == EPlacementSurfaceType::Ceiling)
	{
		Target->SetPlacementState(EPlacementState::Preview);
		InvalidReason = EPlacementInvalidReason::None;
		return;
	}

	GizmoDragOriginalAnchor = Target->PlacedGridAnchor;

	int L = (int)Target->PlacedDimensions.X;
	int B = (int)Target->PlacedDimensions.Y;

	// 쒕옒洹以먭린 먯떊寃뱀묠 먯젙嫄몃━吏 딅룄濡洹몃━먯꽌 쒓굅
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

	if (Target->GetPlacedSurfaceType() == EPlacementSurfaceType::Wall)
	{
		if (Axis != TEXT("MoveX") && Axis != TEXT("MoveY")) return;

		const FVector2D CursorXY(CursorOnGround.X, CursorOnGround.Y);
		const FVector NewWallLoc = ComputeWallSnappedLocation(Target, CursorXY, GizmoWallSegStart, GizmoWallSegEnd, GizmoDragStartLocation.Z);
		Target->SetActorLocation(NewWallLoc);

		bool bOverlapping = false;
		const FBox TargetBox = Target->GetCollisionBounds().ExpandBy(-1.0f);
		for (AFurniture* Placed : PlacedFurnitures)
		{
			if (!IsValid(Placed) || Placed == Target) continue;
			if (TargetBox.Intersect(Placed->GetCollisionBounds()))
			{
				bOverlapping = true;
				break;
			}
		}

		InvalidReason = bOverlapping ? EPlacementInvalidReason::Overlapping : EPlacementInvalidReason::None;
		Target->SetPlacementState(bOverlapping ? EPlacementState::Invalid : EPlacementState::Preview);
		RefreshPlacementCellViz(Target, bOverlapping);
		return;
	}

	if (Target->GetPlacedSurfaceType() == EPlacementSurfaceType::Ceiling)
	{
		if (Axis != TEXT("MoveX") && Axis != TEXT("MoveY")) return;

		FVector NewLoc = Target->GetActorLocation();
		if (Axis == TEXT("MoveX"))
		{
			NewLoc.X = FMath::RoundToFloat(CursorOnGround.X / GridCellSize) * GridCellSize;
		}
		else
		{
			NewLoc.Y = FMath::RoundToFloat(CursorOnGround.Y / GridCellSize) * GridCellSize;
		}
		NewLoc.Z = GizmoDragStartLocation.Z;
		Target->SetActorLocation(NewLoc);

		bool bOutOfBounds = !IsFurnitureCornersInsideFloor(Target) || FurnitureIntersectsWalls(Target);

		bool bOverlapping = false;
		if (!bOutOfBounds)
		{
			const FBox TargetBox = Target->GetCollisionBounds().ExpandBy(-1.0f);
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

		InvalidReason = bOutOfBounds ? EPlacementInvalidReason::OutOfBounds : (bOverlapping ? EPlacementInvalidReason::Overlapping : EPlacementInvalidReason::None);
		Target->SetPlacementState((bOutOfBounds || bOverlapping) ? EPlacementState::Invalid : EPlacementState::Preview);
		RefreshPlacementCellViz(Target, bOutOfBounds || bOverlapping);
		return;
	}

	FVector NewLoc = GizmoDragStartLocation;
	if (Axis == TEXT("MoveX"))
	{
		NewLoc.X = CursorOnGround.X;
	}
	else if (Axis == TEXT("MoveY"))
	{
		NewLoc.Y = CursorOnGround.Y;
	}

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

	// AABB ㅼ젣 寃뱀묠 寃(먭린 먯떊 쒖쇅)
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

	if (Target->GetPlacedSurfaceType() == EPlacementSurfaceType::Wall)
	{
		const FVector2D CursorXY(TargetWorldLocation.X, TargetWorldLocation.Y);
		const FVector NewWallLoc = ComputeWallSnappedLocation(Target, CursorXY, GizmoWallSegStart, GizmoWallSegEnd, GizmoDragStartLocation.Z);
		Target->SetActorLocation(NewWallLoc);

		bool bOverlapping = false;
		const FBox TargetBox = Target->GetCollisionBounds().ExpandBy(-1.0f);
		for (AFurniture* Placed : PlacedFurnitures)
		{
			if (!IsValid(Placed) || Placed == Target) continue;
			if (TargetBox.Intersect(Placed->GetCollisionBounds()))
			{
				bOverlapping = true;
				break;
			}
		}

		InvalidReason = bOverlapping ? EPlacementInvalidReason::Overlapping : EPlacementInvalidReason::None;
		Target->SetPlacementState(bOverlapping ? EPlacementState::Invalid : EPlacementState::Preview);
		RefreshPlacementCellViz(Target, bOverlapping);
		return;
	}

	if (Target->GetPlacedSurfaceType() == EPlacementSurfaceType::Ceiling)
	{
		FVector NewLoc = TargetWorldLocation;
		NewLoc.X = FMath::RoundToFloat(NewLoc.X / GridCellSize) * GridCellSize;
		NewLoc.Y = FMath::RoundToFloat(NewLoc.Y / GridCellSize) * GridCellSize;
		NewLoc.Z = GizmoDragStartLocation.Z;
		Target->SetActorLocation(NewLoc);

		bool bOutOfBounds = !IsFurnitureCornersInsideFloor(Target) || FurnitureIntersectsWalls(Target);

		bool bOverlapping = false;
		if (!bOutOfBounds)
		{
			const FBox TargetBox = Target->GetCollisionBounds().ExpandBy(-1.0f);
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

		InvalidReason = bOutOfBounds ? EPlacementInvalidReason::OutOfBounds : (bOverlapping ? EPlacementInvalidReason::Overlapping : EPlacementInvalidReason::None);
		Target->SetPlacementState((bOutOfBounds || bOverlapping) ? EPlacementState::Invalid : EPlacementState::Preview);
		RefreshPlacementCellViz(Target, bOutOfBounds || bOverlapping);
		return;
	}

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

	// AABB ㅼ젣 寃뱀묠 寃(먭린 먯떊 쒖쇅)
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

	bHasPendingGizmoUndoSnapshot = false;
	PendingGizmoUndoSnapshot.Empty();

	if (Target->GetPlacedSurfaceType() == EPlacementSurfaceType::Wall ||
		Target->GetPlacedSurfaceType() == EPlacementSurfaceType::Ceiling)
	{
		Target->SetActorLocation(GizmoDragStartLocation);
		Target->SetPlacementState(EPlacementState::Placed);
		InvalidReason = EPlacementInvalidReason::None;
		ClearPlacementCellViz();
		return;
	}

	// 쒕옒洹쒖옉 ν븳 뺥솗붾뱶 꾩튂濡蹂듭썝 (ㅻ깄 놁쓬)
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

	if (bHasPendingGizmoUndoSnapshot && InvalidReason == EPlacementInvalidReason::None)
	{
		PushUndoSnapshot(PendingGizmoUndoSnapshot);
	}

	bHasPendingGizmoUndoSnapshot = false;
	PendingGizmoUndoSnapshot.Empty();

	if (Target->GetPlacedSurfaceType() == EPlacementSurfaceType::Wall ||
		Target->GetPlacedSurfaceType() == EPlacementSurfaceType::Ceiling)
	{
		if (InvalidReason != EPlacementInvalidReason::None)
		{
			Target->SetActorLocation(GizmoDragStartLocation);
		}

		Target->SetPlacementState(EPlacementState::Placed);
		InvalidReason = EPlacementInvalidReason::None;
		ClearPlacementCellViz();
		return;
	}

	int L = (int)Target->PlacedDimensions.X;
	int B = (int)Target->PlacedDimensions.Y;

	// 諛곗튂 遺덇 곹깭쇰㈃ 쒕옒洹쒖옉 꾩튂濡蹂듦
	FVector2D FinalAnchor = (InvalidReason == EPlacementInvalidReason::None)
		                        ? Target->PlacedGridAnchor
		                        : GizmoDragOriginalAnchor;

	Target->PlacedGridAnchor = FinalAnchor;

	// 洹몃━뺤쨷붾뱶 醫뚰몴濡ㅻ깄
	FVector SnappedLoc = Grid->ToWorldPosition(FVector2D(
		(float)FinalAnchor.X + ((float)L / 2.0f) - 0.5f,
		(float)FinalAnchor.Y + ((float)B / 2.0f) - 0.5f
	));
	SnappedLoc.Z = GetActorLocation().Z;

	Target->SetActorLocation(SnappedLoc);

	// 洹몃━ щ벑濡	for (int i = 0; i < L; i++)
	{
		for (int j = 0; j < B; j++)
		{
			Grid->SetFurniture(FVector2D(FinalAnchor.X + i, FinalAnchor.Y + j), Target);
		}
	}

	Target->SetPlacementState(EPlacementState::Placed);
	InvalidReason = EPlacementInvalidReason::None;
	ClearPlacementCellViz();
}

void AInteriorPlacementManager::ImportPlacedFurnituresJson(const FString& JsonString)
{
	if (!bRestoringHistory)
	{
		RecordUndoSnapshot();
	}
	
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
		// InitializeGrid媛 洹몃━쒕 앹꽦섎㈃GridOrigin(0,0)쇰줈 由ъ뀑		// 留ㅻ땲 ≫꽣 꾩튂瑜湲곕컲쇰줈 먯젏 蹂듭썝
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
}

bool AInteriorPlacementManager::IsEditableSurfaceComponent(const UMeshComponent* MeshComp) const
{
	if (!MeshComp)
	{
		return false;
	}

	return MeshComp->ComponentHasTag(TEXT("EditableWall")) ||
		   MeshComp->ComponentHasTag(TEXT("EditableFloor")) ||
		   MeshComp->ComponentHasTag(TEXT("Floor"));
}

void AInteriorPlacementManager::ExportSurfaceMaterials(TArray<TSharedPtr<FJsonValue>>& OutArray) const
{
	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
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

			const int32 MaterialCount = MeshComp->GetNumMaterials();
			for (int32 SlotIndex = 0; SlotIndex < MaterialCount; ++SlotIndex)
			{
				UMaterialInterface* Material = MeshComp->GetMaterial(SlotIndex);

				TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject());
				Obj->SetStringField(TEXT("componentPath"), MeshComp->GetPathName());
				Obj->SetNumberField(TEXT("materialSlot"), SlotIndex);
				Obj->SetStringField(TEXT("materialPath"), Material ? Material->GetPathName() : FString());

				OutArray.Add(MakeShareable(new FJsonValueObject(Obj)));
			}
		}
	}
}

void AInteriorPlacementManager::ImportSurfaceMaterials(const TArray<TSharedPtr<FJsonValue>>& SurfaceArray)
{
	if (!GetWorld())
	{
		return;
	}

	TMap<FString, UMeshComponent*> ComponentMap;

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
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

		FString ComponentPath;
		FString MaterialPath;
		int32 MaterialSlot = 0;

		if (!Obj->TryGetStringField(TEXT("componentPath"), ComponentPath))
		{
			continue;
		}

		Obj->TryGetNumberField(TEXT("materialSlot"), MaterialSlot);
		Obj->TryGetStringField(TEXT("materialPath"), MaterialPath);

		UMeshComponent** FoundComp = ComponentMap.Find(ComponentPath);
		if (!FoundComp || !IsValid(*FoundComp))
		{
			continue;
		}

		UMaterialInterface* LoadedMaterial = nullptr;
		if (!MaterialPath.IsEmpty())
		{
			LoadedMaterial = Cast<UMaterialInterface>(
				StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MaterialPath)
			);
		}

		(*FoundComp)->SetMaterial(MaterialSlot, LoadedMaterial);
	}
}

FString AInteriorPlacementManager::ExportEditStateJson()
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

void AInteriorPlacementManager::ImportEditStateJson(const FString& JsonString)
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
*/
