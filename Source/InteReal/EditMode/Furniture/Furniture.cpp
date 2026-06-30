#include "Furniture.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "PhysicsEngine/BodySetup.h"
#include "InteReal/EditMode/Gizmo/InteRealGizmoComponent.h"
#include "InteReal/EditMode/Managers/GridSpaceManager.h"
#include "LightFixture.h"
#include "PhysicsEngine/AggregateGeom.h"

static void UpdatePostProcessOutlineColor(UWorld* World, FLinearColor Color, float Thickness)
{
	if (!World) return;
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		for (FWeightedBlendable& WB : It->Settings.WeightedBlendables.Array)
		{
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(WB.Object);
			if (!MID)
			{
				if (UMaterialInterface* MI = Cast<UMaterialInterface>(WB.Object))
				{
					MID = UMaterialInstanceDynamic::Create(MI, *It);
					WB.Object = MID;
				}
			}
			if (MID)
			{
				MID->SetVectorParameterValue(TEXT("OutlineColor"), Color);
				MID->SetScalarParameterValue(TEXT("OutlineThickness"), Thickness);
			}
		}
		break; // 첫 번째 PostProcessVolume만 적용
	}
}

void AFurniture::SetMeshesCustomDepth(bool bEnabled, int32 StencilValue)
{
	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (GizmoComponent && GizmoComponent->OwnsGizmoComponent(Mesh))
		{
			continue;
		}
		Mesh->SetRenderCustomDepth(bEnabled);
		Mesh->SetCustomDepthStencilValue(StencilValue);
	}
}

void AFurniture::SetMeshesVisibilityCollision(ECollisionResponse Response)
{
	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (GizmoComponent && GizmoComponent->OwnsGizmoComponent(Mesh))
		{
			continue;
		}
		Mesh->SetCollisionResponseToChannel(ECC_Visibility, Response);
	}
}

FBox AFurniture::GetVisualBounds() const
{
	FBox Bounds(EForceInit::ForceInit);
	TArray<UPrimitiveComponent*> Components;
	GetComponents<UPrimitiveComponent>(Components);
	for (const UPrimitiveComponent* Component : Components)
	{
		if (!Component || (GizmoComponent && GizmoComponent->OwnsGizmoComponent(Component)))
		{
			continue;
		}
		Bounds += Component->Bounds.GetBox();
	}
	return Bounds.IsValid ? Bounds : GetComponentsBoundingBox(true);
}

static void ApplyFurniturePawnCollision(AFurniture* Furniture)
{
	if (!Furniture)
	{
		return;
	}

	const bool bBlocksPawn = Furniture->ActorHasTag(TEXT("WindowAsset"));
	TArray<UPrimitiveComponent*> Components;
	Furniture->GetComponents<UPrimitiveComponent>(Components);
	for (UPrimitiveComponent* Component : Components)
	{
		if (!Component)
		{
			continue;
		}
		Component->SetCollisionObjectType(bBlocksPawn ? ECC_WorldStatic : ECC_WorldDynamic);
		Component->SetCollisionResponseToChannel(ECC_Pawn, bBlocksPawn ? ECR_Block : ECR_Ignore);
	}
}

AFurniture::AFurniture()
{
	PrimaryActorTick.bCanEverTick = false;
	PlacementState = EPlacementState::Preview;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;
	MeshComponent->bReceivesDecals = false;
	MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	MeshComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);

	CollisionBoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBoxComponent->SetupAttachment(MeshComponent);
	CollisionBoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionBoxComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionBoxComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionBoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CollisionBoxComponent->SetHiddenInGame(true);

	GizmoComponent = CreateDefaultSubobject<UInteRealGizmoComponent>(TEXT("GizmoComponent"));
	GizmoComponent->SetupAttachment(MeshComponent);
	
	LightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("LightComponent"));
	LightComponent->SetupAttachment(MeshComponent);
	LightComponent->SetVisibility(false);
	LightComponent->IntensityUnits = ELightUnits::Candelas;
}

