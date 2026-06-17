#include "InteRealGizmoActor.h"
#include "InteReal/EditMode/Furnitures/Furniture.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AInteRealGizmoActor::AInteRealGizmoActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AInteRealGizmoActor::InitAxisMaterials()
{
	AxisMaterials.Empty();
	HoveredAxis.Empty();

	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);

	for (UMeshComponent* Mesh : Meshes)
	{
		FString AxisTag;
		for (const FName& Tag : Mesh->ComponentTags)
		{
			const FString TagStr = Tag.ToString();
			if (TagStr.StartsWith(TEXT("Move")) || TagStr.StartsWith(TEXT("Rotate")) || TagStr == TEXT("RotationRing"))
			{
				AxisTag = TagStr;
				break;
			}
		}
		if (AxisTag.IsEmpty()) continue;

		TArray<TObjectPtr<UMaterialInstanceDynamic>>& DMIs = AxisMaterials.FindOrAdd(AxisTag);
		for (int32 i = 0; i < Mesh->GetNumMaterials(); i++)
		{
			if (UMaterialInstanceDynamic* DMI = Mesh->CreateAndSetMaterialInstanceDynamic(i))
			{
				DMI->SetScalarParameterValue(OpacityParamName, DefaultOpacity);
				DMIs.Add(DMI);
			}
		}
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

FString AInteRealGizmoActor::GetAxisTagFromHit(const FHitResult& CursorHit) const
{
	UPrimitiveComponent* HitComp = CursorHit.GetComponent();
	if (!HitComp || HitComp->GetOwner() != this) return FString();

	for (const FName& Tag : HitComp->ComponentTags)
	{
		const FString TagStr = Tag.ToString();
		if (AxisMaterials.Contains(TagStr))
		{
			return TagStr;
		}
	}
	return FString();
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
			FPlane GroundPlane(DragStartLocation, FVector::UpVector);
			FVector CursorOnGround = FMath::LinePlaneIntersection(WorldOrigin,
			                                                      WorldOrigin + WorldDir * 100000.f,
			                                                      GroundPlane);
			DragCursorOffset = CursorOnGround - DragStartLocation;
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
			NewRot.Pitch = FRotator::NormalizeAxis(NewRot.Pitch + DeltaAngle);
		}
		else if (CurrentDraggingAxis == EGizmoTransformAxis::RotateRoll)
		{
			NewRot.Roll = FRotator::NormalizeAxis(NewRot.Roll - DeltaAngle);
		}
		else
		{
			NewRot.Yaw = FRotator::NormalizeAxis(NewRot.Yaw + DeltaAngle);
		}

		Target->SetActorRotation(NewRot);
		return;
	}

	// X / Y 이동 (그리드 스냅 + 충돌은 PlacementSubsystem이 처리)
	if ((CurrentDraggingAxis == EGizmoTransformAxis::MoveX || CurrentDraggingAxis == EGizmoTransformAxis::MoveY) && PlacementSubsystem)
	{
		FPlane GroundPlane(DragStartLocation, FVector::UpVector);
		FVector CursorOnGround = FMath::LinePlaneIntersection(WorldOrigin,
		                                                      WorldOrigin + WorldDir * 100000.f,
		                                                      GroundPlane);
		PlacementSubsystem->UpdateGizmoMoveLocation(CursorOnGround - DragCursorOffset, Target, CurrentDraggingAxis);
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
		Target->SetActorLocation(NewLoc);
		return;
	}
}

void AInteRealGizmoActor::UpdateConstantScreenSize(const FVector& CameraLocation, float CameraFOVDegrees)
{
	const float Distance = FVector::Dist(CameraLocation, GetActorLocation());
	
	const float FOVScale = FMath::Tan(FMath::DegreesToRadians(CameraFOVDegrees * 0.5f));
	const float Scale = Distance * FOVScale / ReferenceDistance;
	SetActorScale3D(FVector(Scale));
}

void AInteRealGizmoActor::EndDrag()
{
	if (!CurrentDraggingAxisTag.IsEmpty())
	{
		SetAxisOpacity(CurrentDraggingAxisTag, DefaultOpacity);
	}
	bIsDragging = false;
	CurrentDraggingAxis = EGizmoTransformAxis::None;
	CurrentDraggingAxisTag.Empty();
}
