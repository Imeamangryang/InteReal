#include "CeilingPlacementHandler.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "InteReal/EditMode/Furniture/Furniture.h"
#include "InteReal/EditMode/Managers/GridSpaceManager.h"
#include "InteReal/EditMode/Visualization/PlacementVisualizerActor.h"

FVector UCeilingPlacementHandler::SnapXYToGrid(const FVector& WorldLocation) const
{
	FVector SnappedLocation = WorldLocation;
	if (Subsystem)
	{
		if (AGridSpaceManager* Grid = Subsystem->GetGrid())
		{
			const FVector2D GridPos = Grid->ToGridPosition(WorldLocation);
			const FVector WorldXY = Grid->ToWorldPosition(GridPos);
			SnappedLocation.X = WorldXY.X;
			SnappedLocation.Y = WorldXY.Y;
		}
	}
	return SnappedLocation;
}

float UCeilingPlacementHandler::ResolveCeilingPlaneZ(const FHitResult& Hit, const FVector& SnappedLocation, AFurniture* IgnoredFurniture) const
{
	if (!Subsystem)
	{
		return Hit.ImpactPoint.Z;
	}

	const UPrimitiveComponent* HitComp = Hit.GetComponent();
	if ((HitComp && HitComp->ComponentHasTag(TEXT("Ceiling"))) || Hit.ImpactNormal.Z < -0.5f)
	{
		return Hit.ImpactPoint.Z;
	}

	FHitResult CeilingHit;
	FCollisionQueryParams Params(NAME_None, true);
	if (IgnoredFurniture)
	{
		Params.AddIgnoredActor(IgnoredFurniture);
	}

	const FVector TraceStart(SnappedLocation.X, SnappedLocation.Y, Hit.ImpactPoint.Z + 10.0f);
	const FVector TraceEnd(SnappedLocation.X, SnappedLocation.Y, Hit.ImpactPoint.Z + 100000.0f);
	if (Subsystem->GetWorld()->LineTraceSingleByChannel(CeilingHit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		const UPrimitiveComponent* CeilingComp = CeilingHit.GetComponent();
		if (CeilingComp && CeilingComp->ComponentHasTag(TEXT("Ceiling")))
		{
			return CeilingHit.ImpactPoint.Z;
		}
	}

	return Hit.ImpactPoint.Z;
}

void UCeilingPlacementHandler::AlignFurnitureTopToCeiling(AFurniture* Furniture, float CeilingPlaneZ) const
{
	if (!Furniture)
	{
		return;
	}

	const FBox VisualBounds = Furniture->GetVisualBounds();
	const float DeltaZ = CeilingPlaneZ - VisualBounds.Max.Z;
	if (!FMath::IsNearlyZero(DeltaZ))
	{
		FVector AdjustedLocation = Furniture->GetActorLocation();
		AdjustedLocation.Z += DeltaZ;
		Furniture->SetActorLocation(AdjustedLocation);
	}
}

void UCeilingPlacementHandler::Initialize(UInteriorPlacementSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
}

bool UCeilingPlacementHandler::CanHandle(const FHitResult& Hit) const
{
	const AFurniture* Preview = Subsystem ? Subsystem->GetPreviewFurniture() : nullptr;
	if (!Preview || !Preview->SupportsPlacementType(EPlacementSurfaceType::Ceiling))
	{
		return false;
	}

	const UPrimitiveComponent* Comp = Hit.GetComponent();
	if (Comp && Comp->ComponentHasTag(TEXT("Ceiling")))
	{
		return true;
	}
	return Hit.ImpactNormal.Z < -0.5f;
}

bool UCeilingPlacementHandler::OwnsFurniture(const AFurniture* Furniture) const
{
	if (!Furniture)
	{
		return false;
	}
	return Furniture->GetPlacedSurfaceType() == EPlacementSurfaceType::Ceiling;
}

void UCeilingPlacementHandler::UpdatePreview(AFurniture* Preview, const FHitResult& Hit)
{
	if (!Preview || !Subsystem)
	{
		return;
	}

	// 사용자 Yaw 회전 적용 (Roll 강제 없음 — 에셋이 이미 올바른 방향으로 설정됨)
	Preview->SetActorRotation(Subsystem->GetPreviewRotation());

	FVector SnappedLocation = SnapXYToGrid(Hit.ImpactPoint);
	const float CeilingPlaneZ = ResolveCeilingPlaneZ(Hit, SnappedLocation, Preview);

	SnappedLocation.Z = CeilingPlaneZ;
	Preview->SetActorLocation(SnappedLocation);
	AlignFurnitureTopToCeiling(Preview, CeilingPlaneZ);
	Subsystem->SetLastRayPosition(Preview->GetActorLocation());

	bool bValid = true;
	const FBox Bounds = Preview->GetVisualBounds().ExpandBy(-1.0f);
	for (const AFurniture* Placed : Subsystem->GetPlacedFurnitures())
	{
		if (!IsValid(Placed) || Placed == Preview)
		{
			continue;
		}
		if (Placed->GetPlacedSurfaceType() != EPlacementSurfaceType::Ceiling)
		{
			continue;
		}
		if (Bounds.Intersect(Placed->GetVisualBounds()))
		{
			bValid = false;
			Subsystem->SetInvalidReason(EPlacementInvalidReason::Overlapping);
			break;
		}
	}

	Subsystem->SetInvalidReason(bValid ? EPlacementInvalidReason::None : EPlacementInvalidReason::Overlapping);
	Preview->SetPlacementState(bValid ? EPlacementState::Preview : EPlacementState::Invalid);

	// 천장 가구는 Visualizer가 바닥(FloorZ)에 있으므로 바닥 기준 viz를 사용하지 않음
	// 가구 자체 머티리얼 상태(Preview/Invalid)로 시각 피드백을 대신함
	if (APlacementVisualizerActor* Visualizer = Subsystem->GetVisualizer())
	{
		Visualizer->ClearPlacementCellViz();
	}
}

void UCeilingPlacementHandler::OnConfirm(AFurniture* Furniture)
{
	if (!Furniture)
	{
		return;
	}
	Furniture->SetPlacedSurfaceType(EPlacementSurfaceType::Ceiling);
}

void UCeilingPlacementHandler::OnRemove(AFurniture* Furniture)
{
}

// 기즈모
void UCeilingPlacementHandler::BeginGizmoMove(AFurniture* Target)
{
	if (Target)
	{
		GizmoDragStartLocation = Target->GetActorLocation();
		GizmoDragCeilingZ = Target->GetVisualBounds().Max.Z;
	}
}

void UCeilingPlacementHandler::UpdateGizmoMove(AFurniture* Target, FVector Cursor, EGizmoTransformAxis Axis)
{
	if (!Target)
	{
		return;
	}
	FVector NewLoc = Target->GetActorLocation();
	if (Axis == EGizmoTransformAxis::MoveX)
	{
		NewLoc.X = Cursor.X;
	}
	else if (Axis == EGizmoTransformAxis::MoveY)
	{
		NewLoc.Y = Cursor.Y;
	}
	else
	{
		NewLoc.X = Cursor.X;
		NewLoc.Y = Cursor.Y;
	}

	NewLoc = SnapXYToGrid(NewLoc);
	NewLoc.Z = GizmoDragCeilingZ;
	Target->SetActorLocation(NewLoc);
	AlignFurnitureTopToCeiling(Target, GizmoDragCeilingZ);

	if (Subsystem)
	{
		if (APlacementVisualizerActor* Visualizer = Subsystem->GetVisualizer())
		{
			Visualizer->ClearPlacementCellViz();
		}
	}
}

void UCeilingPlacementHandler::UpdateGizmoMoveFree(AFurniture* Target, FVector TargetLoc)
{
	if (!Target)
	{
		return;
	}
	FVector SnappedLocation = SnapXYToGrid(TargetLoc);
	SnappedLocation.Z = GizmoDragCeilingZ;
	Target->SetActorLocation(SnappedLocation);
	AlignFurnitureTopToCeiling(Target, GizmoDragCeilingZ);
}

void UCeilingPlacementHandler::FinalizeGizmoMove(AFurniture* Target)
{
	if (Subsystem)
	{
		if (APlacementVisualizerActor* Visualizer = Subsystem->GetVisualizer())
		{
			Visualizer->ClearPlacementCellViz();
		}
	}
}

void UCeilingPlacementHandler::AbortGizmoMove(AFurniture* Target)
{
	if (Target)
	{
		Target->SetActorLocation(GizmoDragStartLocation);
	}
	if (Subsystem)
	{
		if (APlacementVisualizerActor* Visualizer = Subsystem->GetVisualizer())
		{
			Visualizer->ClearPlacementCellViz();
		}
	}
}
