#include "FloorPlacementHandler.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "InteReal/EditMode/Furniture/Furniture.h"
#include "InteReal/EditMode/Managers/GridSpaceManager.h"
#include "InteReal/EditMode/Visualization/PlacementVisualizerActor.h"

static bool ValidateOccupiedCells(const TArray<FIntPoint>& Cells, AGridSpaceManager* Grid,
	const AFurniture* Furniture, EPlacementInvalidReason& OutReason)
{
	for (const FIntPoint& Cell : Cells)
	{
		const FVector2D GridCell(Cell.X, Cell.Y);
		if (Grid->GetTileState(GridCell) == EGridTileState::None)
		{
			OutReason = EPlacementInvalidReason::OutsideFloor;
			return false;
		}
		if (AActor* Occupant = Grid->GetFurniture(GridCell); Occupant && Occupant != Furniture)
		{
			OutReason = EPlacementInvalidReason::Overlapping;
			return false;
		}
	}
	return true;
}

static void SetOccupiedCells(AGridSpaceManager* Grid, const TArray<FIntPoint>& Cells, AFurniture* Furniture)
{
	for (const FIntPoint& Cell : Cells)
	{
		Grid->SetFurniture(FVector2D(Cell.X, Cell.Y), Furniture);
	}
}

static void ClearOccupiedCells(AGridSpaceManager* Grid, const TArray<FIntPoint>& Cells, const AFurniture* Furniture)
{
	for (const FIntPoint& Cell : Cells)
	{
		const FVector2D GridCell(Cell.X, Cell.Y);
		if (Grid->GetFurniture(GridCell) == Furniture)
		{
			Grid->SetFurniture(GridCell, nullptr);
		}
	}
}

static bool SegmentIntersectsAABB(FVector2D P1, FVector2D P2, FVector2D BoxMin, FVector2D BoxMax)
{
	float TMin = 0.0f;
	float TMax = 1.0f;
	const float DX = P2.X - P1.X;
	const float DY = P2.Y - P1.Y;

	auto Clip = [&](float P, float Q) -> bool
	{
		if (FMath::Abs(P) < KINDA_SMALL_NUMBER)
		{
			return Q >= 0.0f;
		}

		const float R = Q / P;
		if (P < 0.0f)
		{
			if (R > TMax)
			{
				return false;
			}
			TMin = FMath::Max(TMin, R);
		}
		else
		{
			if (R < TMin)
			{
				return false;
			}
			TMax = FMath::Min(TMax, R);
		}
		return true;
	};

	return Clip(-DX, P1.X - BoxMin.X) &&
	       Clip(DX, BoxMax.X - P1.X) &&
	       Clip(-DY, P1.Y - BoxMin.Y) &&
	       Clip(DY, BoxMax.Y - P1.Y);
}

void UFloorPlacementHandler::Initialize(UInteriorPlacementSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
}

bool UFloorPlacementHandler::CanHandle(const FHitResult& Hit) const
{
	if (Cast<AFurniture>(Hit.GetActor()))
	{
		return false;
	}
	const UPrimitiveComponent* Comp = Hit.GetComponent();
	if (Comp && Comp->ComponentHasTag(TEXT("EditableWall")))
	{
		return false;
	}
	if (Comp && Comp->ComponentHasTag(TEXT("Ceiling")))
	{
		return false;
	}
	const float FloorThreshold = Subsystem ? Subsystem->GetFloorZ() + 5.0f : 0.0f;
	return Hit.Location.Z <= FloorThreshold;
}

bool UFloorPlacementHandler::OwnsFurniture(const AFurniture* Furniture) const
{
	if (!Furniture)
	{
		return false;
	}
	return Furniture->GetPlacedSurfaceType() == EPlacementSurfaceType::Floor;
}

