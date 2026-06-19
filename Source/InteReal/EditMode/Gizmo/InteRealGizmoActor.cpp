#include "InteRealGizmoActor.h"
#include "InteReal/EditMode/Furniture/Furniture.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AInteRealGizmoActor::AInteRealGizmoActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

static bool IsCollisionOnlyGizmoComponent(const UPrimitiveComponent* Component)
{
	if (!Component)
	{
		return false;
	}

	const FString Name = Component->GetName();
	if (Name.Contains(TEXT("Collision"), ESearchCase::IgnoreCase) ||
		Name.Contains(TEXT("Hit"), ESearchCase::IgnoreCase))
	{
		return true;
	}

	return false;
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
	if (Name.StartsWith(TEXT("RingYaw")) || Name == TEXT("RotationRing"))
	{
		return TEXT("RotateYaw");
	}

	return FString();
}

void AInteRealGizmoActor::InitAxisMaterials()
{
	AxisMaterials.Empty();
	HoveredAxis.Empty();

	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);

	for (UMeshComponent* Mesh : Meshes)
	{
		if (IsCollisionOnlyGizmoComponent(Mesh))
		{
			Mesh->SetVisibility(false, false);
			Mesh->SetHiddenInGame(true, false);
			continue;
		}

		const FString AxisTag = GetAxisTagFromComponent(Mesh);
		if (AxisTag.IsEmpty()) continue;

		TArray<TObjectPtr<UMaterialInstanceDynamic>>& DMIs = AxisMaterials.FindOrAdd(AxisTag);
		for (int32 i = 0; i < Mesh->GetNumMaterials(); i++)
		{
			if (UMaterialInstanceDynamic* DMI = Mesh->CreateAndSetMaterialInstanceDynamic(i))
			{
				DMI->SetScalarParameterValue(OpacityParamName, DefaultOpacity);
				DMI->SetScalarParameterValue(RadialWipeParamName, 0.0f);
				DMI->SetScalarParameterValue(SnapHighlightParamName, 0.0f);
				DMI->SetScalarParameterValue(RotationDirectionParamName, 1.0f);
				DMIs.Add(DMI);
			}
		}
	}

	SetDisplayMode(DisplayMode);
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
		const bool bShouldShow = DisplayMode == EInteRealGizmoDisplayMode::Rotation
			? bIsRotationComponent
			: !bIsRotationComponent;

		const bool bCollisionOnly = IsCollisionOnlyGizmoComponent(Component);
		Component->SetVisibility(bShouldShow && !bCollisionOnly, true);
		Component->SetHiddenInGame(!bShouldShow || bCollisionOnly, true);
		Component->SetCollisionEnabled(bShouldShow ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

void AInteRealGizmoActor::SetAxisOpacity(const FString& Axis, float Opacity)
{
	if (const TArray<TObjectPtr<UMaterialInstanceDynamic>>* DMIs = AxisMaterials.Find(Axis))
	{
		for (UMaterialInstanceDynamic* DMI : *DMIs)
		{
			if (DMI)
			{
				DMI->SetScalarParameterValue(OpacityParamName, Opacity);
			}
		}
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
		SetAxisOpacity(HoveredAxis, DefaultOpacity);
	}
	if (!NewHoveredAxis.IsEmpty())
	{
		SetAxisOpacity(NewHoveredAxis, HighlightOpacity);
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

	if (CurrentDraggingAxis == EGizmoTransformAxis::RotateYaw ||
		CurrentDraggingAxis == EGizmoTransformAxis::RotatePitch ||
		CurrentDraggingAxis == EGizmoTransformAxis::RotateRoll)
	{
		DragStartFurnitureRot = Target->GetActorRotation();

		// 축별 회전 평면 선택
		FVector PlaneNormal = FVector::UpVector;
		if (CurrentDraggingAxis == EGizmoTransformAxis::RotatePitch)
		{
			PlaneNormal = FVector::RightVector;
		}
		else if (CurrentDraggingAxis == EGizmoTransformAxis::RotateRoll)
		{
			PlaneNormal = FVector::ForwardVector;
		}

		const FVector Center = Target->GetActorLocation();
		const FVector Hit = FMath::LinePlaneIntersection(WorldOrigin,
		                                                 WorldOrigin + WorldDir * 100000.f,
		                                                 FPlane(Center, PlaneNormal));

		if (CurrentDraggingAxis == EGizmoTransformAxis::RotatePitch)
		{
			DragStartAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Hit.Z - Center.Z, Hit.X - Center.X));
		}
		else if (CurrentDraggingAxis == EGizmoTransformAxis::RotateRoll)
		{
			DragStartAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Hit.Z - Center.Z, Hit.Y - Center.Y));
		}
		else
		{
			DragStartAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Hit.Y - Center.Y, Hit.X - Center.X));
		}
	}
	else if (CurrentDraggingAxis == EGizmoTransformAxis::MoveX ||
	         CurrentDraggingAxis == EGizmoTransformAxis::MoveY ||
	         CurrentDraggingAxis == EGizmoTransformAxis::MoveZ)
	{
		DragStartLocation = Target->GetActorLocation();

		if (CurrentDraggingAxis == EGizmoTransformAxis::MoveX || CurrentDraggingAxis == EGizmoTransformAxis::MoveY)
		{
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
		FVector PlaneNormal = FVector::UpVector;
		if (CurrentDraggingAxis == EGizmoTransformAxis::RotatePitch)
		{
			PlaneNormal = FVector::RightVector;
		}
		else if (CurrentDraggingAxis == EGizmoTransformAxis::RotateRoll)
		{
			PlaneNormal = FVector::ForwardVector;
		}

		const FVector Center = Target->GetActorLocation();
		const FVector Hit = FMath::LinePlaneIntersection(WorldOrigin,
		                                                 WorldOrigin + WorldDir * 100000.f,
		                                                 FPlane(Center, PlaneNormal));

		float CurrentAngle;
		if (CurrentDraggingAxis == EGizmoTransformAxis::RotatePitch)
		{
			CurrentAngle = FMath::RadiansToDegrees(FMath::Atan2(Hit.Z - Center.Z, Hit.X - Center.X));
		}
		else if (CurrentDraggingAxis == EGizmoTransformAxis::RotateRoll)
		{
			CurrentAngle = FMath::RadiansToDegrees(FMath::Atan2(Hit.Z - Center.Z, Hit.Y - Center.Y));
		}
		else
		{
			CurrentAngle = FMath::RadiansToDegrees(FMath::Atan2(Hit.Y - Center.Y, Hit.X - Center.X));
		}

		float DeltaAngle = FRotator::NormalizeAxis(CurrentAngle - DragStartAngleDeg) * RotationSensitivity;
		if (bSnapRotationToGrid)
		{
			DeltaAngle = FMath::GridSnap(DeltaAngle, 15.0f);
		}

		FRotator NewRot = DragStartFurnitureRot;
		if (CurrentDraggingAxis == EGizmoTransformAxis::RotatePitch)
		{
			const float RawAngle = FRotator::NormalizeAxis(NewRot.Pitch + DeltaAngle);
			NewRot.Pitch = ApplyCardinalSnap(NewRot.Pitch + DeltaAngle);
			DeltaAngle = FRotator::NormalizeAxis(NewRot.Pitch - DragStartFurnitureRot.Pitch);
			SetAxisRotationVisuals(CurrentDraggingAxisTag, DeltaAngle, !FMath::IsNearlyEqual(RawAngle, NewRot.Pitch, 0.01f));
		}
		else if (CurrentDraggingAxis == EGizmoTransformAxis::RotateRoll)
		{
			const float RawAngle = FRotator::NormalizeAxis(NewRot.Roll - DeltaAngle);
			NewRot.Roll = ApplyCardinalSnap(NewRot.Roll - DeltaAngle);
			DeltaAngle = FRotator::NormalizeAxis(DragStartFurnitureRot.Roll - NewRot.Roll);
			SetAxisRotationVisuals(CurrentDraggingAxisTag, DeltaAngle, !FMath::IsNearlyEqual(RawAngle, NewRot.Roll, 0.01f));
		}
		else
		{
			const float RawAngle = FRotator::NormalizeAxis(NewRot.Yaw + DeltaAngle);
			NewRot.Yaw = ApplyCardinalSnap(NewRot.Yaw + DeltaAngle);
			DeltaAngle = FRotator::NormalizeAxis(NewRot.Yaw - DragStartFurnitureRot.Yaw);
			SetAxisRotationVisuals(CurrentDraggingAxisTag, DeltaAngle, !FMath::IsNearlyEqual(RawAngle, NewRot.Yaw, 0.01f));
		}

		CurrentRotationDeltaDegrees = DeltaAngle;
		Target->SetActorRotation(NewRot);
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

void AInteRealGizmoActor::UpdateConstantScreenSize(const FVector& CameraLocation, float CameraFOVDegrees, float ScaleMultiplier)
{
	const float Distance = FVector::Dist(CameraLocation, GetActorLocation());
	
	const float FOVScale = FMath::Tan(FMath::DegreesToRadians(CameraFOVDegrees * 0.5f));
	const float Scale = FMath::Clamp(Distance * FOVScale * ScaleMultiplier / ReferenceDistance, MinScreenScale, MaxScreenScale);
	SetActorScale3D(FVector(Scale));
}

void AInteRealGizmoActor::EndDrag()
{
	if (!CurrentDraggingAxisTag.IsEmpty())
	{
		SetAxisOpacity(CurrentDraggingAxisTag, DefaultOpacity);
	}
	ResetRotationVisuals();
	bIsDragging = false;
	CurrentDraggingAxis = EGizmoTransformAxis::None;
	CurrentDraggingAxisTag.Empty();
	CurrentRotationDeltaDegrees = 0.0f;
}
