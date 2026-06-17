#include "SurfacePlacementHandler.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "InteReal/EditMode/Furnitures/Furniture.h"
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
		const FBox ShrunkBounds = PreviewBounds.ExpandBy(-1.0f);
		for (const AFurniture* Placed : Subsystem->GetPlacedFurnitures())
		{
			if (!IsValid(Placed) || Placed == Preview || Placed == HitFurniture)
			{
				continue;
			}
			if (Placed->GetPlacedSurfaceType() != EPlacementSurfaceType::Surface)
			{
				continue;
			}
			if (Placed->ParentFurniture != HitFurniture)
			{
				continue;
			}
			if (ShrunkBounds.Intersect(Placed->GetCollisionBounds()))
			{
				bOverlaps = true;
				break;
			}
		}
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