void AFurniture::SetPlacementState(EPlacementState NewState)
{
	PlacementState = NewState;

	if (CollisionBoxComponent)
	{
		switch (NewState)
		{
		case EPlacementState::Preview:
			CollisionBoxComponent->SetHiddenInGame(false);
			CollisionBoxComponent->ShapeColor = FLinearColor(0.225781f, 0.420849f, 0.850000f, 0.5f).ToFColor(true);
			CollisionBoxComponent->MarkRenderStateDirty();
			CollisionBoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
			SetMeshesVisibilityCollision(ECR_Ignore);
			SetMeshesCustomDepth(true, 1);
			UpdatePostProcessOutlineColor(GetWorld(), FLinearColor(0.348958f, 0.552408f, 0.800000f, 0.5f), PreviewOutlineThickness);
			break;

		case EPlacementState::Invalid:
			CollisionBoxComponent->SetHiddenInGame(false);
			CollisionBoxComponent->ShapeColor = FLinearColor(0.850000f, 0.188151f, 0.222533f, 0.5f).ToFColor(true);
			CollisionBoxComponent->MarkRenderStateDirty();
			CollisionBoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
			SetMeshesVisibilityCollision(ECR_Ignore);
			SetMeshesCustomDepth(true, 1);
			UpdatePostProcessOutlineColor(GetWorld(), FLinearColor(0.800000f, 0.312500f, 0.348214f, 0.5f), PreviewOutlineThickness);
			break;

		case EPlacementState::Placed:
			CollisionBoxComponent->SetHiddenInGame(true);
			CollisionBoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			SetMeshesVisibilityCollision(ECR_Block);
			ApplyFurniturePawnCollision(this);
			SetMeshesCustomDepth(false, 0);
			UpdatePostProcessOutlineColor(GetWorld(), FLinearColor(1.f, 1.f, 1.f, 1.f), PlacedOutlineThickness);
			break;
		}
	}
}

void AFurniture::SetSelected(bool bSelected)
{
	
	UE_LOG(LogTemp, Warning, TEXT("[Furniture::SetSelected][Before] Furniture=%s bSelected=%d Mesh=%s CollisionEnabled=%d VisibilityResponse=%d"),
		*GetNameSafe(this),
		bSelected,
		*GetNameSafe(MeshComponent),
		MeshComponent ? static_cast<int32>(MeshComponent->GetCollisionEnabled()) : -1,
		MeshComponent ? static_cast<int32>(MeshComponent->GetCollisionResponseToChannel(ECC_Visibility)) : -1);

	SetMeshesCustomDepth(bSelected, 1);
	CollisionBoxComponent->SetHiddenInGame(true);
	
	if (bSelected)
	{
		UpdatePostProcessOutlineColor(GetWorld(), FLinearColor(1.f, 1.f, 1.f, 1.f), PlacedOutlineThickness);
	}
	
	/*if (CollisionBoxComponent)
	{
		CollisionBoxComponent->SetHiddenInGame(!bSelected);
		CollisionBoxComponent->ShapeColor = FColor::Orange;
		// CollisionBoxComponent->LineThickness = 0.5f;
		CollisionBoxComponent->MarkRenderStateDirty();
	}*/
	
	UE_LOG(LogTemp, Warning, TEXT("[Furniture::SetSelected][After] Furniture=%s bSelected=%d Mesh=%s CollisionEnabled=%d VisibilityResponse=%d"),
		*GetNameSafe(this),
		bSelected,
		*GetNameSafe(MeshComponent),
		MeshComponent ? static_cast<int32>(MeshComponent->GetCollisionEnabled()) : -1,
		MeshComponent ? static_cast<int32>(MeshComponent->GetCollisionResponseToChannel(ECC_Visibility)) : -1);
}

