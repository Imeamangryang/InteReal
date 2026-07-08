#include "InteRealGizmoComponent.h"

#include "InteReal/EditMode/Furniture/Furniture.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	/* 기즈모 포스트프로세스 아웃라인 로직 비활성화 - 선택 시 화면 깜빡임 원인 중 하나
	static void SetComponentGizmoOutlineColor(UWorld* World, FLinearColor Color)
	{
		if (!World)
		{
			return;
		}

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
	*/

	static FInteRealGizmoVisualPart MakeGizmoPart(const TCHAR* AxisTag,
		const TCHAR* MeshPath,
		const TCHAR* MaterialPath,
		const FVector& Location,
		const FRotator& Rotation,
		const FVector& Scale)
	{
		FInteRealGizmoVisualPart Part;
		Part.AxisTag = FName(AxisTag);
		Part.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshPath));
		Part.Material = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(MaterialPath));
		Part.RelativeTransform = FTransform(Rotation, Location, Scale);
		return Part;
	}
	
	
	static void ConfigureAlwaysOnTopGizmoMesh(UStaticMeshComponent* MeshComponent)
	{
		if (!MeshComponent)
		{
			return;
		}

		MeshComponent->SetCastShadow(false);
		MeshComponent->bCastDynamicShadow = false;
		MeshComponent->bCastStaticShadow = false;
		MeshComponent->bReceivesDecals = false;
		MeshComponent->SetRenderInMainPass(true);
		MeshComponent->SetRenderCustomDepth(false);
		MeshComponent->SetTranslucentSortPriority(9999);
		MeshComponent->SetBoundsScale(10.0f);
		MeshComponent->MarkRenderStateDirty();
	}
}

UInteRealGizmoComponent::UInteRealGizmoComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetUsingAbsoluteRotation(true);

	const TCHAR* ArrowMesh = TEXT("/Game/EditMode/Resources/GizmoArrowHandle.GizmoArrowHandle");
	const TCHAR* RingMesh = TEXT("/Game/EditMode/Resources/GizmoFullCircleHandle.GizmoFullCircleHandle");
	VisualParts = {
		MakeGizmoPart(TEXT("MoveX"), ArrowMesh, TEXT("/Game/EditMode/Materials/M_Gizmo_X_Inst.M_Gizmo_X_Inst"), FVector::ZeroVector, FRotator::ZeroRotator, FVector(3.0f)),
		MakeGizmoPart(TEXT("MoveY"), ArrowMesh, TEXT("/Game/EditMode/Materials/M_Gizmo_Y_Inst.M_Gizmo_Y_Inst"), FVector::ZeroVector, FRotator(0.0f, 90.0f, 0.0f), FVector(3.0f)),
		MakeGizmoPart(TEXT("MoveZ"), ArrowMesh, TEXT("/Game/EditMode/Materials/M_Gizmo_Z_Inst.M_Gizmo_Z_Inst"), FVector::ZeroVector, FRotator(90.0f, 0.0f, 0.0f), FVector(3.0f)),
		MakeGizmoPart(TEXT("RotateYaw"), RingMesh, TEXT("/Game/EditMode/Materials/M_Gizmo_Rot_Inst.M_Gizmo_Rot_Inst"), FVector::ZeroVector, FRotator(90.0f, 0.0f, 0.0f), FVector(1.2f)),
		MakeGizmoPart(TEXT("RotatePitch"), RingMesh, TEXT("/Game/EditMode/Materials/M_Gizmo_Rot_Pitch_Inst.M_Gizmo_Rot_Pitch_Inst"), FVector::ZeroVector, FRotator(0.0f, 90.0f, 0.0f), FVector(1.2f)),
		MakeGizmoPart(TEXT("RotateRoll"), RingMesh, TEXT("/Game/EditMode/Materials/M_Gizmo_Rot_Roll_Inst.M_Gizmo_Rot_Roll_Inst"), FVector::ZeroVector, FRotator::ZeroRotator, FVector(1.2f))
	};
}

void UInteRealGizmoComponent::OnRegister()
{
	Super::OnRegister();
	BuildGeneratedVisuals();
	UpdateAnchorFromOwner();
	SetSelectedActive(false);
}