void UFloorPlacementHandler::UpdatePreview(AFurniture* Preview, const FHitResult& Hit)
{
	if (!Preview || !Subsystem)
	{
		return;
	}

	AGridSpaceManager* Grid = Subsystem->GetGrid();
	APlacementVisualizerActor* Visualizer = Subsystem->GetVisualizer();
	if (!Grid || !Visualizer)
	{
		return;
	}

	const FVector2D Dims = Subsystem->GetCurrentDimensions();
	const int32 L = FMath::Max(1, (int32)Dims.X);
	const int32 B = FMath::Max(1, (int32)Dims.Y);

	const FVector2D GridPos = Grid->ToGridPosition(Hit.Location);
	const int32 AnchorX = FMath::FloorToInt(GridPos.X) - (L / 2);
	const int32 AnchorY = FMath::FloorToInt(GridPos.Y) - (B / 2);
	const FVector2D NewAnchor(AnchorX, AnchorY);
	Subsystem->GetPreviewGridAnchor() = NewAnchor;

	const FVector WorldCenter = Grid->ToWorldPosition(FVector2D(AnchorX + L * 0.5f - 0.5f, AnchorY + B * 0.5f - 0.5f));
	Preview->SetActorLocation(FVector(WorldCenter.X, WorldCenter.Y, Subsystem->GetFloorZ()));
	Preview->AlignPlacementBottomCenterTo(WorldCenter, Subsystem->GetFloorZ());

	// 유효성 검사
	bool bValid = true;
	EPlacementInvalidReason Reason = EPlacementInvalidReason::None;

	TArray<FIntPoint> OccupiedCells;
	Preview->GetOccupiedGridCells(Grid, NewAnchor, Dims, OccupiedCells);
	bValid = ValidateOccupiedCells(OccupiedCells, Grid, Preview, Reason);

	if (bValid && !IsCornersInsideFloor(Preview))
	{
		bValid = false;
		Reason = EPlacementInvalidReason::OutsideFloor;
	}

	if (bValid && IntersectsWalls(Preview))
	{
		bValid = false;
		Reason = EPlacementInvalidReason::IntersectsWall;
	}

	Subsystem->SetInvalidReason(Reason);
	Preview->SetPlacementState(bValid ? EPlacementState::Preview : EPlacementState::Invalid);

	const FBox PreviewBounds = Preview->GetCollisionBounds();
	Visualizer->RefreshPlacementCellViz(PreviewBounds, !bValid, Subsystem->GetFloorZ());
}

void UFloorPlacementHandler::OnConfirm(AFurniture* Furniture)
{
	if (!Furniture || !Subsystem)
	{
		return;
	}

	AGridSpaceManager* Grid = Subsystem->GetGrid();
	if (!Grid)
	{
		return;
	}

	const FVector2D Anchor = Subsystem->GetPreviewGridAnchor();
	const FVector2D Dims = Subsystem->GetCurrentDimensions();
	Furniture->GetOccupiedGridCells(Grid, Anchor, Dims, Furniture->PlacedOccupiedCells);
	SetOccupiedCells(Grid, Furniture->PlacedOccupiedCells, Furniture);

	Furniture->PlacedGridAnchor = Anchor;
	Furniture->PlacedDimensions = Dims;
	Furniture->SetPlacedSurfaceType(EPlacementSurfaceType::Floor);
}

void UFloorPlacementHandler::OnRemove(AFurniture* Furniture)
{
	if (!Furniture || !Subsystem)
	{
		return;
	}

	AGridSpaceManager* Grid = Subsystem->GetGrid();
	if (!Grid)
	{
		return;
	}

	ClearOccupiedCells(Grid, Furniture->PlacedOccupiedCells, Furniture);
	Furniture->PlacedOccupiedCells.Reset();
}

// ─────────────────────────────────────────────
// 기즈모
// ─────────────────────────────────────────────

void UFloorPlacementHandler::BeginGizmoMove(AFurniture* Target)
{
	if (!Target || !Subsystem)
	{
		return;
	}

	AGridSpaceManager* Grid = Subsystem->GetGrid();
	if (!Grid)
	{
		return;
	}

	GizmoDragOriginalAnchor = Target->PlacedGridAnchor;
	GizmoDragOriginalDimensions = Target->PlacedDimensions;
	GizmoDragOriginalOccupiedCells = Target->PlacedOccupiedCells;
	GizmoDragStartLocation = Target->GetActorLocation();
	GizmoDragStartRotation = Target->GetActorRotation();
	Subsystem->GetPreviewGridAnchor() = GizmoDragOriginalAnchor;

	// 그리드 셀 비워두기
	ClearOccupiedCells(Grid, Target->PlacedOccupiedCells, Target);
}