void AFurniture::ApplyFurnitureRow(const FFurnitureDataRow& InFurnitureRow)
{
	FurnitureDataRow = InFurnitureRow;
	bHasFurnitureDataRow = true;
	
	FurnitureID = InFurnitureRow.ID;
	AllowedPlacementTypes = InFurnitureRow.AllowedPlacementTypes;

	if (InFurnitureRow.FurnitureMesh)
	{
		MeshComponent->SetStaticMesh(InFurnitureRow.FurnitureMesh);

		const FBoxSphereBounds MeshBounds = InFurnitureRow.FurnitureMesh->GetBounds();
		
		const FVector NativeSize = MeshBounds.GetBox().GetSize();
		const FVector TargetSizeCm(InFurnitureRow.Width, InFurnitureRow.Depth, InFurnitureRow.Height);
		MeshComponent->SetRelativeScale3D(ComputeSizeScale(NativeSize, TargetSizeCm));

		FBox PlacementBounds = MeshBounds.GetBox();
		bool bUsesSimpleCollision = false;
		if (const UBodySetup* BodySetup = InFurnitureRow.FurnitureMesh->GetBodySetup())
		{
			// Authored simple collision is a more reliable placement footprint than render bounds.
			if (BodySetup->AggGeom.GetElementCount() > 0)
			{
				const FBox CollisionBounds = BodySetup->AggGeom.CalcAABB(FTransform::Identity);
				if (CollisionBounds.IsValid && CollisionBounds.GetSize().X > KINDA_SMALL_NUMBER &&
					CollisionBounds.GetSize().Y > KINDA_SMALL_NUMBER)
				{
					PlacementBounds = CollisionBounds;
					bUsesSimpleCollision = true;
				}
			}
		}

		FVector Extent = PlacementBounds.GetExtent();
		PlacementLocalBounds = PlacementBounds;
		Extent.Z = 2.0f;
		CollisionBoxComponent->SetBoxExtent(Extent);
		
		const bool bCeilingOnly = SupportsPlacementType(EPlacementSurfaceType::Ceiling)
		                       && !SupportsPlacementType(EPlacementSurfaceType::Floor);
		const float IndicatorZ = bCeilingOnly ? PlacementBounds.Max.Z : PlacementBounds.Min.Z;
		CollisionBoxComponent->SetRelativeLocation(FVector(
			PlacementBounds.GetCenter().X, PlacementBounds.GetCenter().Y, IndicatorZ));
		CollisionBoxComponent->UpdateBounds();

		UE_LOG(LogTemp, Log,
			TEXT("[Placement] Furniture %d mesh=%s renderSize=%s placementSize=%s source=%s"),
			FurnitureID, *GetNameSafe(InFurnitureRow.FurnitureMesh),
			*MeshBounds.GetBox().GetSize().ToCompactString(),
			*PlacementBounds.GetSize().ToCompactString(),
			bUsesSimpleCollision ? TEXT("SimpleCollision") : TEXT("RenderBounds"));
		
		/*CollisionBoxComponent->SetBoxExtent(MeshBounds.BoxExtent);
		CollisionBoxComponent->SetRelativeLocation(MeshBounds.Origin);*/
	}

	SetPlacementState(EPlacementState::Preview);
}

FVector AFurniture::ComputeSizeScale(const FVector& NativeSize, const FVector& TargetSizeCm)
{
	FVector Scale(1.0f, 1.0f, 1.0f);
	if (TargetSizeCm.X > KINDA_SMALL_NUMBER && NativeSize.X > KINDA_SMALL_NUMBER)
	{
		Scale.X = TargetSizeCm.X / NativeSize.X;
	}
	if (TargetSizeCm.Y > KINDA_SMALL_NUMBER && NativeSize.Y > KINDA_SMALL_NUMBER)
	{
		Scale.Y = TargetSizeCm.Y / NativeSize.Y;
	}
	if (TargetSizeCm.Z > KINDA_SMALL_NUMBER && NativeSize.Z > KINDA_SMALL_NUMBER)
	{
		Scale.Z = TargetSizeCm.Z / NativeSize.Z;
	}
	return Scale;
}

FVector AFurniture::GetCurrentSizeCm() const
{
	const UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
	if (!Mesh)
	{
		return FVector::ZeroVector;
	}

	const FVector NativeSize = Mesh->GetBounds().GetBox().GetSize();
	const FVector Scale = MeshComponent->GetRelativeScale3D();
	return FVector(NativeSize.X * Scale.X, NativeSize.Y * Scale.Y, NativeSize.Z * Scale.Z);
}

void AFurniture::SetTargetSizeCm(FVector InSizeCm)
{
	const UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}

	const FVector NativeSize = Mesh->GetBounds().GetBox().GetSize();

	// 바닥(또는 닿아있는 면)에 고정된 위치를 유지하기 위해 스케일 적용 전 바운즈를 기록해둔다.
	const FBox PrevBounds = GetPlacementGeometryBounds();
	const FVector PrevCenter = PrevBounds.GetCenter();
	const float PrevMinZ = PrevBounds.Min.Z;

	MeshComponent->SetRelativeScale3D(ComputeSizeScale(NativeSize, InSizeCm));

	AlignPlacementBottomCenterTo(FVector(PrevCenter.X, PrevCenter.Y, PrevMinZ), PrevMinZ);
}

