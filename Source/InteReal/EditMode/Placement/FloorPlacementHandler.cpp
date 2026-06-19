#include "FloorPlacementHandler.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "InteReal/EditMode/Furniture/Furniture.h"
#include "InteReal/EditMode/Managers/GridSpaceManager.h"
#include "InteReal/EditMode/Visualization/PlacementVisualizerActor.h"

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

	// 유효성 검사
	bool bValid = true;
	EPlacementInvalidReason Reason = EPlacementInvalidReason::None;

	for (int32 i = 0; i < L; i++)
	{
		for (int32 j = 0; j < B; j++)
		{
			const FVector2D Cell(AnchorX + i, AnchorY + j);
			if (Grid->GetTileState(Cell) == EGridTileState::None)
			{
				bValid = false;
				Reason = EPlacementInvalidReason::OutsideFloor;
				break;
			}
			AActor* Occupant = Grid->GetFurniture(Cell);
			if (Occupant && Occupant != Preview)
			{
				bValid = false;
				Reason = EPlacementInvalidReason::Overlapping;
				break;
			}
		}
		if (!bValid)
		{
			break;
		}
	}

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

	// 배치된 가구와 바운딩박스 겹침 체크
	if (bValid)
	{
		const FBox PreviewBounds = Preview->GetCollisionBounds().ExpandBy(-1.0f);
		for (const AFurniture* Placed : Subsystem->GetPlacedFurnitures())
		{
			if (!IsValid(Placed) || Placed == Preview)
			{
				continue;
			}
			if (Placed->GetPlacedSurfaceType() != EPlacementSurfaceType::Floor)
			{
				continue;
			}
			if (PreviewBounds.Intersect(Placed->GetCollisionBounds()))
			{
				bValid = false;
				Reason = EPlacementInvalidReason::Overlapping;
				break;
			}
		}
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
	const int32 L = (int32)Dims.X;
	const int32 B = (int32)Dims.Y;

	for (int32 i = 0; i < L; i++)
	{
		for (int32 j = 0; j < B; j++)
		{
			Grid->SetFurniture(FVector2D(Anchor.X + i, Anchor.Y + j), Furniture);
		}
	}

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

	const int32 L = (int32)Furniture->PlacedDimensions.X;
	const int32 B = (int32)Furniture->PlacedDimensions.Y;
	for (int32 i = 0; i < L; i++)
	{
		for (int32 j = 0; j < B; j++)
		{
			Grid->SetFurniture(FVector2D(Furniture->PlacedGridAnchor.X + i, Furniture->PlacedGridAnchor.Y + j),
			                   nullptr);
		}
	}
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
	GizmoDragStartLocation = Target->GetActorLocation();
	Subsystem->GetPreviewGridAnchor() = GizmoDragOriginalAnchor;

	// 그리드 셀 비워두기
	const int32 L = (int32)Target->PlacedDimensions.X;
	const int32 B = (int32)Target->PlacedDimensions.Y;
	for (int32 i = 0; i < L; i++)
	{
		for (int32 j = 0; j < B; j++)
		{
			Grid->SetFurniture(FVector2D(Target->PlacedGridAnchor.X + i, Target->PlacedGridAnchor.Y + j), nullptr);
		}
	}
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
		NewLoc.Z = Cursor.Z;
	}
	else
	{
		NewLoc = FVector(Cursor.X, Cursor.Y, NewLoc.Z);
	}

	const FVector2D GridPos = Grid->ToGridPosition(NewLoc);
	const int32 AnchorX = FMath::RoundToInt(GridPos.X) - (L / 2);
	const int32 AnchorY = FMath::RoundToInt(GridPos.Y) - (B / 2);

	bool bValid = true;
	EPlacementInvalidReason Reason = EPlacementInvalidReason::None;
	for (int32 i = 0; i < L && bValid; i++)
	{
		for (int32 j = 0; j < B && bValid; j++)
		{
			const FVector2D Cell(AnchorX + i, AnchorY + j);
			if (Grid->GetTileState(Cell) == EGridTileState::None)
			{
				bValid = false;
				Reason = EPlacementInvalidReason::OutsideFloor;
			}
			AActor* Occ = Grid->GetFurniture(Cell);
			if (Occ && Occ != Target)
			{
				bValid = false;
				Reason = EPlacementInvalidReason::Overlapping;
			}
		}
	}

	const FVector WorldCenter = Grid->ToWorldPosition(
		FVector2D(AnchorX + L * 0.5f - 0.5f, AnchorY + B * 0.5f - 0.5f));
	Target->SetActorLocation(FVector(WorldCenter.X, WorldCenter.Y, NewLoc.Z));

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

	if (bValid && Subsystem->IsOverlappingPlacedFurniture(Target))
	{
		bValid = false;
		Reason = EPlacementInvalidReason::Overlapping;
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
	const int32 L = (int32)Target->PlacedDimensions.X;
	const int32 B = (int32)Target->PlacedDimensions.Y;

	bool bValid = Subsystem->InvalidReason == EPlacementInvalidReason::None;
	for (int32 i = 0; i < L && bValid; i++)
	{
		for (int32 j = 0; j < B && bValid; j++)
		{
			const FVector2D Cell(NewAnchor.X + i, NewAnchor.Y + j);
			if (Grid->GetTileState(Cell) == EGridTileState::None)
			{
				bValid = false;
			}
			AActor* Occ = Grid->GetFurniture(Cell);
			if (Occ && Occ != Target)
			{
				bValid = false;
			}
		}
	}

	if (bValid && (!IsCornersInsideFloor(Target) || IntersectsWalls(Target) || Subsystem->IsOverlappingPlacedFurniture(Target)))
	{
		bValid = false;
	}

	if (bValid)
	{
		Target->PlacedGridAnchor = NewAnchor;
		for (int32 i = 0; i < L; i++)
		{
			for (int32 j = 0; j < B; j++)
			{
				Grid->SetFurniture(FVector2D(NewAnchor.X + i, NewAnchor.Y + j), Target);
			}
		}
	}
	else
	{
		// 원래 위치로 되돌리기
		const int32 OL = (int32)Target->PlacedDimensions.X;
		const int32 OB = (int32)Target->PlacedDimensions.Y;
		const FVector WorldCenter = Grid->ToWorldPosition(FVector2D(
			GizmoDragOriginalAnchor.X + OL * 0.5f - 0.5f,
			GizmoDragOriginalAnchor.Y + OB * 0.5f - 0.5f));
		Target->SetActorLocation(FVector(WorldCenter.X, WorldCenter.Y, GizmoDragStartLocation.Z));
		Target->PlacedGridAnchor = GizmoDragOriginalAnchor;
		for (int32 i = 0; i < OL; i++)
		{
			for (int32 j = 0; j < OB; j++)
			{
				Grid->SetFurniture(FVector2D(GizmoDragOriginalAnchor.X + i, GizmoDragOriginalAnchor.Y + j), Target);
			}
		}
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

	const int32 L = (int32)Target->PlacedDimensions.X;
	const int32 B = (int32)Target->PlacedDimensions.Y;
	const FVector WorldCenter = Grid->ToWorldPosition(FVector2D(
		GizmoDragOriginalAnchor.X + L * 0.5f - 0.5f,
		GizmoDragOriginalAnchor.Y + B * 0.5f - 0.5f));
	Target->SetActorLocation(FVector(WorldCenter.X, WorldCenter.Y, GizmoDragStartLocation.Z));
	Target->PlacedGridAnchor = GizmoDragOriginalAnchor;
	for (int32 i = 0; i < L; i++)
	{
		for (int32 j = 0; j < B; j++)
		{
			Grid->SetFurniture(FVector2D(GizmoDragOriginalAnchor.X + i, GizmoDragOriginalAnchor.Y + j), Target);
		}
	}

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
