#include "PlacementVisualizerActor.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Components/SceneCaptureComponent2D.h"
#include "EngineUtils.h"

APlacementVisualizerActor::APlacementVisualizerActor()
{
	PrimaryActorTick.bCanEverTick = false;

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

void APlacementVisualizerActor::BeginPlay()
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

	// 미니맵 SceneCapture에서 숨김
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TArray<USceneCaptureComponent2D*> Captures;
		It->GetComponents<USceneCaptureComponent2D>(Captures);
		for (USceneCaptureComponent2D* Cap : Captures)
		{
			Cap->HiddenActors.AddUnique(this);
		}
	}

	// Subsystem에 자신을 등록
	if (UInteriorPlacementSubsystem* Subsystem = GetWorld()->GetSubsystem<UInteriorPlacementSubsystem>())
	{
		Subsystem->RegisterVisualizer(this);
	}
}

void APlacementVisualizerActor::SetFloorZ(float Z)
{
	SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, Z));
}

void APlacementVisualizerActor::SetGridVisible(bool bVisible)
{
	if (GridMeshComp && GridMeshComp->GetDynamicMesh() && GridMeshComp->GetDynamicMesh()->GetMeshPtr()->TriangleCount() > 0)
	{
		GridMeshComp->SetVisibility(bVisible);
		GridDecal->SetVisibility(false);
	}
	else
	{
		GridDecal->SetVisibility(bVisible);
	}
}

void APlacementVisualizerActor::RebuildGridMesh(const TArray<FVector2D>& FloorPolygon, float InGridCellSize, float FloorZ)
{
	if (!GridMeshComp || FloorPolygon.Num() < 3)
	{
		return;
	}

	SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, FloorZ + 1.0f));

	FVector2D Centroid(0, 0);
	for (const FVector2D& P : FloorPolygon)
	{
		Centroid += P;
	}
	Centroid /= (float)FloorPolygon.Num();

	UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GridMeshComp);
	UE::Geometry::FDynamicMesh3& Mesh = *DynMesh->GetMeshPtr();

	constexpr float GridZ = 0.0f;
	int32 CenterIdx = Mesh.AppendVertex(FVector3d(Centroid.X, Centroid.Y, GridZ));

	TArray<int32> VertIds;
	for (const FVector2D& P : FloorPolygon)
	{
		VertIds.Add(Mesh.AppendVertex(FVector3d(P.X, P.Y, GridZ)));
	}

	double SignedArea = 0.0;
	const int32 N = FloorPolygon.Num();
	for (int32 i = 0; i < N; i++)
	{
		FVector2D Pi = FloorPolygon[i];
		FVector2D Pj = FloorPolygon[(i + 1) % N];
		SignedArea += Pi.X * Pj.Y - Pj.X * Pi.Y;
	}
	const bool bCCW = SignedArea > 0.0;

	for (int32 i = 0; i < N; i++)
	{
		int32 Va = VertIds[i];
		int32 Vb = VertIds[(i + 1) % N];
		Mesh.AppendTriangle(bCCW ? CenterIdx : Va, bCCW ? Vb : CenterIdx, bCCW ? Va : Vb);
	}

	GridMeshComp->SetDynamicMesh(DynMesh);
	GridMeshComp->SetWorldLocation(FVector(0.0f, 0.0f, FloorZ + 1.0f));

	if (GridMaterial)
	{
		GridDynMat = UMaterialInstanceDynamic::Create(GridMaterial, this);
		GridDynMat->SetScalarParameterValue(TEXT("CellSize"), InGridCellSize);
		GridMeshComp->SetMaterial(0, GridDynMat);
	}
}

void APlacementVisualizerActor::RefreshPlacementCellViz(const FBox& FurnitureBounds, bool bInvalid, float ManagerZ)
{
	if (!PlacementVizValid || !PlacementVizInvalid)
	{
		return;
	}

	const float X0 = FurnitureBounds.Min.X - GetActorLocation().X;
	const float Y0 = FurnitureBounds.Min.Y - GetActorLocation().Y;
	const float X1 = FurnitureBounds.Max.X - GetActorLocation().X;
	const float Y1 = FurnitureBounds.Max.Y - GetActorLocation().Y;
	constexpr float LocalZ = 0.5f;

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

void APlacementVisualizerActor::ClearPlacementCellViz()
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
