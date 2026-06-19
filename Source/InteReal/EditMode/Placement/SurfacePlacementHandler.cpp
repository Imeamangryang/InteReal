#include "SurfacePlacementHandler.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "InteReal/EditMode/Furniture/Furniture.h"
#include "InteReal/EditMode/Managers/GridSpaceManager.h"

void USurfacePlacementHandler::Initialize(UInteriorPlacementSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
}

bool USurfacePlacementHandler::CanHandle(const FHitResult& Hit) const
{
	const AFurniture* HitFurniture = Cast<AFurniture>(Hit.GetActor());
	return HitFurniture && HitFurniture->GetPlacementState() == EPlacementState::Placed;
}

bool USurfacePlacementHandler::OwnsFurniture(const AFurniture* Furniture) const
{
	if (!Furniture)
	{
		return false;
	}
	return Furniture->GetPlacedSurfaceType() == EPlacementSurfaceType::Surface;
}

void USurfacePlacementHandler::UpdatePreview(AFurniture* Preview, const FHitResult& Hit)
{
	if (!Preview || !Subsystem)
	{
		return;
	}

	AFurniture* HitFurniture = Cast<AFurniture>(Hit.GetActor());
	if (!HitFurniture || HitFurniture->GetPlacementState() != EPlacementState::Placed)
	{
		Preview->SetPlacementState(EPlacementState::Invalid);
		Subsystem->SetInvalidReason(EPlacementInvalidReason::UnsupportedSurface);
		CurrentSurfaceParent = nullptr;
		return;
	}

	CurrentSurfaceParent = HitFurniture;

	FVector SnappedImpact = Hit.ImpactPoint;
	if (AGridSpaceManager* Grid = Subsystem->GetGrid())
	{
		const FVector2D GridPos = Grid->ToGridPosition(Hit.ImpactPoint);
		const FVector WorldXY = Grid->ToWorldPosition(GridPos);
		SnappedImpact.X = WorldXY.X;
		SnappedImpact.Y = WorldXY.Y;
	}

	// 가구 상단 Z 찾기 (아래방향 라인트레이스)
	float TopZ = SnappedImpact.Z;
	{
		FHitResult TopHit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(Preview);
		if (Subsystem->GetWorld()->LineTraceSingleByChannel(TopHit,
			FVector(SnappedImpact.X, SnappedImpact.Y, HitFurniture->GetActorLocation().Z + 500.0f),
			FVector(SnappedImpact.X, SnappedImpact.Y, HitFurniture->GetActorLocation().Z - 500.0f),
			ECC_Visibility, Params))
		{
			if (Cast<AFurniture>(TopHit.GetActor()) == HitFurniture)
			{
				TopZ = TopHit.ImpactPoint.Z;
			}
		}
	}

	Preview->SetActorLocation(FVector(SnappedImpact.X, SnappedImpact.Y, TopZ));

	// 부모 가구 바운드 안에 있는지 체크 (AABB 기준)
	const FBox ParentBounds  = HitFurniture->GetCollisionBounds();
	const FBox PreviewBounds = Preview->GetCollisionBounds();
	const bool bWithinParent =
		PreviewBounds.Min.X >= ParentBounds.Min.X &&
		PreviewBounds.Max.X <= ParentBounds.Max.X &&
		PreviewBounds.Min.Y >= ParentBounds.Min.Y &&
		PreviewBounds.Max.Y <= ParentBounds.Max.Y;

	// 다른 Surface 배치 가구와 겹침 체크
	bool bOverlaps = false;
	if (bWithinParent)
	{
		bOverlaps = Subsystem->IsOverlappingPlacedFurniture(Preview, HitFurniture, HitFurniture);
	}

	const bool bValid = bWithinParent && !bOverlaps;
	Subsystem->SetInvalidReason(bValid ? EPlacementInvalidReason::None : (bWithinParent ? EPlacementInvalidReason::Overlapping : EPlacementInvalidReason::OutsideFloor));

	Preview->SetPlacementState(bValid ? EPlacementState::Preview : EPlacementState::Invalid);
}

