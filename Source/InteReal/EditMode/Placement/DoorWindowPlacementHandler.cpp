#include "DoorWindowPlacementHandler.h"

#include "InteReal/EditMode/Furniture/Furniture.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "InteReal/EditMode/Visualization/PlacementVisualizerActor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

namespace
{
	bool IsWallComponent(const FHitResult& Hit)
	{
		const UPrimitiveComponent* Comp = Hit.GetComponent();
		return Comp && Comp->ComponentHasTag(TEXT("EditableWall"));
	}

	bool SupportsWallPlacement(const FFurnitureDataRow& Row)
	{
		return (Row.AllowedPlacementTypes & static_cast<uint8>(EPlacementSurfaceType::Wall)) != 0;
	}

	FString RowLabelLower(const FFurnitureDataRow& Row)
	{
		return Row.DisplayName.ToString().ToLower();
	}

	bool DoorWindow_IsWindowLikeName(const FString& Label)
	{
		return Label.Contains(TEXT("window")) ||
		       Label.Contains(TEXT("\ucc3d\ubb38")) ||
		       Label.Contains(TEXT("\ucc3d\ud638")) ||
		       Label.Contains(TEXT("\ucc3d"));
	}

	bool DoorWindow_IsDoorLikeName(const FString& Label)
	{
		return Label.Contains(TEXT("door")) ||
		       Label.Contains(TEXT("\ubb38")) ||
		       Label.Contains(TEXT("\ud604\uad00")) ||
		       Label.Contains(TEXT("\ubbf8\ub2eb")) ||
		       Label.Contains(TEXT("\uc2ac\ub77c\uc774\ub529"));
	}

	bool OverlapsOpeningFurniture(const AFurniture* Target, const UInteriorPlacementSubsystem* Subsystem)
	{
		if (!Target || !Subsystem)
		{
			return false;
		}

		const FBox TargetBounds = Target->GetMeshBounds().ExpandBy(-1.0f);
		for (const AFurniture* Placed : Subsystem->GetPlacedFurnitures())
		{
			if (!IsValid(Placed) || Placed == Target ||
				Placed->GetPlacedSurfaceType() != EPlacementSurfaceType::Wall)
			{
				continue;
			}

			const FFurnitureDataRow* Row = Subsystem->FindFurnitureRowByID(Placed->FurnitureID);
			if (!Row)
			{
				continue;
			}

			const FString Label = RowLabelLower(*Row);
            const bool bPlacedIsOpening =
                Row->AssetKind != EPlacementAssetKind::Generic ||
				DoorWindow_IsDoorLikeName(Label) ||
				DoorWindow_IsWindowLikeName(Label);

			if (bPlacedIsOpening && TargetBounds.Intersect(Placed->GetMeshBounds()))
			{
				return true;
			}
		}
		return false;
	}
}

void UDoorWindowPlacementHandler::Initialize(UInteriorPlacementSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
}

bool UDoorWindowPlacementHandler::CanHandle(const FHitResult& Hit) const
{
	if (!Subsystem || Cast<AFurniture>(Hit.GetActor()))
	{
		return false;
	}

	const UPrimitiveComponent* Comp = Hit.GetComponent();
	if (Comp && Comp->ComponentHasTag(TEXT("Ceiling")))
	{
		return false;
	}
	if (Hit.ImpactNormal.Z < -0.5f || !IsWallComponent(Hit))
	{
		return false;
	}

	const FFurnitureDataRow& Row = Subsystem->GetCurrentFurnitureRow();
	return SupportsWallPlacement(Row) && IsOpeningRow(Row);
}

bool UDoorWindowPlacementHandler::OwnsFurniture(const AFurniture* Furniture) const
{
	if (!Furniture || !Subsystem || Furniture->GetPlacedSurfaceType() != EPlacementSurfaceType::Wall)
	{
		return false;
	}

	const FFurnitureDataRow* Row = Subsystem->FindFurnitureRowByID(Furniture->FurnitureID);
	if (Row)
	{
		return IsOpeningRow(*Row);
	}
	return Furniture->Tags.Contains(FName(TEXT("OpeningAsset")));
}

