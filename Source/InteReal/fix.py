import os

path = r'c:\Users\PC\Documents\Unreal Projects\InteReal\Source\InteReal\Master\InteRealPlayerController.cpp'
with open(path, 'r', encoding='utf-8') as f:
    lines = f.readlines()

new_lines = []
skip = False
for i, line in enumerate(lines):
    if i == 1340: # start replacing from line 1341
        pass
    if 1340 <= i <= 1366:
        continue
    new_lines.append(line)

replacement = '''	if (AFurniture* HitFurniture = Cast<AFurniture>(LastCursorHit.GetActor()))
	{
		if (HitFurniture->GetPlacementState() == EPlacementState::Placed)
		{
			if (SelectedFurniture == HitFurniture)
			{
				if (FloorPlanPlacementSyncComponent && !FloorPlanPlacementSyncComponent->IsSyncingFurniture3DFrom2D())
				{
					FloorPlanPlacementSyncComponent->SelectFloorPlan2DForFurniture(SelectedFurniture);
				}

				bIsMovingFurniture = true;
				DragStartFurnitureLocation = SelectedFurniture->GetActorLocation();
				MoveDragOffset = DragStartFurnitureLocation - CurrentCursorWorldLoc;
				MoveDragOffset.Z = 0.0f;

				if (UInteriorPlacementSubsystem* PSMove = GetPlacementSubsystem())
				{
					PSMove->BeginGizmoMove(SelectedFurniture);
				}
				return;
			}

			SelectFurniture(HitFurniture);
			return;
		}
	}
'''
new_lines.insert(1340, replacement)

with open(path, 'w', encoding='utf-8') as f:
    f.write(''.join(new_lines))