void UInteRealGizmoComponent::OnUnregister()
{
	DestroyGeneratedVisuals();
	Super::OnUnregister();
}

void UInteRealGizmoComponent::BuildGeneratedVisuals()
{
	if (bVisualsBuilt || IsTemplate())
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	for (const FInteRealGizmoVisualPart& Part : VisualParts)
	{
		if (Part.AxisTag.IsNone())
		{
			continue;
		}

		UStaticMesh* Mesh = Part.Mesh.LoadSynchronous();
		if (!Mesh)
		{
			continue;
		}

		const FName ComponentName(*FString::Printf(TEXT("Gizmo_%s"), *Part.AxisTag.ToString()));
		UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(Owner, ComponentName);
		if (!MeshComponent)
		{
			continue;
		}

		MeshComponent->SetStaticMesh(Mesh);
		// 기즈모는 반투명 머티리얼을 사용하므로 Nanite 렌더링 대상에서 제외한다.
		// Nanite + Translucent 조합에서 발생하는 경고와 클릭/호버 깜빡임을 방지한다.
		MeshComponent->bDisallowNanite = true;
		if (UMaterialInterface* Material = Part.Material.LoadSynchronous())
		{
			MeshComponent->SetMaterial(0, Material);
		}
		MeshComponent->ComponentTags.AddUnique(Part.AxisTag);
		MeshComponent->SetupAttachment(this);
		MeshComponent->SetRelativeTransform(Part.RelativeTransform);
		MeshComponent->bReceivesDecals = false;
		ConfigureAlwaysOnTopGizmoMesh(MeshComponent);
		ConfigureGizmoCollision(MeshComponent, false);
		MeshComponent->SetVisibility(false, true);
		MeshComponent->SetHiddenInGame(true, true);

		Owner->AddInstanceComponent(MeshComponent);
		MeshComponent->RegisterComponent();
		GeneratedMeshes.Add(MeshComponent);
	}

	bVisualsBuilt = true;
	InitAxisMaterials();
}

void UInteRealGizmoComponent::DestroyGeneratedVisuals()
{
	for (UStaticMeshComponent* Mesh : GeneratedMeshes)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	GeneratedMeshes.Reset();
	AxisMaterials.Reset();
	AxisMeshes.Reset();
	OriginalAxisColors.Reset();
	HoveredAxis.Empty();
	bVisualsBuilt = false;
}

void UInteRealGizmoComponent::ConfigureGizmoCollision(UPrimitiveComponent* Component, bool bEnabled) const
{
	if (!Component)
	{
		return;
	}

	const ECollisionChannel GizmoTraceChannel = ECC_GameTraceChannel1;

	Component->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	Component->SetCollisionObjectType(ECC_WorldDynamic);
	Component->SetCollisionResponseToAllChannels(ECR_Ignore);
	Component->SetCollisionResponseToChannel(ECC_Visibility, bEnabled ? ECR_Block : ECR_Ignore);
	Component->SetCollisionResponseToChannel(GizmoTraceChannel, bEnabled ? ECR_Block : ECR_Ignore);
	Component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}

void UInteRealGizmoComponent::SetSelectedActive(bool bActive)
{
	bSelectedActive = bActive;
	if (!bSelectedActive)
	{
		EndDrag();
	}
	SetDisplayMode(DisplayMode);
}

void UInteRealGizmoComponent::SetGizmoHidden(bool bHidden)
{
	bIsGizmoHidden = bHidden;
	SetDisplayMode(DisplayMode);
}

void UInteRealGizmoComponent::SetShowMove(bool bShow)
{
	SetDisplayMode(bShow
		? (IsShowingRotate() ? EInteRealGizmoDisplayMode::All : EInteRealGizmoDisplayMode::Move)
		: (IsShowingRotate() ? EInteRealGizmoDisplayMode::Rotation : EInteRealGizmoDisplayMode::None));
}

