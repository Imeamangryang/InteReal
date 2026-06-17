#include "WallPlacementHandler.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "InteReal/EditMode/Furnitures/Furniture.h"
#include "InteReal/EditMode/Visualization/PlacementVisualizerActor.h"

void UWallPlacementHandler::Initialize(UInteriorPlacementSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
}

bool UWallPlacementHandler::CanHandle(const FHitResult& Hit) const
{
	if (Cast<AFurniture>(Hit.GetActor()))
	{
		return false;
	}
	const UPrimitiveComponent* Comp = Hit.GetComponent();
	if (Comp && Comp->ComponentHasTag(TEXT("Ceiling")))
	{
		return false;
	}
	// 천장 표면 (법선이 아래 방향) → CeilingPlacementHandler에게 넘김
	if (Hit.ImpactNormal.Z < -0.5f)
	{
		return false;
	}
	if (Comp && Comp->ComponentHasTag(TEXT("EditableWall")))
	{
		return true;
	}
	// 천장 지원 가구 (Wall 미지원) → CeilingPlacementHandler에게 넘김
	if (Subsystem)
	{
		const AFurniture* Preview = Subsystem->GetPreviewFurniture();
		if (Preview
			&& Preview->SupportsPlacementType(EPlacementSurfaceType::Ceiling)
			&& !Preview->SupportsPlacementType(EPlacementSurfaceType::Wall))
		{
			return false;
		}
	}
	const float FloorThreshold = Subsystem ? Subsystem->GetFloorZ() + 5.0f : 0.0f;
	return Hit.Location.Z > FloorThreshold;
}

bool UWallPlacementHandler::OwnsFurniture(const AFurniture* Furniture) const
{
	if (!Furniture)
	{
		return false;
	}
	return Furniture->GetPlacedSurfaceType() == EPlacementSurfaceType::Wall;
}

void UWallPlacementHandler::UpdatePreview(AFurniture* Preview, const FHitResult& Hit)
{
	if (!Preview || !Subsystem)
	{
		return;
	}

	// 먼저 회전 적용 — 이후 GetCollisionBounds가 올바른 회전 기준으로 계산됨
	Preview->SetActorRotation(Subsystem->GetPreviewRotation());

	const FVector2D HitXY(Hit.ImpactPoint.X, Hit.ImpactPoint.Y);
	FVector2D SegStart, SegEnd;
	if (!FindNearestWallSegment(HitXY, SegStart, SegEnd))
	{
		Preview->SetPlacementState(EPlacementState::Invalid);
		Subsystem->SetInvalidReason(EPlacementInvalidReason::OutsideFloor);
		return;
	}

	// Z 스냅 (그리드 셀 단위, 최소 FloorZ)
	const float GridCell = Subsystem->GetGridCellSize();
	const float FloorZ   = Subsystem->GetFloorZ();
	const float SnappedZ = FMath::Max(
		FMath::RoundToFloat(Hit.ImpactPoint.Z / GridCell) * GridCell,
		FloorZ
	);

	// ComputeWallSnappedLocation 내부에서 CurrentWallNormal도 갱신됨
	const FVector SnappedLoc = ComputeWallSnappedLocation(Preview, HitXY, SegStart, SegEnd, SnappedZ);
	Preview->SetActorLocation(SnappedLoc);

	bool bValid = true;
	const FBox Bounds = Preview->GetCollisionBounds();
	if (Bounds.Min.Z < Subsystem->GetFloorZ())
	{
		bValid = false;
		Subsystem->SetInvalidReason(EPlacementInvalidReason::OutsideFloor);
	}

	if (bValid)
	{
		const FBox ExpandedBounds = Bounds.ExpandBy(-1.0f);
		for (const AFurniture* Placed : Subsystem->GetPlacedFurnitures())
		{
			if (!IsValid(Placed) || Placed == Preview)
			{
				continue;
			}
			if (Placed->GetPlacedSurfaceType() != EPlacementSurfaceType::Wall)
			{
				continue;
			}
			if (ExpandedBounds.Intersect(Placed->GetCollisionBounds()))
			{
				bValid = false;
				Subsystem->SetInvalidReason(EPlacementInvalidReason::Overlapping);
				break;
			}
		}
	}

	if (bValid)
	{
		Subsystem->SetInvalidReason(EPlacementInvalidReason::None);
	}
	Preview->SetPlacementState(bValid ? EPlacementState::Preview : EPlacementState::Invalid);

	if (APlacementVisualizerActor* Visualizer = Subsystem->GetVisualizer())
	{
		Visualizer->RefreshPlacementCellViz(Preview->GetCollisionBounds(), bValid, Subsystem->GetFloorZ());
	}
}

void UWallPlacementHandler::OnConfirm(AFurniture* Furniture)
{
	if (!Furniture)
	{
		return;
	}
	Furniture->SetPlacedSurfaceType(EPlacementSurfaceType::Wall);
	Furniture->WallNormalAtPlacement = CurrentWallNormal;
}

void UWallPlacementHandler::OnRemove(AFurniture* Furniture)
{
}

// 기즈모
void UWallPlacementHandler::BeginGizmoMove(AFurniture* Target)
{
	if (!Target)
	{
		return;
	}
	GizmoDragStartLocation = Target->GetActorLocation();
	const FVector2D Loc2D(Target->GetActorLocation().X, Target->GetActorLocation().Y);
	FindNearestWallSegment(Loc2D, GizmoWallSegStart, GizmoWallSegEnd);
}