void UDoorWindowPlacementHandler::UpdatePreview(AFurniture* Preview, const FHitResult& Hit)
{
	if (!Preview || !Subsystem)
	{
		return;
	}

	const FFurnitureDataRow& Row = Subsystem->GetCurrentFurnitureRow();
	const EPlacementAssetKind Kind = ResolveOpeningKind(Row);
	const FVector2D HitXY(Hit.ImpactPoint.X, Hit.ImpactPoint.Y);

	FVector2D SegStart, SegEnd;
	if (!FindNearestWallSegment(HitXY, SegStart, SegEnd))
	{
		Preview->SetPlacementState(EPlacementState::Invalid);
		Subsystem->SetInvalidReason(EPlacementInvalidReason::OutsideFloor);
		return;
	}

	FVector2D HitWallNormal(Hit.ImpactNormal.X, Hit.ImpactNormal.Y);
	if (!HitWallNormal.Normalize())
	{
		HitWallNormal = FVector2D::ZeroVector;
	}

	ApplyOpeningAlignedRotation(Preview, HitWallNormal);
	ApplyOpeningFitScale(Preview, Row, Kind);

	const FVector SnappedLoc = ComputeOpeningSnappedLocation(
		Preview,
		Row,
		Kind,
		HitXY,
		SegStart,
		SegEnd,
		HitWallNormal.IsNearlyZero() ? nullptr : &HitWallNormal);
	Preview->SetActorLocation(SnappedLoc);

	const float BottomZ = Subsystem->GetFloorZ() + ResolveOpeningBottom(Row, Kind);
	Preview->AlignMeshBottomCenterTo(SnappedLoc, BottomZ);

	const bool bValid = ValidateOpeningPlacement(Preview, Row, SegStart, SegEnd);
	Preview->SetPlacementState(bValid ? EPlacementState::Preview : EPlacementState::Invalid);

	if (APlacementVisualizerActor* Visualizer = Subsystem->GetVisualizer())
	{
		Visualizer->RefreshPlacementWallViz(Preview->GetCollisionBounds(), CurrentWallNormal, !bValid);
	}
}

void UDoorWindowPlacementHandler::OnConfirm(AFurniture* Furniture, bool bIsValid)
{
	if (!Furniture)
	{
		return;
	}

	Furniture->SetPlacedSurfaceType(EPlacementSurfaceType::Wall);
	Furniture->WallNormalAtPlacement = CurrentWallNormal;
	Furniture->Tags.AddUnique(FName(TEXT("OpeningAsset")));

	if (const FFurnitureDataRow* Row = Subsystem ? Subsystem->FindFurnitureRowByID(Furniture->FurnitureID) : nullptr)
	{
		const EPlacementAssetKind Kind = ResolveOpeningKind(*Row);
		if (Kind == EPlacementAssetKind::Window)
		{
			Furniture->Tags.AddUnique(FName(TEXT("WindowAsset")));
		}
		else
		{
			Furniture->Tags.AddUnique(FName(TEXT("DoorAsset")));
		}
	}
}

void UDoorWindowPlacementHandler::OnRemove(AFurniture* Furniture)
{
}

void UDoorWindowPlacementHandler::BeginGizmoMove(AFurniture* Target)
{
	if (!Target)
	{
		return;
	}

	GizmoDragStartLocation = Target->GetActorLocation();
	const FVector2D Loc2D(Target->GetActorLocation().X, Target->GetActorLocation().Y);
	FindNearestWallSegment(Loc2D, GizmoWallSegStart, GizmoWallSegEnd);
	CurrentWallNormal = Target->WallNormalAtPlacement;
}

void UDoorWindowPlacementHandler::UpdateGizmoMove(AFurniture* Target, FVector Cursor, EGizmoTransformAxis Axis)
{
	if (!Target || !Subsystem)
	{
		return;
	}

	const FFurnitureDataRow* Row = Subsystem->FindFurnitureRowByID(Target->FurnitureID);
	if (!Row)
	{
		return;
	}

	FVector NewLoc = Target->GetActorLocation();
	if (Axis == EGizmoTransformAxis::MoveZ)
	{
		NewLoc.Z = FMath::Max(Cursor.Z, Subsystem->GetFloorZ());
		Target->SetActorLocation(NewLoc);
	}
	else
	{
		FVector2D FixedNormal2D(CurrentWallNormal.X, CurrentWallNormal.Y);
		if (FixedNormal2D.IsNearlyZero())
		{
			FixedNormal2D = FVector2D(Target->WallNormalAtPlacement.X, Target->WallNormalAtPlacement.Y);
		}

		NewLoc = ComputeOpeningSnappedLocation(
			Target,
			*Row,
			ResolveOpeningKind(*Row),
			FVector2D(Cursor.X, Cursor.Y),
			GizmoWallSegStart,
			GizmoWallSegEnd,
			FixedNormal2D.IsNearlyZero() ? nullptr : &FixedNormal2D);
		NewLoc.Z = Target->GetActorLocation().Z;
		Target->SetActorLocation(NewLoc);
	}

	const bool bValid = ValidateOpeningPlacement(Target, *Row, GizmoWallSegStart, GizmoWallSegEnd);
	Target->SetPlacementState(bValid ? EPlacementState::Preview : EPlacementState::Invalid);

	if (APlacementVisualizerActor* Visualizer = Subsystem->GetVisualizer())
	{
		Visualizer->RefreshPlacementWallViz(Target->GetCollisionBounds(), CurrentWallNormal, !bValid);
	}
}

