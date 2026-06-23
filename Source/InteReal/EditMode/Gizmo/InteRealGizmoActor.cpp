#include "InteRealGizmoActor.h"
#include "InteReal/EditMode/Furniture/Furniture.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"

static void SetGizmoOutlineColor(UWorld* World, FLinearColor Color)
{
	if (!World) return;
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		for (FWeightedBlendable& WB : It->Settings.WeightedBlendables.Array)
		{
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(WB.Object);
			if (!MID)
			{
				if (UMaterialInterface* Material = Cast<UMaterialInterface>(WB.Object))
				{
					MID = UMaterialInstanceDynamic::Create(Material, *It);
					WB.Object = MID;
				}
			}
			if (MID)
			{
				MID->SetVectorParameterValue(TEXT("OutlineColor"), Color);
			}
		}
	}
}

AInteRealGizmoActor::AInteRealGizmoActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

FString AInteRealGizmoActor::GetAxisTagFromComponent(const UPrimitiveComponent* Component)
{
	if (!Component)
	{
		return FString();
	}

	for (const FName& Tag : Component->ComponentTags)
	{
		const FString TagStr = Tag.ToString();
		if (TagStr.StartsWith(TEXT("Move")) || TagStr.StartsWith(TEXT("Rotate")) || TagStr == TEXT("RotationRing"))
		{
			return TagStr;
		}
	}

	const FString Name = Component->GetName();
	if (Name.StartsWith(TEXT("ArrowX")))
	{
		return TEXT("MoveX");
	}
	if (Name.StartsWith(TEXT("ArrowY")))
	{
		return TEXT("MoveY");
	}
	if (Name.StartsWith(TEXT("ArrowZ")))
	{
		return TEXT("MoveZ");
	}
	if (Name.StartsWith(TEXT("RingPitch")))
	{
		return TEXT("RotatePitch");
	}
	if (Name.StartsWith(TEXT("RingRoll")))
	{
		return TEXT("RotateRoll");
	}
	if (Name.StartsWith(TEXT("RingYaw")) || Name.StartsWith(TEXT("RotationRing")))
	{
		return TEXT("RotateYaw");
	}

	return FString();
}

void AInteRealGizmoActor::InitAxisMaterials()
{
	AxisMaterials.Empty();
	AxisMeshes.Empty();
	OriginalAxisColors.Empty();
	HoveredAxis.Empty();

	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);

	for (UMeshComponent* Mesh : Meshes)
	{
		const FString AxisTag = GetAxisTagFromComponent(Mesh);
		if (AxisTag.IsEmpty()) continue;

		Mesh->SetRenderCustomDepth(false);

		AxisMeshes.FindOrAdd(AxisTag).Add(Mesh);

		TArray<TObjectPtr<UMaterialInstanceDynamic>>& DMIs = AxisMaterials.FindOrAdd(AxisTag);
		for (int32 i = 0; i < Mesh->GetNumMaterials(); i++)
		{
			if (UMaterialInstanceDynamic* DMI = Mesh->CreateAndSetMaterialInstanceDynamic(i))
			{
				DMI->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
				OriginalAxisColors.FindOrAdd(AxisTag) = DMI->K2_GetVectorParameterValue(GizmoColorParamName);
				DMI->SetScalarParameterValue(RadialWipeParamName, 0.0f);
				DMI->SetScalarParameterValue(SnapHighlightParamName, 0.0f);
				DMI->SetScalarParameterValue(RotationDirectionParamName, 1.0f);
				DMIs.Add(DMI);
			}
		}
	}

	SetDisplayMode(EInteRealGizmoDisplayMode::All);
}