void UInteRealGizmoComponent::SetShowRotate(bool bShow)
{
	SetDisplayMode(bShow
		? (IsShowingMove() ? EInteRealGizmoDisplayMode::All : EInteRealGizmoDisplayMode::Rotation)
		: (IsShowingMove() ? EInteRealGizmoDisplayMode::Move : EInteRealGizmoDisplayMode::None));
}

bool UInteRealGizmoComponent::IsShowingMove() const
{
	return DisplayMode == EInteRealGizmoDisplayMode::All || DisplayMode == EInteRealGizmoDisplayMode::Move;
}

bool UInteRealGizmoComponent::IsShowingRotate() const
{
	return DisplayMode == EInteRealGizmoDisplayMode::All || DisplayMode == EInteRealGizmoDisplayMode::Rotation;
}

void UInteRealGizmoComponent::SetDisplayMode(EInteRealGizmoDisplayMode NewMode)
{
	DisplayMode = NewMode;
	HoveredAxis.Empty();
	ResetRotationVisuals();

	for (UStaticMeshComponent* Mesh : GeneratedMeshes)
	{
		if (!Mesh)
		{
			continue;
		}

		const FString AxisTag = GetAxisTagFromComponent(Mesh);
		const bool bIsRotationComponent = IsRotationAxisVisible(AxisTag);
		bool bShouldShow = bSelectedActive && !bIsGizmoHidden;
		if (bShouldShow)
		{
			switch (DisplayMode)
			{
			case EInteRealGizmoDisplayMode::All:
				bShouldShow = true;
				break;
			case EInteRealGizmoDisplayMode::Move:
				bShouldShow = !bIsRotationComponent;
				break;
			case EInteRealGizmoDisplayMode::Rotation:
				bShouldShow = bIsRotationComponent;
				break;
			case EInteRealGizmoDisplayMode::None:
			default:
				bShouldShow = false;
				break;
			}
		}

		ConfigureAlwaysOnTopGizmoMesh(Mesh);
		Mesh->SetVisibility(bShouldShow, true);
		Mesh->SetHiddenInGame(!bShouldShow, true);
		ConfigureGizmoCollision(Mesh, bShouldShow);
	}

	if (bIsDragging)
	{
		const bool bDraggingMove = CurrentDraggingAxisTag.StartsWith(TEXT("Move"));
		const bool bDraggingRotate = IsRotationAxisVisible(CurrentDraggingAxisTag);
		if ((bDraggingMove && !IsShowingMove()) || (bDraggingRotate && !IsShowingRotate()))
		{
			EndDrag();
		}
	}
}

FString UInteRealGizmoComponent::GetAxisTagFromComponent(const UPrimitiveComponent* Component)
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
	if (Name.Contains(TEXT("MoveX")) || Name.StartsWith(TEXT("ArrowX")))
	{
		return TEXT("MoveX");
	}
	if (Name.Contains(TEXT("MoveY")) || Name.StartsWith(TEXT("ArrowY")))
	{
		return TEXT("MoveY");
	}
	if (Name.Contains(TEXT("MoveZ")) || Name.StartsWith(TEXT("ArrowZ")))
	{
		return TEXT("MoveZ");
	}
	if (Name.Contains(TEXT("RotatePitch")) || Name.StartsWith(TEXT("RingPitch")))
	{
		return TEXT("RotatePitch");
	}
	if (Name.Contains(TEXT("RotateRoll")) || Name.StartsWith(TEXT("RingRoll")))
	{
		return TEXT("RotateRoll");
	}
	if (Name.Contains(TEXT("RotateYaw")) || Name.StartsWith(TEXT("RingYaw")) || Name.StartsWith(TEXT("RotationRing")))
	{
		return TEXT("RotateYaw");
	}
	return FString();
}

bool UInteRealGizmoComponent::OwnsGizmoComponent(const UPrimitiveComponent* Component) const
{
	if (!Component)
	{
		return false;
	}
	for (const UStaticMeshComponent* Mesh : GeneratedMeshes)
	{
		if (Mesh == Component)
		{
			return true;
		}
	}
	return false;
}