void UDoorWindowPlacementHandler::UpdateGizmoMoveFree(AFurniture* Target, FVector TargetLoc)
{
	UpdateGizmoMove(Target, TargetLoc, EGizmoTransformAxis::None);
}

void UDoorWindowPlacementHandler::FinalizeGizmoMove(AFurniture* Target)
{
	if (Target)
	{
		// 벽 슬롯을 벗어나도 위치는 그대로 두고 경고만 표시한다 (되돌리지 않음).
		// 이 핸들러는 Subsystem->InvalidReason을 갱신하지 않으므로 가구 자신의
		// PlacementState(Invalid 여부)를 직접 확인한다.
		Target->SetOverlapWarning(Target->GetPlacementState() == EPlacementState::Invalid
			? EPlacementInvalidReason::OutOfBounds : EPlacementInvalidReason::None);
		Target->WallNormalAtPlacement = CurrentWallNormal;
		Target->SetPlacementState(EPlacementState::Placed);
	}
	if (Subsystem)
	{
		Subsystem->SetInvalidReason(EPlacementInvalidReason::None);
	}
	if (APlacementVisualizerActor* Visualizer = Subsystem ? Subsystem->GetVisualizer() : nullptr)
	{
		Visualizer->ClearPlacementCellViz();
	}
}

void UDoorWindowPlacementHandler::AbortGizmoMove(AFurniture* Target)
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
	if (APlacementVisualizerActor* Visualizer = Subsystem ? Subsystem->GetVisualizer() : nullptr)
	{
		Visualizer->ClearPlacementCellViz();
	}
}

bool UDoorWindowPlacementHandler::IsOpeningRow(const FFurnitureDataRow& Row) const
{
    if (ResolveOpeningKind(Row) != EPlacementAssetKind::Generic)
    {
        return true;
    }

    const FString Label = RowLabelLower(Row);
    return DoorWindow_IsDoorLikeName(Label) || DoorWindow_IsWindowLikeName(Label);
}

EPlacementAssetKind UDoorWindowPlacementHandler::ResolveOpeningKind(const FFurnitureDataRow& Row) const
{
    if (Row.AssetKind != EPlacementAssetKind::Generic)
    {
        return Row.AssetKind;
    }

    const FString Label = RowLabelLower(Row);
    if (Label.Contains(TEXT("sliding")) ||
        Label.Contains(TEXT("\ubbf8\ub2eb")) ||
        Label.Contains(TEXT("\uc2ac\ub77c\uc774\ub529")))
    {
        return EPlacementAssetKind::SlidingDoor;
    }
    if (Label.Contains(TEXT("entrance")) || Label.Contains(TEXT("\ud604\uad00")))
    {
        return EPlacementAssetKind::EntranceDoor;
    }
    if (DoorWindow_IsWindowLikeName(Label))
    {
        return EPlacementAssetKind::Window;
    }
    if (DoorWindow_IsDoorLikeName(Label))
    {
        return EPlacementAssetKind::Door;
    }
    return EPlacementAssetKind::Generic;
}

bool UDoorWindowPlacementHandler::FindNearestWallSegment(const FVector2D& Point2D, FVector2D& OutSegStart, FVector2D& OutSegEnd) const
{
	if (!Subsystem)
	{
		return false;
	}

	const TArray<TPair<FVector2D, FVector2D>>& Walls = Subsystem->GetWallSegments();
	if (Walls.IsEmpty())
	{
		return false;
	}

	float MinDistSq = TNumericLimits<float>::Max();
	int32 BestIdx = INDEX_NONE;
	for (int32 i = 0; i < Walls.Num(); ++i)
	{
		float T = 0.0f;
		const FVector2D Closest = ProjectPointOnSegment(Point2D, Walls[i].Key, Walls[i].Value, T);
		const float DistSq = FVector2D::DistSquared(Point2D, Closest);
		if (DistSq < MinDistSq)
		{
			MinDistSq = DistSq;
			BestIdx = i;
		}
	}

	if (BestIdx == INDEX_NONE)
	{
		return false;
	}

	OutSegStart = Walls[BestIdx].Key;
	OutSegEnd = Walls[BestIdx].Value;
	return true;
}