void AInteRealGizmoActor::SetDisplayMode(EInteRealGizmoDisplayMode NewMode)
{
	DisplayMode = NewMode;
	HoveredAxis.Empty();
	ResetRotationVisuals();

	TArray<UPrimitiveComponent*> Components;
	GetComponents<UPrimitiveComponent>(Components);

	for (UPrimitiveComponent* Component : Components)
	{
		if (!Component) continue;

		const FString AxisTag = GetAxisTagFromComponent(Component);
		if (AxisTag.IsEmpty()) continue;

		const bool bIsRotationComponent = AxisTag.StartsWith(TEXT("Rotate")) || AxisTag == TEXT("RotationRing");
		const bool bShouldShow =
			DisplayMode == EInteRealGizmoDisplayMode::All ||
			(DisplayMode == EInteRealGizmoDisplayMode::Rotation
				? bIsRotationComponent
				: !bIsRotationComponent);

		const bool bIsVisualMesh = Component->IsA<UMeshComponent>();
		Component->SetVisibility(bIsVisualMesh && bShouldShow, true);
		Component->SetHiddenInGame(!bIsVisualMesh || !bShouldShow, true);
		Component->SetCollisionEnabled(bShouldShow ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

void AInteRealGizmoActor::SetAxisOutline(const FString& Axis, bool bEnable)
{
	const TArray<TObjectPtr<UMeshComponent>>* Meshes = AxisMeshes.Find(Axis);
	if (!Meshes) return;

	if (bEnable)
	{
		SetGizmoOutlineColor(GetWorld(), FLinearColor::White);
	}
	SetAxisColorHighlight(Axis, bEnable);

	const bool bIsMove = Axis.StartsWith(TEXT("Move"));
	const int32 Stencil = bIsMove ? MoveOutlineStencil : RotateOutlineStencil;

	for (UMeshComponent* Mesh : *Meshes)
	{
		if (!Mesh) continue;
		Mesh->SetCustomDepthStencilValue(bEnable ? Stencil : 0);
		Mesh->SetRenderCustomDepth(bEnable);
		Mesh->MarkRenderStateDirty();
	}

}

void AInteRealGizmoActor::SetAxisRotationVisuals(const FString& Axis, float DeltaAngle, bool bSnapped)
{
	if (const TArray<TObjectPtr<UMaterialInstanceDynamic>>* DMIs = AxisMaterials.Find(Axis))
	{
		const float Wipe = FMath::Clamp(FMath::Abs(DeltaAngle) / 360.0f, 0.0f, 1.0f);
		const float Direction = DeltaAngle < 0.0f ? -1.0f : 1.0f;
		const float SnapHighlight = bSnapped ? 1.0f : 0.0f;

		for (UMaterialInstanceDynamic* DMI : *DMIs)
		{
			if (DMI)
			{
				DMI->SetScalarParameterValue(RadialWipeParamName, Wipe);
				DMI->SetScalarParameterValue(RotationDirectionParamName, Direction);
				DMI->SetScalarParameterValue(SnapHighlightParamName, SnapHighlight);
			}
		}
	}
}

void AInteRealGizmoActor::ResetRotationVisuals()
{
	for (const TPair<FString, TArray<TObjectPtr<UMaterialInstanceDynamic>>>& Pair : AxisMaterials)
	{
		for (UMaterialInstanceDynamic* DMI : Pair.Value)
		{
			if (DMI)
			{
				DMI->SetScalarParameterValue(RadialWipeParamName, 0.0f);
				DMI->SetScalarParameterValue(SnapHighlightParamName, 0.0f);
				DMI->SetScalarParameterValue(RotationDirectionParamName, 1.0f);
			}
		}
	}
}

FString AInteRealGizmoActor::GetAxisTagFromHit(const FHitResult& CursorHit) const
{
	UPrimitiveComponent* HitComp = CursorHit.GetComponent();
	if (!HitComp || HitComp->GetOwner() != this) return FString();

	return GetAxisTagFromComponent(HitComp);
}

EGizmoTransformAxis AInteRealGizmoActor::ParseAxisTag(const FString& AxisTag) const
{
	if (AxisTag == TEXT("MoveX"))
	{
		return EGizmoTransformAxis::MoveX;
	}
	if (AxisTag == TEXT("MoveY"))
	{
		return EGizmoTransformAxis::MoveY;
	}
	if (AxisTag == TEXT("MoveZ"))
	{
		return EGizmoTransformAxis::MoveZ;
	}
	if (AxisTag == TEXT("RotatePitch"))
	{
		return EGizmoTransformAxis::RotatePitch;
	}
	if (AxisTag == TEXT("RotateRoll"))
	{
		return EGizmoTransformAxis::RotateRoll;
	}
	if (AxisTag.StartsWith(TEXT("Rotate")) || AxisTag == TEXT("RotationRing"))
	{
		return EGizmoTransformAxis::RotateYaw;
	}
	return EGizmoTransformAxis::None;
}

float AInteRealGizmoActor::ApplyCardinalSnap(float AngleDegrees) const
{
	if (CardinalSnapIntervalDegrees <= UE_SMALL_NUMBER || CardinalSnapToleranceDegrees <= 0.0f)
	{
		return FRotator::NormalizeAxis(AngleDegrees);
	}

	const float Snapped = FMath::GridSnap(AngleDegrees, CardinalSnapIntervalDegrees);
	const float DeltaToSnap = FMath::Abs(FRotator::NormalizeAxis(AngleDegrees - Snapped));
	return DeltaToSnap <= CardinalSnapToleranceDegrees
		? FRotator::NormalizeAxis(Snapped)
		: FRotator::NormalizeAxis(AngleDegrees);
}

void AInteRealGizmoActor::UpdateHover(bool bIsHitting, const FHitResult& CursorHit)
{
	if (AxisMaterials.Num() == 0) return;

	const FString NewHoveredAxis = bIsHitting ? GetAxisTagFromHit(CursorHit) : FString();
	if (NewHoveredAxis == HoveredAxis) return;

	if (!HoveredAxis.IsEmpty())
	{
		SetAxisOutline(HoveredAxis, false);
	}
	if (!NewHoveredAxis.IsEmpty())
	{
		SetAxisOutline(NewHoveredAxis, true);
	}
	HoveredAxis = NewHoveredAxis;
}

void AInteRealGizmoActor::BeginDrag(const FString& Axis,
                                    AFurniture* Target,
                                    const FVector& WorldOrigin,
                                    const FVector& WorldDir,
                                    const FVector2D& MousePos)
{
	if (!Target) return;

	CurrentDraggingAxisTag = Axis;
	CurrentDraggingAxis = ParseAxisTag(Axis);
	bIsDragging = true;
	DragStartMousePos = MousePos;
	CurrentRotationDeltaDegrees = 0.0f;

	SetAxisOutline(Axis, true);

	if (CurrentDraggingAxis == EGizmoTransformAxis::RotateYaw ||
		CurrentDraggingAxis == EGizmoTransformAxis::RotatePitch ||
		CurrentDraggingAxis == EGizmoTransformAxis::RotateRoll)
	{
		DragStartFurnitureRot = Target->GetActorRotation();
		bHasRotationScreenCenter = false;
		if (const UWorld* World = GetWorld())
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				bHasRotationScreenCenter = PC->ProjectWorldLocationToScreen(
					Target->GetVisualBounds().GetCenter(), RotationScreenCenter);
			}
		}

		DragStartAngleDeg = bHasRotationScreenCenter
			? FMath::RadiansToDegrees(FMath::Atan2(
				MousePos.Y - RotationScreenCenter.Y,
				MousePos.X - RotationScreenCenter.X))
			: 0.0f;
		LastRotationMouseAngleDeg = DragStartAngleDeg;
		AccumulatedRotationDeltaDegrees = 0.0f;
	}
	else if (CurrentDraggingAxis == EGizmoTransformAxis::MoveX ||
	         CurrentDraggingAxis == EGizmoTransformAxis::MoveY ||
	         CurrentDraggingAxis == EGizmoTransformAxis::MoveZ)
	{
		DragStartLocation = Target->GetActorLocation();

		if (CurrentDraggingAxis == EGizmoTransformAxis::MoveX || CurrentDraggingAxis == EGizmoTransformAxis::MoveY)
		{
			if (Target->GetPlacedSurfaceType() == EPlacementSurfaceType::Floor)
			{
				// FloorPlacementHandler snaps the visible mesh center, not the imported pivot.
				DragStartLocation = Target->GetMeshBounds().GetCenter();
			}

			// 클릭 시점의 커서-가구 오프셋을 기록해 드래그 시작 시 위치가 튀지 않게 함
			FVector PlaneNormal = FVector::UpVector;
			if (Target->GetPlacedSurfaceType() == EPlacementSurfaceType::Wall && !Target->WallNormalAtPlacement.IsNearlyZero())
			{
				PlaneNormal = Target->WallNormalAtPlacement;
			}

			const FPlane DragPlane(DragStartLocation, PlaneNormal);
			const FVector CursorOnPlane = FMath::LinePlaneIntersection(WorldOrigin,
			                                                           WorldOrigin + WorldDir * 100000.f,
			                                                           DragPlane);
			DragCursorOffset = CursorOnPlane - DragStartLocation;
		}
	}
}

void AInteRealGizmoActor::UpdateDrag(AFurniture* Target,
                                     const FVector& WorldOrigin,
                                     const FVector& WorldDir,
                                     UInteriorPlacementSubsystem* PlacementSubsystem,
                                     bool bSnapRotationToGrid,
                                     const FVector2D& MousePos)
{
	if (!bIsDragging || !Target) return;

	// 회전
	if (CurrentDraggingAxis == EGizmoTransformAxis::RotateYaw ||
		CurrentDraggingAxis == EGizmoTransformAxis::RotatePitch ||
		CurrentDraggingAxis == EGizmoTransformAxis::RotateRoll)
	{
		const float CurrentAngle = bHasRotationScreenCenter
			? FMath::RadiansToDegrees(FMath::Atan2(
				MousePos.Y - RotationScreenCenter.Y,
				MousePos.X - RotationScreenCenter.X))
			: (MousePos.X - DragStartMousePos.X) * 0.5f;

		if (bHasRotationScreenCenter)
		{
			const float FrameDelta = FRotator::NormalizeAxis(CurrentAngle - LastRotationMouseAngleDeg);
			AccumulatedRotationDeltaDegrees += FrameDelta;
			LastRotationMouseAngleDeg = CurrentAngle;
		}
		else
		{
			AccumulatedRotationDeltaDegrees =
				(MousePos.X - DragStartMousePos.X) * 0.5f * RotationSensitivity;
		}

		float DeltaAngle = AccumulatedRotationDeltaDegrees;
		if (bSnapRotationToGrid)
		{
			DeltaAngle = FMath::GridSnap(DeltaAngle, 15.0f);
		}

		const float DeltaMagnitude = FMath::Abs(DeltaAngle);
		const float WrappedMagnitude = FMath::Fmod(DeltaMagnitude, 360.0f);
		CurrentRotationDeltaDegrees = DeltaAngle < 0.0f ? -WrappedMagnitude : WrappedMagnitude;

		FRotator NewRot = DragStartFurnitureRot;
		if (CurrentDraggingAxis == EGizmoTransformAxis::RotatePitch)
		{
			const float RawAngle = FRotator::NormalizeAxis(NewRot.Pitch + DeltaAngle);
			NewRot.Pitch = bSnapRotationToGrid
				? ApplyCardinalSnap(NewRot.Pitch + DeltaAngle)
				: RawAngle;
			SetAxisRotationVisuals(CurrentDraggingAxisTag, CurrentRotationDeltaDegrees, !FMath::IsNearlyEqual(RawAngle, NewRot.Pitch, 0.01f));
		}
		else if (CurrentDraggingAxis == EGizmoTransformAxis::RotateRoll)
		{
			const float RawAngle = FRotator::NormalizeAxis(NewRot.Roll - DeltaAngle);
			NewRot.Roll = bSnapRotationToGrid
				? ApplyCardinalSnap(NewRot.Roll - DeltaAngle)
				: RawAngle;
			SetAxisRotationVisuals(CurrentDraggingAxisTag, CurrentRotationDeltaDegrees, !FMath::IsNearlyEqual(RawAngle, NewRot.Roll, 0.01f));
		}
		else
		{
			const float RawAngle = FRotator::NormalizeAxis(NewRot.Yaw + DeltaAngle);
			NewRot.Yaw = bSnapRotationToGrid
				? ApplyCardinalSnap(NewRot.Yaw + DeltaAngle)
				: RawAngle;
			SetAxisRotationVisuals(CurrentDraggingAxisTag, CurrentRotationDeltaDegrees, !FMath::IsNearlyEqual(RawAngle, NewRot.Yaw, 0.01f));
		}

		Target->SetRotationPreservingPlacement(NewRot);
		return;
	}

	// X / Y 이동 (그리드 스냅 + 충돌은 PlacementSubsystem이 처리)
	if ((CurrentDraggingAxis == EGizmoTransformAxis::MoveX || CurrentDraggingAxis == EGizmoTransformAxis::MoveY) && PlacementSubsystem)
	{
		FVector PlaneNormal = FVector::UpVector;
		if (Target->GetPlacedSurfaceType() == EPlacementSurfaceType::Wall && !Target->WallNormalAtPlacement.IsNearlyZero())
		{
			PlaneNormal = Target->WallNormalAtPlacement;
		}

		const FPlane DragPlane(DragStartLocation, PlaneNormal);
		const FVector CursorOnPlane = FMath::LinePlaneIntersection(WorldOrigin,
		                                                           WorldOrigin + WorldDir * 100000.f,
		                                                           DragPlane);
		PlacementSubsystem->UpdateGizmoMoveLocation(CursorOnPlane - DragCursorOffset, Target, CurrentDraggingAxis);
		return;
	}

	// Z 이동 (수직) - 화면상 마우스 Y 이동량을 카메라 거리에 비례한 월드 단위로 변환
	// (TopDown처럼 커서 레이가 거의 수직일 때는 레이-평면 교차로 Z를 구할 수 없어 화면 델타 기반으로 계산)
	if (CurrentDraggingAxis == EGizmoTransformAxis::MoveZ)
	{
		const float Distance = FVector::Dist(WorldOrigin, DragStartLocation);
		const float DeltaY = DragStartMousePos.Y - MousePos.Y; // 위로 드래그 = 양수
		FVector NewLoc = DragStartLocation;
		NewLoc.Z += DeltaY * ZDragSensitivity * (Distance / ReferenceDistance);

		// 가구 하단이 바닥 아래로 뚫고 내려가지 않도록 클램프
		if (PlacementSubsystem)
		{
			const float BoundsBottomOffset = Target->GetActorLocation().Z - Target->GetVisualBounds().Min.Z;
			const float MinActorZ = PlacementSubsystem->GetFloorZ() + BoundsBottomOffset;
			NewLoc.Z = FMath::Max(NewLoc.Z, MinActorZ);
			PlacementSubsystem->UpdateGizmoMoveLocation(NewLoc, Target, CurrentDraggingAxis);
		}
		else
		{
			Target->SetActorLocation(NewLoc);
		}
		return;
	}
}

void AInteRealGizmoActor::UpdateConstantScreenSize(APlayerController* PlayerController, float ScaleMultiplier)
{
	if (!PlayerController || !PlayerController->PlayerCameraManager)
	{
		return;
	}

	const FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
	const float CameraFOVDegrees = PlayerController->PlayerCameraManager->GetFOVAngle();
	const float Distance = FVector::Dist(CameraLocation, GetActorLocation());
	const float FOVScale = FMath::Tan(FMath::DegreesToRadians(CameraFOVDegrees * 0.5f));
	float Scale = FMath::Clamp(Distance * FOVScale * ScaleMultiplier / ReferenceDistance, MinScreenScale, MaxScreenScale);
	SetActorScale3D(FVector(Scale));

	// Correct the approximation using the gizmo's actual projected pixel bounds.
	const FBox Bounds = GetVisibleGizmoBounds();
	if (!Bounds.IsValid)
	{
		return;
	}

	const FVector Min = Bounds.Min;
	const FVector Max = Bounds.Max;
	const FVector Corners[8] = {
		{Min.X, Min.Y, Min.Z}, {Max.X, Min.Y, Min.Z},
		{Min.X, Max.Y, Min.Z}, {Max.X, Max.Y, Min.Z},
		{Min.X, Min.Y, Max.Z}, {Max.X, Min.Y, Max.Z},
		{Min.X, Max.Y, Max.Z}, {Max.X, Max.Y, Max.Z}
	};

	FVector2D ScreenMin(FLT_MAX, FLT_MAX);
	FVector2D ScreenMax(-FLT_MAX, -FLT_MAX);
	bool bProjected = false;
	for (const FVector& Corner : Corners)
	{
		FVector2D ScreenCorner;
		if (PlayerController->ProjectWorldLocationToScreen(Corner, ScreenCorner, true))
		{
			ScreenMin.X = FMath::Min(ScreenMin.X, ScreenCorner.X);
			ScreenMin.Y = FMath::Min(ScreenMin.Y, ScreenCorner.Y);
			ScreenMax.X = FMath::Max(ScreenMax.X, ScreenCorner.X);
			ScreenMax.Y = FMath::Max(ScreenMax.Y, ScreenCorner.Y);
			bProjected = true;
		}
	}

	if (bProjected)
	{
		const FVector2D PixelSize = ScreenMax - ScreenMin;
		const float CurrentDiameter = FMath::Max(PixelSize.X, PixelSize.Y);
		if (CurrentDiameter > 1.0f)
		{
			const float DesiredDiameter = TargetScreenDiameterPixels;
			Scale = FMath::Clamp(Scale * DesiredDiameter / CurrentDiameter, MinScreenScale, MaxScreenScale);
			SetActorScale3D(FVector(Scale));
		}
	}
}

void AInteRealGizmoActor::SetAxisColorHighlight(const FString& Axis, bool bEnable)
{
	if (const TArray<TObjectPtr<UMaterialInstanceDynamic>>* Materials = AxisMaterials.Find(Axis))
	{
		const FLinearColor Color = bEnable ? ActiveAxisColor : GetBaseAxisColor(Axis);
		for (UMaterialInstanceDynamic* Material : *Materials)
		{
			if (Material)
			{
				Material->SetVectorParameterValue(GizmoColorParamName, Color);
			}
		}
	}
}

FLinearColor AInteRealGizmoActor::GetBaseAxisColor(const FString& Axis) const
{
	if (const FLinearColor* OriginalColor = OriginalAxisColors.Find(Axis))
	{
		return *OriginalColor;
	}
	return FLinearColor::White;
}

FBox AInteRealGizmoActor::GetVisibleGizmoBounds() const
{
	FBox Bounds(EForceInit::ForceInit);
	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);
	for (const UMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->IsVisible())
		{
			Bounds += Mesh->Bounds.GetBox();
		}
	}
	return Bounds;
}

void AInteRealGizmoActor::EndDrag()
{
	if (!CurrentDraggingAxisTag.IsEmpty())
	{
		SetAxisOutline(CurrentDraggingAxisTag, false);
	}
	ResetRotationVisuals();
	bIsDragging = false;
	bHasRotationScreenCenter = false;
	CurrentDraggingAxis = EGizmoTransformAxis::None;
	CurrentDraggingAxisTag.Empty();
	// UpdateHover must treat the axis under the released cursor as a fresh hover
	// and turn CustomDepth back on during the next tick.
	HoveredAxis.Empty();
	CurrentRotationDeltaDegrees = 0.0f;
}