TSubclassOf<AFurniture> AFurniture::ResolveSpawnClass(const FFurnitureDataRow& Row, TSubclassOf<AFurniture> DefaultClass,
	TSubclassOf<ALightFixture> LightFixtureClassOverride)
{
	if (Row.Category == EFurnitureAssetCategory::Lighting)
	{
		return LightFixtureClassOverride ? TSubclassOf<AFurniture>(LightFixtureClassOverride) : TSubclassOf<AFurniture>(ALightFixture::StaticClass());
	}
	return DefaultClass;
}

void AFurniture::GetOccupiedGridCells(const AGridSpaceManager* Grid, FVector2D Anchor,
	FVector2D Dimensions, TArray<FIntPoint>& OutCells) const
{
	OutCells.Reset();
	if (!Grid)
	{
		return;
	}

	const int32 L = FMath::Max(1, FMath::RoundToInt(Dimensions.X));
	const int32 B = FMath::Max(1, FMath::RoundToInt(Dimensions.Y));
	const UStaticMesh* StaticMesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
	const UBodySetup* BodySetup = StaticMesh ? StaticMesh->GetBodySetup() : nullptr;
	const bool bHasSimpleCollision = BodySetup && BodySetup->AggGeom.GetElementCount() > 0;
	const float CellHalfExtent = Grid->GetCellSize() * 0.48f;
	const FBox Bounds = GetPlacementGeometryBounds();
	const float HalfHeight = FMath::Max(1.0f, Bounds.GetExtent().Z - 1.0f);

	for (int32 X = 0; X < L; ++X)
	{
		for (int32 Y = 0; Y < B; ++Y)
		{
			const FIntPoint Cell(FMath::RoundToInt(Anchor.X) + X, FMath::RoundToInt(Anchor.Y) + Y);
			bool bOccupied = true;
			if (bHasSimpleCollision && MeshComponent)
			{
				FVector CellCenter = Grid->ToWorldPosition(FVector2D(Cell.X, Cell.Y));
				CellCenter.Z = Bounds.GetCenter().Z;
				bOccupied = MeshComponent->OverlapComponent(
					CellCenter, FQuat::Identity,
					FCollisionShape::MakeBox(FVector(CellHalfExtent, CellHalfExtent, HalfHeight)));
			}

			if (bOccupied)
			{
				OutCells.Add(Cell);
			}
		}
	}

	// A missing physics body must not make furniture occupy no cells at all.
	if (OutCells.IsEmpty())
	{
		for (int32 X = 0; X < L; ++X)
		{
			for (int32 Y = 0; Y < B; ++Y)
			{
				OutCells.Emplace(FMath::RoundToInt(Anchor.X) + X, FMath::RoundToInt(Anchor.Y) + Y);
			}
		}
	}
}

void AFurniture::AlignMeshBottomToZ(float SurfaceZ)
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh())
	{
		return;
	}

	const float DeltaZ = SurfaceZ - MeshComponent->Bounds.GetBox().Min.Z;
	if (!FMath::IsNearlyZero(DeltaZ))
	{
		AddActorWorldOffset(FVector(0.0f, 0.0f, DeltaZ));
	}
}

void AFurniture::AlignMeshBottomCenterTo(const FVector& TargetCenter, float SurfaceZ)
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh())
	{
		return;
	}

	const FBox MeshBounds = GetMeshBounds();
	const FVector MeshCenter = MeshBounds.GetCenter();
	AddActorWorldOffset(FVector(
		TargetCenter.X - MeshCenter.X,
		TargetCenter.Y - MeshCenter.Y,
		SurfaceZ - MeshBounds.Min.Z));
}

void AFurniture::AlignPlacementBottomCenterTo(const FVector& TargetCenter, float SurfaceZ)
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh())
	{
		return;
	}

	const FBox Bounds = GetPlacementGeometryBounds();
	const FVector Center = Bounds.GetCenter();
	AddActorWorldOffset(FVector(
		TargetCenter.X - Center.X,
		TargetCenter.Y - Center.Y,
		SurfaceZ - Bounds.Min.Z));
}

void AFurniture::SetRotationPreservingPlacement(const FRotator& NewRotation)
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh())
	{
		SetActorRotation(NewRotation);
		return;
	}

	const FBox Before = GetMeshBounds();
	SetActorRotation(NewRotation);
	const FBox After = GetMeshBounds();

	FVector Offset(
		Before.GetCenter().X - After.GetCenter().X,
		Before.GetCenter().Y - After.GetCenter().Y,
		Before.GetCenter().Z - After.GetCenter().Z);

	if (PlacedSurfaceType == EPlacementSurfaceType::Floor ||
		PlacedSurfaceType == EPlacementSurfaceType::Surface)
	{
		Offset.Z = Before.Min.Z - After.Min.Z;
	}
	else if (PlacedSurfaceType == EPlacementSurfaceType::Ceiling)
	{
		Offset.Z = Before.Max.Z - After.Max.Z;
	}

	AddActorWorldOffset(Offset);
}