void UDoorWindowPlacementHandler::ApplyOpeningAlignedRotation(AFurniture* Target, const FVector2D& WallNormal) const
{
	if (!Target || !Subsystem || WallNormal.IsNearlyZero())
	{
		return;
	}

	const FVector2D Tangent(WallNormal.Y, -WallNormal.X);
	const float TangentYaw = FMath::RadiansToDegrees(FMath::Atan2(Tangent.Y, Tangent.X));
	FRotator Rotation = Subsystem->GetPreviewRotation();
	Rotation.Yaw = FRotator::NormalizeAxis(TangentYaw + Rotation.Yaw);
	Target->SetActorRotation(Rotation);
}

void UDoorWindowPlacementHandler::ApplyOpeningFitScale(AFurniture* Target, const FFurnitureDataRow& Row, EPlacementAssetKind Kind) const
{
	if (!Target || !Row.bFitToOpeningSlot)
	{
		return;
	}

	const FVector NativeSize = GetNativeMeshSize(Target);
	if (NativeSize.X <= KINDA_SMALL_NUMBER ||
		NativeSize.Y <= KINDA_SMALL_NUMBER ||
		NativeSize.Z <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float SlotWidth = ResolveSlotWidth(Row, Kind, NativeSize);
	const float SlotDepth = ResolveSlotDepth(Row, Subsystem, NativeSize);
	const float SlotHeight = ResolveSlotHeight(Row, Kind, NativeSize);

	Target->SetActorScale3D(FVector(
		SlotWidth / NativeSize.X,
		SlotDepth / NativeSize.Y,
		SlotHeight / NativeSize.Z));
}

FVector UDoorWindowPlacementHandler::ComputeOpeningSnappedLocation(AFurniture* Target,
                                                                   const FFurnitureDataRow& Row,
                                                                   EPlacementAssetKind Kind,
                                                                   const FVector2D& CursorXY,
                                                                   const FVector2D& SegStart,
                                                                   const FVector2D& SegEnd,
                                                                   const FVector2D* FixedWallNormal)
{
	if (!Target || !Subsystem)
	{
		return FVector(CursorXY.X, CursorXY.Y, Subsystem ? Subsystem->GetFloorZ() : 0.0f);
	}

	float T = 0.0f;
	const FVector2D ClosestPt = ProjectPointOnSegment(CursorXY, SegStart, SegEnd, T);
	const FVector2D SegVec = SegEnd - SegStart;
	const float SegLength = SegVec.Size();
	const FVector2D SegDirN = SegLength > KINDA_SMALL_NUMBER ? SegVec / SegLength : FVector2D(1.0f, 0.0f);

	const FVector NativeSize = GetNativeMeshSize(Target);
	const float SlotWidth = ResolveSlotWidth(Row, Kind, NativeSize);
	const float HalfSlotWidth = SlotWidth * 0.5f;
	const float DistAlongWall = FMath::Clamp(T * SegLength, HalfSlotWidth, FMath::Max(HalfSlotWidth, SegLength - HalfSlotWidth));
	const FVector2D SnappedXY = SegStart + SegDirN * DistAlongWall;

	FVector2D WallNormal(-SegDirN.Y, SegDirN.X);
	if (FixedWallNormal && !FixedWallNormal->IsNearlyZero())
	{
		WallNormal = FixedWallNormal->GetSafeNormal();
	}
	else
	{
		const FVector2D ToCursor = CursorXY - ClosestPt;
		if (FVector2D::DotProduct(WallNormal, ToCursor) < 0.0f)
		{
			WallNormal = -WallNormal;
		}
	}

	CurrentWallNormal = FVector(WallNormal.X, WallNormal.Y, 0.0f);
	const FVector2D FinalXY = SnappedXY + WallNormal * Row.WallOffset;
	const float BottomZ = Subsystem->GetFloorZ() + ResolveOpeningBottom(Row, Kind);
	return FVector(FinalXY.X, FinalXY.Y, BottomZ);
}

bool UDoorWindowPlacementHandler::ValidateOpeningPlacement(AFurniture* Target,
                                                           const FFurnitureDataRow& Row,
                                                           const FVector2D& SegStart,
                                                           const FVector2D& SegEnd) const
{
	if (!Target || !Subsystem)
	{
		return false;
	}

	const FVector NativeSize = GetNativeMeshSize(Target);
	const EPlacementAssetKind Kind = ResolveOpeningKind(Row);
	const float SlotWidth = ResolveSlotWidth(Row, Kind, NativeSize);
	const float SegLength = FVector2D::Distance(SegStart, SegEnd);
	if (SegLength + KINDA_SMALL_NUMBER < SlotWidth)
	{
		Subsystem->SetInvalidReason(EPlacementInvalidReason::OutOfBounds);
		return false;
	}

	const FBox Bounds = Target->GetMeshBounds();
	if (Bounds.Min.Z < Subsystem->GetFloorZ() - KINDA_SMALL_NUMBER)
	{
		Subsystem->SetInvalidReason(EPlacementInvalidReason::OutsideFloor);
		return false;
	}

	if (OverlapsOpeningFurniture(Target, Subsystem))
	{
		Subsystem->SetInvalidReason(EPlacementInvalidReason::Overlapping);
		return false;
	}

	Subsystem->SetInvalidReason(EPlacementInvalidReason::None);
	return true;
}

FVector UDoorWindowPlacementHandler::GetNativeMeshSize(const AFurniture* Target)
{
	if (!Target)
	{
		return FVector::ZeroVector;
	}

	TArray<UStaticMeshComponent*> Meshes;
	Target->GetComponents<UStaticMeshComponent>(Meshes);
	for (const UStaticMeshComponent* MeshComp : Meshes)
	{
		if (const UStaticMesh* Mesh = MeshComp ? MeshComp->GetStaticMesh() : nullptr)
		{
			return Mesh->GetBounds().GetBox().GetSize();
		}
	}
	return Target->GetMeshBounds().GetSize();
}

float UDoorWindowPlacementHandler::ResolveSlotWidth(const FFurnitureDataRow& Row, EPlacementAssetKind Kind, const FVector& NativeSize)
{
	if (Row.Width > KINDA_SMALL_NUMBER)
	{
		return Row.Width;
	}
	if (Kind == EPlacementAssetKind::Window)
	{
		return NativeSize.X > KINDA_SMALL_NUMBER ? NativeSize.X : 120.0f;
	}
	return NativeSize.X > KINDA_SMALL_NUMBER ? NativeSize.X : 90.0f;
}

float UDoorWindowPlacementHandler::ResolveSlotHeight(const FFurnitureDataRow& Row, EPlacementAssetKind Kind, const FVector& NativeSize)
{
	if (Row.Height > KINDA_SMALL_NUMBER)
	{
		return Row.Height;
	}
	if (NativeSize.Z > KINDA_SMALL_NUMBER)
	{
		return NativeSize.Z;
	}
	return Kind == EPlacementAssetKind::Window ? 120.0f : 210.0f;
}

float UDoorWindowPlacementHandler::ResolveSlotDepth(const FFurnitureDataRow& Row,
                                                    const UInteriorPlacementSubsystem* Subsystem,
                                                    const FVector& NativeSize)
{
	if (Row.Depth > KINDA_SMALL_NUMBER)
	{
		return Row.Depth;
	}
	if (Subsystem && Subsystem->GetWallThickness() > KINDA_SMALL_NUMBER)
	{
		return Subsystem->GetWallThickness();
	}
	return NativeSize.Y > KINDA_SMALL_NUMBER ? NativeSize.Y : 20.0f;
}

float UDoorWindowPlacementHandler::ResolveOpeningBottom(const FFurnitureDataRow& Row, EPlacementAssetKind Kind)
{
	if (Row.OpeningBottom > KINDA_SMALL_NUMBER)
	{
		return Row.OpeningBottom;
	}
	return Kind == EPlacementAssetKind::Window ? 90.0f : 0.0f;
}

FVector2D UDoorWindowPlacementHandler::ProjectPointOnSegment(const FVector2D& Point,
                                                             const FVector2D& SegStart,
                                                             const FVector2D& SegEnd,
                                                             float& OutT)
{
	const FVector2D SegVec = SegEnd - SegStart;
	const float LenSq = SegVec.SizeSquared();
	if (LenSq < KINDA_SMALL_NUMBER)
	{
		OutT = 0.0f;
		return SegStart;
	}
	OutT = FMath::Clamp(FVector2D::DotProduct(Point - SegStart, SegVec) / LenSq, 0.0f, 1.0f);
	return SegStart + SegVec * OutT;
}