void UFloorPlacementHandler::UpdateGizmoMove(AFurniture* Target, FVector Cursor, EGizmoTransformAxis Axis)
{
	if (!Target || !Subsystem)
	{
		return;
	}

	AGridSpaceManager* Grid = Subsystem->GetGrid();
	APlacementVisualizerActor* Visualizer = Subsystem->GetVisualizer();
	if (!Grid)
	{
		return;
	}

	const FVector2D Dims = Target->PlacedDimensions;
	const int32 L = FMath::Max(1, (int32)Dims.X);
	const int32 B = FMath::Max(1, (int32)Dims.Y);

	FVector DesiredMeshCenter = Target->GetMeshBounds().GetCenter();
	if (Axis == EGizmoTransformAxis::MoveX)
	{
		DesiredMeshCenter.X = Cursor.X;
	}
	else if (Axis == EGizmoTransformAxis::MoveY)
	{
		DesiredMeshCenter.Y = Cursor.Y;
	}
	else if (Axis != EGizmoTransformAxis::MoveZ)
	{
		DesiredMeshCenter.X = Cursor.X;
		DesiredMeshCenter.Y = Cursor.Y;
	}

	const FVector2D GridPos = Grid->ToGridPosition(DesiredMeshCenter);
	int32 AnchorX = FMath::RoundToInt(GizmoDragOriginalAnchor.X);
	int32 AnchorY = FMath::RoundToInt(GizmoDragOriginalAnchor.Y);
	if (Axis == EGizmoTransformAxis::MoveX)
	{
		AnchorX = FMath::RoundToInt(GridPos.X) - (L / 2);
	}
	else if (Axis == EGizmoTransformAxis::MoveY)
	{
		AnchorY = FMath::RoundToInt(GridPos.Y) - (B / 2);
	}
	else if (Axis != EGizmoTransformAxis::MoveZ)
	{
		AnchorX = FMath::RoundToInt(GridPos.X) - (L / 2);
		AnchorY = FMath::RoundToInt(GridPos.Y) - (B / 2);
	}

	bool bValid = true;
	EPlacementInvalidReason Reason = EPlacementInvalidReason::None;

	// 자유배치 모드: 그리드 셀 중심으로 스냅하지 않고 커서가 가리키는 연속 좌표를 그대로 사용.
	// 충돌/범위 판정은 여전히 셀 단위(AnchorX/Y)로 근사해서 처리한다.
	const FVector WorldCenter = Subsystem->IsFreePlacementMode()
		? DesiredMeshCenter
		: Grid->ToWorldPosition(FVector2D(AnchorX + L * 0.5f - 0.5f, AnchorY + B * 0.5f - 0.5f));
	if (Axis == EGizmoTransformAxis::MoveZ)
	{
		FVector ActorLocation = Target->GetActorLocation();
		ActorLocation.Z = Cursor.Z;
		Target->SetActorLocation(ActorLocation);
	}
	const FVector UpdatedMeshCenter = Target->GetMeshBounds().GetCenter();
	Target->AddActorWorldOffset(FVector(
		WorldCenter.X - UpdatedMeshCenter.X,
		WorldCenter.Y - UpdatedMeshCenter.Y,
		0.0f));

	TArray<FIntPoint> OccupiedCells;
	Target->GetOccupiedGridCells(Grid, FVector2D(AnchorX, AnchorY), Dims, OccupiedCells);
	bValid = ValidateOccupiedCells(OccupiedCells, Grid, Target, Reason);

	if (bValid && !IsCornersInsideFloor(Target))
	{
		bValid = false;
		Reason = EPlacementInvalidReason::OutsideFloor;
	}

	if (bValid && IntersectsWalls(Target))
	{
		bValid = false;
		Reason = EPlacementInvalidReason::IntersectsWall;
	}

	if (bValid)
	{
		Subsystem->GetPreviewGridAnchor() = FVector2D(AnchorX, AnchorY);
	}
	else
	{
		Subsystem->GetPreviewGridAnchor() = GizmoDragOriginalAnchor;
	}

	Subsystem->SetInvalidReason(Reason);
	Target->SetPlacementState(bValid ? EPlacementState::Preview : EPlacementState::Invalid);

	if (Visualizer)
	{
		const FBox Bounds = Target->GetCollisionBounds();
		Visualizer->RefreshPlacementCellViz(Bounds, !bValid, Subsystem->GetFloorZ());
	}
}

void UFloorPlacementHandler::UpdateGizmoMoveFree(AFurniture* Target, FVector TargetLoc)
{
	UpdateGizmoMove(Target, TargetLoc, EGizmoTransformAxis::None);
}

void UFloorPlacementHandler::FinalizeGizmoMove(AFurniture* Target)
{
	if (!Target || !Subsystem)
	{
		return;
	}

	AGridSpaceManager* Grid = Subsystem->GetGrid();
	APlacementVisualizerActor* Visualizer = Subsystem->GetVisualizer();
	if (!Grid)
	{
		return;
	}

	const FVector2D NewAnchor = Subsystem->GetPreviewGridAnchor();
	bool bValid = Subsystem->InvalidReason == EPlacementInvalidReason::None;
	TArray<FIntPoint> NewOccupiedCells;
	Target->GetOccupiedGridCells(Grid, NewAnchor, Target->PlacedDimensions, NewOccupiedCells);
	EPlacementInvalidReason CellReason = EPlacementInvalidReason::None;
	bValid = bValid && ValidateOccupiedCells(NewOccupiedCells, Grid, Target, CellReason);

	if (bValid && (!IsCornersInsideFloor(Target) || IntersectsWalls(Target)))
	{
		bValid = false;
	}

	if (bValid)
	{
		Target->PlacedGridAnchor = NewAnchor;
		Target->PlacedOccupiedCells = MoveTemp(NewOccupiedCells);
		SetOccupiedCells(Grid, Target->PlacedOccupiedCells, Target);
	}
	else
	{
		Target->SetActorRotation(GizmoDragStartRotation);
		Target->SetActorLocation(GizmoDragStartLocation);
		Target->PlacedGridAnchor = GizmoDragOriginalAnchor;
		Target->PlacedDimensions = GizmoDragOriginalDimensions;
		Target->PlacedOccupiedCells = GizmoDragOriginalOccupiedCells;
		SetOccupiedCells(Grid, Target->PlacedOccupiedCells, Target);
	}

	if (Visualizer)
	{
		Visualizer->ClearPlacementCellViz();
	}
	Target->SetPlacementState(EPlacementState::Placed);
	Subsystem->SetInvalidReason(EPlacementInvalidReason::None);
}