FString UInteRealGizmoComponent::GetAxisTagFromHit(const FHitResult& CursorHit) const
{
	UPrimitiveComponent* HitComp = CursorHit.GetComponent();
	if (!OwnsGizmoComponent(HitComp))
	{
		return FString();
	}
	return GetAxisTagFromComponent(HitComp);
}

void UInteRealGizmoComponent::InitAxisMaterials()
{
	AxisMaterials.Empty();
	AxisMeshes.Empty();
	OriginalAxisColors.Empty();

	for (UStaticMeshComponent* Mesh : GeneratedMeshes)
	{
		const FString AxisTag = GetAxisTagFromComponent(Mesh);
		if (!Mesh || AxisTag.IsEmpty())
		{
			continue;
		}

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
}

void UInteRealGizmoComponent::UpdateAnchorFromOwner()
{
	SetWorldLocation(GetAnchorLocation());
	SetWorldRotation(FRotator::ZeroRotator);
}

FVector UInteRealGizmoComponent::GetAnchorLocation() const
{
	const AFurniture* Furniture = Cast<AFurniture>(GetOwner());
	if (!Furniture)
	{
		return GetComponentLocation();
	}

	const FBox VisualBounds = Furniture->GetVisualBounds();
	return VisualBounds.IsValid ? VisualBounds.GetCenter() : Furniture->GetActorLocation();
}

AFurniture* UInteRealGizmoComponent::GetOwnerFurniture() const
{
	return Cast<AFurniture>(GetOwner());
}

bool UInteRealGizmoComponent::IsMoveAxisVisible(const FString& Axis) const
{
	return Axis.StartsWith(TEXT("Move"));
}

bool UInteRealGizmoComponent::IsRotationAxisVisible(const FString& Axis) const
{
	return Axis.StartsWith(TEXT("Rotate")) || Axis == TEXT("RotationRing");
}

void UInteRealGizmoComponent::SetAxisOutline(const FString& Axis, bool bEnable)
{
	// 기즈모 Hover 피드백은 머티리얼 색상 변경만 사용한다.
	// Custom Depth/Stencil을 매 Hover 전환마다 갱신하면 Pixel Streaming에서
	// 렌더 상태가 반복 생성되며 깜빡임을 유발할 수 있어 비활성화했다.
	SetAxisColorHighlight(Axis, bEnable);
}

void UInteRealGizmoComponent::SetAxisColorHighlight(const FString& Axis, bool bEnable)
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

FLinearColor UInteRealGizmoComponent::GetBaseAxisColor(const FString& Axis) const
{
	if (const FLinearColor* OriginalColor = OriginalAxisColors.Find(Axis))
	{
		return *OriginalColor;
	}
	return FLinearColor::White;
}

void UInteRealGizmoComponent::SetAxisRotationVisuals(const FString& Axis, float DeltaAngle, bool bSnapped)
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

void UInteRealGizmoComponent::ResetRotationVisuals()
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

void UInteRealGizmoComponent::UpdateHover(bool bIsHitting, const FHitResult& CursorHit)
{
	if (!bSelectedActive || AxisMaterials.Num() == 0)
	{
		return;
	}

	const FString NewHoveredAxis = bIsHitting ? GetAxisTagFromHit(CursorHit) : FString();
	if (NewHoveredAxis == HoveredAxis)
	{
		return;
	}

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

EGizmoTransformAxis UInteRealGizmoComponent::ParseAxisTag(const FString& AxisTag) const
{
	if (AxisTag == TEXT("MoveX")) return EGizmoTransformAxis::MoveX;
	if (AxisTag == TEXT("MoveY")) return EGizmoTransformAxis::MoveY;
	if (AxisTag == TEXT("MoveZ")) return EGizmoTransformAxis::MoveZ;
	if (AxisTag == TEXT("RotatePitch")) return EGizmoTransformAxis::RotatePitch;
	if (AxisTag == TEXT("RotateRoll")) return EGizmoTransformAxis::RotateRoll;
	if (AxisTag.StartsWith(TEXT("Rotate")) || AxisTag == TEXT("RotationRing")) return EGizmoTransformAxis::RotateYaw;
	return EGizmoTransformAxis::None;
}

float UInteRealGizmoComponent::ApplyCardinalSnap(float AngleDegrees) const
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

bool UInteRealGizmoComponent::ComputeRotationPlaneAngle(const FVector& WorldOrigin, const FVector& WorldDir, float& OutAngleDeg) const
{
	const float Denom = FVector::DotProduct(WorldDir, RotationAxisWorld);
	if (FMath::IsNearlyZero(Denom, 0.001f))
	{
		return false;
	}

	const FVector PointOnPlane = FMath::LinePlaneIntersection(WorldOrigin,
		WorldOrigin + WorldDir * 100000.f,
		FPlane(RotationPivotWorld, RotationAxisWorld));
	const FVector Offset = PointOnPlane - RotationPivotWorld;
	const float U = FVector::DotProduct(Offset, RotationBasisU);
	const float V = FVector::DotProduct(Offset, RotationBasisV);
	if (FMath::IsNearlyZero(U) && FMath::IsNearlyZero(V))
	{
		return false;
	}

	OutAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(V, U));
	return true;
}

bool UInteRealGizmoComponent::BeginDrag(const FString& Axis, const FVector& WorldOrigin, const FVector& WorldDir, const FVector2D& MousePos)
{
	AFurniture* Target = GetOwnerFurniture();
	if (!Target || Axis.IsEmpty())
	{
		return false;
	}

	CurrentDraggingAxisTag = Axis;
	CurrentDraggingAxis = ParseAxisTag(Axis);
	if (CurrentDraggingAxis == EGizmoTransformAxis::None)
	{
		return false;
	}

	bIsDragging = true;
	DragStartMousePos = MousePos;
	CurrentRotationDeltaDegrees = 0.0f;
	SetAxisOutline(Axis, true);

	if (CurrentDraggingAxis == EGizmoTransformAxis::RotateYaw ||
		CurrentDraggingAxis == EGizmoTransformAxis::RotatePitch ||
		CurrentDraggingAxis == EGizmoTransformAxis::RotateRoll)
	{
		DragStartFurnitureRot = Target->GetActorRotation();
		RotationPivotWorld = Target->GetVisualBounds().GetCenter();

		const FVector LocalX = DragStartFurnitureRot.RotateVector(FVector::ForwardVector);
		const FVector LocalY = DragStartFurnitureRot.RotateVector(FVector::RightVector);
		const FVector LocalZ = DragStartFurnitureRot.RotateVector(FVector::UpVector);
		if (CurrentDraggingAxis == EGizmoTransformAxis::RotatePitch)
		{
			RotationAxisWorld = LocalY;
			RotationBasisU = LocalZ;
			RotationBasisV = FVector::CrossProduct(RotationBasisU, RotationAxisWorld);
		}
		else if (CurrentDraggingAxis == EGizmoTransformAxis::RotateRoll)
		{
			RotationAxisWorld = LocalX;
			RotationBasisU = LocalY;
			RotationBasisV = FVector::CrossProduct(RotationBasisU, RotationAxisWorld);
		}
		else
		{
			RotationAxisWorld = LocalZ;
			RotationBasisU = LocalX;
			RotationBasisV = FVector::CrossProduct(RotationAxisWorld, RotationBasisU);
		}
		if (!ComputeRotationPlaneAngle(WorldOrigin, WorldDir, DragStartAngleDeg))
		{
			DragStartAngleDeg = 0.0f;
		}
		LastRotationMouseAngleDeg = DragStartAngleDeg;
		AccumulatedRotationDeltaDegrees = 0.0f;
	}
	else
	{
		DragStartLocation = Target->GetActorLocation();
		FVector PlaneAnchor = DragStartLocation;
		if ((CurrentDraggingAxis == EGizmoTransformAxis::MoveX || CurrentDraggingAxis == EGizmoTransformAxis::MoveY) &&
			Target->GetPlacedSurfaceType() == EPlacementSurfaceType::Floor)
		{
			PlaneAnchor = Target->GetMeshBounds().GetCenter();
		}

		FVector PlaneNormal = FVector::UpVector;
		if (Target->GetPlacedSurfaceType() == EPlacementSurfaceType::Wall && !Target->WallNormalAtPlacement.IsNearlyZero())
		{
			PlaneNormal = Target->WallNormalAtPlacement;
		}

		const FPlane DragPlane(PlaneAnchor, PlaneNormal);
		DragStartLocation = PlaneAnchor;
		// 카메라 시선이 드래그 평면과 거의 평행하면(분모가 아주 작으면) 교차점이 극단적으로
		// 튈 수 있어(클릭 순간 깜빡임의 원인) 이번 프레임은 계산하지 않고 오프셋 0으로 시작한다.
		if (FMath::Abs(FVector::DotProduct(WorldDir, PlaneNormal)) >= 0.1f)
		{
			const FVector CursorOnPlane = FMath::LinePlaneIntersection(WorldOrigin, WorldOrigin + WorldDir * 100000.f, DragPlane);
			DragCursorOffset = CursorOnPlane - DragStartLocation;
		}
		else
		{
			DragCursorOffset = FVector::ZeroVector;
		}
	}
	return true;
}

bool UInteRealGizmoComponent::UpdateDrag(const FVector& WorldOrigin, const FVector& WorldDir,
	UInteriorPlacementSubsystem* PlacementSubsystem, bool bSnapRotationToGrid, const FVector2D& MousePos)
{
	AFurniture* Target = GetOwnerFurniture();
	if (!bIsDragging || !Target)
	{
		return false;
	}

	const FVector BeforeLocation = Target->GetActorLocation();
	const FRotator BeforeRotation = Target->GetActorRotation();

	if (CurrentDraggingAxis == EGizmoTransformAxis::RotateYaw ||
		CurrentDraggingAxis == EGizmoTransformAxis::RotatePitch ||
		CurrentDraggingAxis == EGizmoTransformAxis::RotateRoll)
	{
		float CurrentAngle = LastRotationMouseAngleDeg;
		if (ComputeRotationPlaneAngle(WorldOrigin, WorldDir, CurrentAngle))
		{
			const float FrameDelta = FRotator::NormalizeAxis(CurrentAngle - LastRotationMouseAngleDeg);
			AccumulatedRotationDeltaDegrees += FrameDelta;
			LastRotationMouseAngleDeg = CurrentAngle;
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
			NewRot.Pitch = bSnapRotationToGrid ? ApplyCardinalSnap(NewRot.Pitch + DeltaAngle) : RawAngle;
			SetAxisRotationVisuals(CurrentDraggingAxisTag, CurrentRotationDeltaDegrees, !FMath::IsNearlyEqual(RawAngle, NewRot.Pitch, 0.01f));
		}
		else if (CurrentDraggingAxis == EGizmoTransformAxis::RotateRoll)
		{
			const float RawAngle = FRotator::NormalizeAxis(NewRot.Roll + DeltaAngle);
			NewRot.Roll = bSnapRotationToGrid ? ApplyCardinalSnap(NewRot.Roll + DeltaAngle) : RawAngle;
			SetAxisRotationVisuals(CurrentDraggingAxisTag, CurrentRotationDeltaDegrees, !FMath::IsNearlyEqual(RawAngle, NewRot.Roll, 0.01f));
		}
		else
		{
			const float RawAngle = FRotator::NormalizeAxis(NewRot.Yaw + DeltaAngle);
			NewRot.Yaw = bSnapRotationToGrid ? ApplyCardinalSnap(NewRot.Yaw + DeltaAngle) : RawAngle;
			SetAxisRotationVisuals(CurrentDraggingAxisTag, CurrentRotationDeltaDegrees, !FMath::IsNearlyEqual(RawAngle, NewRot.Yaw, 0.01f));
		}
		Target->SetRotationPreservingPlacement(NewRot);
		UpdateAnchorFromOwner();
		return !BeforeLocation.Equals(Target->GetActorLocation(), 0.01f) || !BeforeRotation.Equals(Target->GetActorRotation(), 0.01f);
	}

	if ((CurrentDraggingAxis == EGizmoTransformAxis::MoveX || CurrentDraggingAxis == EGizmoTransformAxis::MoveY) && PlacementSubsystem)
	{
		FVector PlaneNormal = FVector::UpVector;
		if (Target->GetPlacedSurfaceType() == EPlacementSurfaceType::Wall && !Target->WallNormalAtPlacement.IsNearlyZero())
		{
			PlaneNormal = Target->WallNormalAtPlacement;
		}

		const FPlane DragPlane(DragStartLocation, PlaneNormal);
		// 이번 프레임 카메라 시선이 드래그 평면과 거의 평행하면 교차점이 불안정해지므로
		// (클릭/드래그 중 깜빡임의 원인) 위치를 갱신하지 않고 이전 프레임 상태를 유지한다.
		if (FMath::Abs(FVector::DotProduct(WorldDir, PlaneNormal)) < 0.1f)
		{
			return false;
		}
		const FVector CursorOnPlane = FMath::LinePlaneIntersection(WorldOrigin, WorldOrigin + WorldDir * 100000.f, DragPlane);
		FVector TargetPoint = CursorOnPlane - DragCursorOffset;
		if (CurrentDraggingAxis == EGizmoTransformAxis::MoveX)
		{
			TargetPoint.Y = DragStartLocation.Y;
		}
		else
		{
			TargetPoint.X = DragStartLocation.X;
		}
		PlacementSubsystem->UpdateGizmoMoveLocation(TargetPoint, Target, CurrentDraggingAxis);
		UpdateAnchorFromOwner();
		return !BeforeLocation.Equals(Target->GetActorLocation(), 0.01f);
	}

	if (CurrentDraggingAxis == EGizmoTransformAxis::MoveZ)
	{
		const float Distance = FVector::Dist(WorldOrigin, DragStartLocation);
		const float DeltaY = DragStartMousePos.Y - MousePos.Y;
		FVector NewLoc = Target->GetActorLocation();
		NewLoc.Z = BeforeLocation.Z;
		NewLoc.Z = DragStartLocation.Z + DeltaY * ZDragSensitivity * (Distance / ReferenceDistance);

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
		UpdateAnchorFromOwner();
		return !BeforeLocation.Equals(Target->GetActorLocation(), 0.01f);
	}

	return false;
}

void UInteRealGizmoComponent::EndDrag()
{
	if (!CurrentDraggingAxisTag.IsEmpty())
	{
		SetAxisOutline(CurrentDraggingAxisTag, false);
	}
	ResetRotationVisuals();
	bIsDragging = false;
	CurrentDraggingAxis = EGizmoTransformAxis::None;
	CurrentDraggingAxisTag.Empty();
	HoveredAxis.Empty();
	CurrentRotationDeltaDegrees = 0.0f;
}

void UInteRealGizmoComponent::UpdateConstantScreenSize(APlayerController* PlayerController, float ScaleMultiplier)
{
	if (!PlayerController || !PlayerController->PlayerCameraManager)
	{
		return;
	}

	UpdateAnchorFromOwner();
	const FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
	const float CameraFOVDegrees = PlayerController->PlayerCameraManager->GetFOVAngle();
	const float Distance = FVector::Dist(CameraLocation, GetComponentLocation());
	const float FOVScale = FMath::Tan(FMath::DegreesToRadians(CameraFOVDegrees * 0.5f));
	const float Scale = FMath::Clamp(
		Distance * FOVScale * ScaleMultiplier * ScreenSizeScale / ReferenceDistance,
		MinScreenScale,
		MaxScreenScale);

	// Never feed generated primitive world bounds back into this calculation: they
	// can still contain the previous frame's scale and cause two-frame oscillation.
	if (!GetComponentScale().Equals(FVector(Scale), KINDA_SMALL_NUMBER))
	{
		SetWorldScale3D(FVector(Scale));
	}
}

FBox UInteRealGizmoComponent::GetVisibleGizmoBounds() const
{
	FBox VisibleBounds(EForceInit::ForceInit);
	for (const UStaticMeshComponent* Mesh : GeneratedMeshes)
	{
		if (Mesh && Mesh->IsVisible())
		{
			VisibleBounds += Mesh->Bounds.GetBox();
		}
	}
	return VisibleBounds;
}