void USurfacePlacementHandler::OnConfirm(AFurniture* Furniture)
{
	if (!Furniture || !CurrentSurfaceParent)
	{
		return;
	}

	Furniture->SetPlacedSurfaceType(EPlacementSurfaceType::Surface);
	Furniture->ParentFurniture = CurrentSurfaceParent;
	Furniture->AttachToActor(CurrentSurfaceParent, FAttachmentTransformRules::KeepWorldTransform);
	CurrentSurfaceParent = nullptr;
}

void USurfacePlacementHandler::OnRemove(AFurniture* Furniture)
{
	if (!Furniture)
	{
		return;
	}
	Furniture->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Furniture->ParentFurniture = nullptr;
}

void USurfacePlacementHandler::BeginGizmoMove(AFurniture* Target)
{
	if (Target)
	{
		GizmoDragStartLocation = Target->GetActorLocation();
		CurrentSurfaceParent = Target->ParentFurniture;
	}
}

void USurfacePlacementHandler::UpdateGizmoMove(AFurniture* Target, FVector Cursor, EGizmoTransformAxis Axis)
{
	if (!Target || !Subsystem || !CurrentSurfaceParent)
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
	else if (Axis == EGizmoTransformAxis::MoveZ)
	{
		NewLoc.Z = FMath::Max(Cursor.Z, CurrentSurfaceParent->GetCollisionBounds().Max.Z);
	}
	else
	{
		NewLoc.X = Cursor.X;
		NewLoc.Y = Cursor.Y;
	}

	if (Axis != EGizmoTransformAxis::MoveZ)
	{
		if (AGridSpaceManager* Grid = Subsystem->GetGrid())
		{
			const FVector2D GridPos = Grid->ToGridPosition(NewLoc);
			const FVector WorldXY = Grid->ToWorldPosition(GridPos);
			NewLoc.X = WorldXY.X;
			NewLoc.Y = WorldXY.Y;
		}
	}

	Target->SetActorLocation(NewLoc);

	const FBox ParentBounds = CurrentSurfaceParent->GetCollisionBounds();
	const FBox TargetBounds = Target->GetCollisionBounds();
	const bool bWithinParent =
		TargetBounds.Min.X >= ParentBounds.Min.X &&
		TargetBounds.Max.X <= ParentBounds.Max.X &&
		TargetBounds.Min.Y >= ParentBounds.Min.Y &&
		TargetBounds.Max.Y <= ParentBounds.Max.Y &&
		TargetBounds.Min.Z >= ParentBounds.Max.Z - 2.0f;

	const bool bOverlapping = bWithinParent && Subsystem->IsOverlappingPlacedFurniture(Target, CurrentSurfaceParent, CurrentSurfaceParent);
	const bool bValid = bWithinParent && !bOverlapping;

	Subsystem->SetInvalidReason(bValid ? EPlacementInvalidReason::None : (bOverlapping ? EPlacementInvalidReason::Overlapping : EPlacementInvalidReason::OutsideFloor));
	Target->SetPlacementState(bValid ? EPlacementState::Preview : EPlacementState::Invalid);
}

void USurfacePlacementHandler::UpdateGizmoMoveFree(AFurniture* Target, FVector TargetLoc)
{
	UpdateGizmoMove(Target, TargetLoc, EGizmoTransformAxis::None);
}

void USurfacePlacementHandler::FinalizeGizmoMove(AFurniture* Target)
{
	if (!Target || !Subsystem)
	{
		return;
	}
	if (Subsystem->InvalidReason != EPlacementInvalidReason::None)
	{
		Target->SetActorLocation(GizmoDragStartLocation);
	}
	Target->SetPlacementState(EPlacementState::Placed);
	Subsystem->SetInvalidReason(EPlacementInvalidReason::None);
}

void USurfacePlacementHandler::AbortGizmoMove(AFurniture* Target)
{
	if (Target)
	{
		Target->SetActorLocation(GizmoDragStartLocation);
		Target->SetPlacementState(EPlacementState::Placed);
	}
	if (Subsystem)
	{
		Subsystem->SetInvalidReason(EPlacementInvalidReason::None);
	}
}