void UFloorPlacementHandler::AbortGizmoMove(AFurniture* Target)
{
	if (!Target || !Subsystem)
	{
		return;
	}

	AGridSpaceManager* Grid = Subsystem->GetGrid();
	if (!Grid)
	{
		return;
	}

	Target->SetActorRotation(GizmoDragStartRotation);
	Target->SetActorLocation(GizmoDragStartLocation);
	Target->PlacedGridAnchor = GizmoDragOriginalAnchor;
	Target->PlacedDimensions = GizmoDragOriginalDimensions;
	Target->PlacedOccupiedCells = GizmoDragOriginalOccupiedCells;
	SetOccupiedCells(Grid, Target->PlacedOccupiedCells, Target);

	if (APlacementVisualizerActor* Visualizer = Subsystem->GetVisualizer())
	{
		Visualizer->ClearPlacementCellViz();
	}
	Target->SetPlacementState(EPlacementState::Placed);
	Subsystem->SetInvalidReason(EPlacementInvalidReason::None);
}

// ─────────────────────────────────────────────
// 공용 유효성 헬퍼
// ─────────────────────────────────────────────

bool UFloorPlacementHandler::IsCornersInsideFloor(const AFurniture* Furniture) const
{
	if (!Subsystem || !Furniture)
	{
		return true;
	}
	const bool bHasRoomPolygons = !Subsystem->GetFloorRoomPolygons().IsEmpty();
	const TArray<FVector2D>& Polygon = Subsystem->GetFloorPolygon();
	if (!bHasRoomPolygons && Polygon.Num() < 3)
	{
		return true;
	}

	const FBox Bounds = Furniture->GetCollisionBounds().ExpandBy(-1.0f);
	const TArray<FVector2D> Corners = {
		FVector2D(Bounds.Min.X, Bounds.Min.Y),
		FVector2D(Bounds.Max.X, Bounds.Min.Y),
		FVector2D(Bounds.Min.X, Bounds.Max.Y),
		FVector2D(Bounds.Max.X, Bounds.Max.Y),
	};
	for (const FVector2D& C : Corners)
	{
		if (!Subsystem->IsPointInsideFloor(C))
		{
			return false;
		}
	}
	return true;
}

bool UFloorPlacementHandler::IntersectsWalls(const AFurniture* Furniture) const
{
	if (!Subsystem || !Furniture)
	{
		return false;
	}
	const TArray<TPair<FVector2D, FVector2D>>& Walls = Subsystem->GetWallSegments();
	if (Walls.IsEmpty())
	{
		return false;
	}

	const FBox Bounds = Furniture->GetCollisionBounds().ExpandBy(-1.0f);
	const FBox2D Bounds2D(FVector2D(Bounds.Min.X, Bounds.Min.Y), FVector2D(Bounds.Max.X, Bounds.Max.Y));

	const float Thickness = Subsystem->GetWallThickness() * 0.5f;
	for (const TPair<FVector2D, FVector2D>& Wall : Walls)
	{
		if (SegmentIntersectsAABB(Wall.Key, Wall.Value, Bounds2D.Min, Bounds2D.Max))
		{
			return true;
		}

		const FVector2D Dir = (Wall.Value - Wall.Key).GetSafeNormal();
		const FVector2D Perp(-Dir.Y, Dir.X);
		const float Len = (Wall.Value - Wall.Key).Size();
		for (const FVector2D& Corner : {
			     Bounds2D.Min, FVector2D(Bounds2D.Min.X, Bounds2D.Max.Y),
			     FVector2D(Bounds2D.Max.X, Bounds2D.Min.Y), Bounds2D.Max
		     })
		{
			const float Along = FVector2D::DotProduct(Corner - Wall.Key, Dir);
			const float PerpDist = FMath::Abs(FVector2D::DotProduct(Corner - Wall.Key, Perp));
			if (Along < -Thickness || Along > Len + Thickness)
			{
				continue;
			}
			if (PerpDist < Thickness)
			{
				return true;
			}
		}
	}
	return false;
}