void UWallPlacementHandler::UpdateGizmoMove(AFurniture* Target, FVector Cursor, EGizmoTransformAxis Axis)
{
	if (!Target || !Subsystem)
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
		NewLoc = FVector(Cursor.X, Cursor.Y, NewLoc.Z);
	}

	const FVector2D CursorXY(NewLoc.X, NewLoc.Y);
	const FVector SnappedLoc = ComputeWallSnappedLocation(Target,
	                                                      CursorXY,
	                                                      GizmoWallSegStart,
	                                                      GizmoWallSegEnd,
	                                                      NewLoc.Z);
	Target->SetActorLocation(SnappedLoc);

	if (APlacementVisualizerActor* Visualizer = Subsystem->GetVisualizer())
	{
		Visualizer->RefreshPlacementCellViz(Target->GetCollisionBounds(), true, Subsystem->GetFloorZ());
	}
}

void UWallPlacementHandler::UpdateGizmoMoveFree(AFurniture* Target, FVector TargetLoc)
{
	UpdateGizmoMove(Target, TargetLoc, EGizmoTransformAxis::None);
}

void UWallPlacementHandler::FinalizeGizmoMove(AFurniture* Target)
{
	if (APlacementVisualizerActor* Visualizer = Subsystem ? Subsystem->GetVisualizer() : nullptr)
	{
		Visualizer->ClearPlacementCellViz();
	}
}

void UWallPlacementHandler::AbortGizmoMove(AFurniture* Target)
{
	if (Target)
	{
		Target->SetActorLocation(GizmoDragStartLocation);
	}
	if (APlacementVisualizerActor* Visualizer = Subsystem ? Subsystem->GetVisualizer() : nullptr)
	{
		Visualizer->ClearPlacementCellViz();
	}
}

bool UWallPlacementHandler::FindNearestWallSegment(const FVector2D& Point2D,
                                                   FVector2D& OutSegStart,
                                                   FVector2D& OutSegEnd) const
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
	int32 BestIdx = -1;
	for (int32 i = 0; i < Walls.Num(); i++)
	{
		float T;
		const FVector2D Closest = ProjectPointOnSegment(Point2D, Walls[i].Key, Walls[i].Value, T);
		const float DistSq = FVector2D::DistSquared(Point2D, Closest);
		if (DistSq < MinDistSq)
		{
			MinDistSq = DistSq;
			BestIdx = i;
		}
	}

	if (BestIdx < 0)
	{
		return false;
	}
	OutSegStart = Walls[BestIdx].Key;
	OutSegEnd = Walls[BestIdx].Value;
	return true;
}

FVector UWallPlacementHandler::ComputeWallSnappedLocation(AFurniture* Target,
                                                          const FVector2D& CursorXY,
                                                          const FVector2D& SegStart,
                                                          const FVector2D& SegEnd,
                                                          float Z)
{
	if (!Target || !Subsystem)
	{
		return FVector(CursorXY.X, CursorXY.Y, Z);
	}

	// 커서를 세그먼트에 투영
	float T;
	const FVector2D ClosestPt = ProjectPointOnSegment(CursorXY, SegStart, SegEnd, T);

	const FVector2D SegVec  = SegEnd - SegStart;
	const float SegLength   = SegVec.Size();
	const FVector2D SegDirN = SegLength > KINDA_SMALL_NUMBER ? SegVec / SegLength : FVector2D(1.0f, 0.0f);

	// 현재 회전이 적용된 상태의 가구 바운드
	const FBox    CollisionBox   = Target->GetCollisionBounds();
	const FVector BoxExtent      = CollisionBox.GetExtent();
	const FVector BoxCenterOff   = CollisionBox.GetCenter() - Target->GetActorLocation();

	// 세그먼트 방향 기준 반폭 → 벽 따라 이동 범위 클램프
	const float HalfFurnitureWidth = FMath::Abs(BoxExtent.X * SegDirN.X) + FMath::Abs(BoxExtent.Y * SegDirN.Y);
	const float GridCell = Subsystem->GetGridCellSize();
	float SnappedDist = FMath::RoundToFloat((T * SegLength) / GridCell) * GridCell;
	SnappedDist = FMath::Clamp(SnappedDist, HalfFurnitureWidth, SegLength - HalfFurnitureWidth);
	const FVector2D SnappedXY = SegStart + SegDirN * SnappedDist;

	// 벽 법선 방향 결정 (커서가 있는 쪽 = 실내 방향)
	FVector2D WallNormal(-SegDirN.Y, SegDirN.X);
	const FVector2D ToCursor = CursorXY - ClosestPt;
	if (FVector2D::DotProduct(WallNormal, ToCursor) < 0.0f)
	{
		WallNormal = -WallNormal;
	}

	// 가구를 벽 표면 밖으로 밀어내는 오프셋
	const float ExtentAlongNormal  = FMath::Abs(BoxExtent.X * WallNormal.X) + FMath::Abs(BoxExtent.Y * WallNormal.Y);
	const float OriginAlongNormal  = BoxCenterOff.X * WallNormal.X + BoxCenterOff.Y * WallNormal.Y;
	const FFurnitureDataRow* Row   = Subsystem->FindFurnitureRowByID(Target->FurnitureID);
	const float WallOffset         = Row ? Row->WallOffset : 0.0f;
	const float PushOffset         = Subsystem->GetWallThickness() * 0.5f
	                               + (ExtentAlongNormal - OriginAlongNormal)
	                               + WallOffset;

	const FVector2D FinalXY = SnappedXY + WallNormal * PushOffset;

	// CurrentWallNormal을 세그먼트 기반으로 갱신 (RotatePreview 호출 시에도 안전)
	CurrentWallNormal = FVector(WallNormal.X, WallNormal.Y, 0.0f);

	return FVector(FinalXY.X, FinalXY.Y, Z);
}

FVector2D UWallPlacementHandler::ProjectPointOnSegment(FVector2D Point,
                                                       FVector2D SegStart,
                                                       FVector2D SegEnd,
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