static void AddBoxFootprintPoints(const FKBoxElem& BoxElem, TArray<FVector2D>& OutPoints)
{
	const FVector Center = BoxElem.Center;
	const FVector Extent(BoxElem.X * 0.5f, BoxElem.Y * 0.5f, BoxElem.Z * 0.5f);
	const FQuat Rotation = BoxElem.Rotation.Quaternion();

	const FVector LocalCorners[4] = {
		FVector(-Extent.X, -Extent.Y, 0.0f),
		FVector(Extent.X, -Extent.Y, 0.0f),
		FVector(Extent.X, Extent.Y, 0.0f),
		FVector(-Extent.X, Extent.Y, 0.0f)
	};

	for (const FVector& Corner : LocalCorners)
	{
		const FVector P = Center + Rotation.RotateVector(Corner);
		OutPoints.Add(FVector2D(P.X, P.Y));
	}
}

static void AddSphereFootprintPoints(const FKSphereElem& SphereElem, TArray<FVector2D>& OutPoints)
{
	constexpr int32 SegmentCount = 20;
	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		const float Angle = 2.0f * PI * static_cast<float>(Index) / static_cast<float>(SegmentCount);
		OutPoints.Add(FVector2D(SphereElem.Center.X + FMath::Cos(Angle) * SphereElem.Radius, SphereElem.Center.Y + FMath::Sin(Angle) * SphereElem.Radius));
	}
}

static void AddConvexFootprintPoints(const FKConvexElem& ConvexElem, TArray<FVector2D>& OutPoints)
{
	for (const FVector& Vertex : ConvexElem.VertexData)
	{
		OutPoints.Add(FVector2D(Vertex.X, Vertex.Y));
	}
}

void AFurniture::GetCollisionFootprint2D(TArray<FVector2D>& OutLocalPoints) const
{
	OutLocalPoints.Reset();

	const UStaticMesh* StaticMesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
	const UBodySetup* BodySetup = StaticMesh ? StaticMesh->GetBodySetup() : nullptr;
	if (!BodySetup || BodySetup->AggGeom.GetElementCount() <= 0)
	{
		const FBox Bounds = PlacementLocalBounds.IsValid ? PlacementLocalBounds : StaticMesh ? StaticMesh->GetBounds().GetBox() : FBox(EForceInit::ForceInit);
		if (Bounds.IsValid)
		{
			const FVector2D HalfSize(Bounds.GetExtent().X, Bounds.GetExtent().Y);
			OutLocalPoints.Add(FVector2D(-HalfSize.X, -HalfSize.Y));
			OutLocalPoints.Add(FVector2D(HalfSize.X, -HalfSize.Y));
			OutLocalPoints.Add(FVector2D(HalfSize.X, HalfSize.Y));
			OutLocalPoints.Add(FVector2D(-HalfSize.X, HalfSize.Y));
		}
		return;
	}

	for (const FKBoxElem& BoxElem : BodySetup->AggGeom.BoxElems)
	{
		AddBoxFootprintPoints(BoxElem, OutLocalPoints);
	}

	for (const FKSphereElem& SphereElem : BodySetup->AggGeom.SphereElems)
	{
		AddSphereFootprintPoints(SphereElem, OutLocalPoints);
	}

	for (const FKConvexElem& ConvexElem : BodySetup->AggGeom.ConvexElems)
	{
		AddConvexFootprintPoints(ConvexElem, OutLocalPoints);
	}

	if (OutLocalPoints.Num() == 0)
	{
		const FBox Bounds = PlacementLocalBounds.IsValid ? PlacementLocalBounds : StaticMesh->GetBounds().GetBox();
		const FVector2D HalfSize(Bounds.GetExtent().X, Bounds.GetExtent().Y);
		OutLocalPoints.Add(FVector2D(-HalfSize.X, -HalfSize.Y));
		OutLocalPoints.Add(FVector2D(HalfSize.X, -HalfSize.Y));
		OutLocalPoints.Add(FVector2D(HalfSize.X, HalfSize.Y));
		OutLocalPoints.Add(FVector2D(-HalfSize.X, HalfSize.Y));
	}
}
