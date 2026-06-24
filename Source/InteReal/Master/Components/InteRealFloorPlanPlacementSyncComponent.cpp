#include "InteRealFloorPlanPlacementSyncComponent.h"
#include "InteReal/Master/InteRealPlayerController.h"
#include "InteReal/Master/InteRealHUD.h"
#include "InteReal/EditMode/2D/InteReal2DFloorPlanViewportWidget.h"
#include "InteReal/EditMode/Furniture/Furniture.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"

UInteRealFloorPlanPlacementSyncComponent::UInteRealFloorPlanPlacementSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInteRealFloorPlanPlacementSyncComponent::RequestRebuildFloorPlan2DFromPlacedFurniture()
{
	RebuildFloorPlan2DFromPlacedFurniture();
}

void UInteRealFloorPlanPlacementSyncComponent::Initialize(AInteRealPlayerController* InOwnerController)
{
	OwnerController = InOwnerController;
}

AInteRealHUD* UInteRealFloorPlanPlacementSyncComponent::GetInteRealHUD() const
{
	return OwnerController ? Cast<AInteRealHUD>(OwnerController->GetHUD()) : nullptr;
}

UInteriorPlacementSubsystem* UInteRealFloorPlanPlacementSyncComponent::GetPlacementSubsystem() const
{
	return OwnerController && OwnerController->GetWorld() ? OwnerController->GetWorld()->GetSubsystem<UInteriorPlacementSubsystem>() : nullptr;
}

UInteReal2DFloorPlanViewportWidget* UInteRealFloorPlanPlacementSyncComponent::GetFloorPlan2DWidget() const
{
	AInteRealHUD* InteRealHUD = GetInteRealHUD();
	return InteRealHUD ? InteRealHUD->GetFloorPlan2DWidget() : nullptr;
}

bool UInteRealFloorPlanPlacementSyncComponent::IsEditMode() const
{
	return OwnerController && OwnerController->GetControlMode() == EInteRealControlMode::Edit;
}

void UInteRealFloorPlanPlacementSyncComponent::SetSyncSource(EInteRealFloorPlanSyncSource NewSource)
{
	CurrentSyncSource = NewSource;
}

void UInteRealFloorPlanPlacementSyncComponent::ClearSyncSource(EInteRealFloorPlanSyncSource ExpectedSource)
{
	if (CurrentSyncSource == ExpectedSource)
	{
		CurrentSyncSource = EInteRealFloorPlanSyncSource::None;
	}
}

void UInteRealFloorPlanPlacementSyncComponent::SetDeletingFrom3D(bool bDeleting)
{
	CurrentSyncSource = bDeleting ? EInteRealFloorPlanSyncSource::DeleteFrom3D : EInteRealFloorPlanSyncSource::None;
}

bool UInteRealFloorPlanPlacementSyncComponent::IsSyncingFurniture3DFrom2D() const
{
	return CurrentSyncSource == EInteRealFloorPlanSyncSource::From2D;
}

bool UInteRealFloorPlanPlacementSyncComponent::IsSyncingFloorPlan2DFrom3D() const
{
	return CurrentSyncSource == EInteRealFloorPlanSyncSource::From3D || CurrentSyncSource == EInteRealFloorPlanSyncSource::Rebuild;
}

void UInteRealFloorPlanPlacementSyncComponent::SelectFloorPlan2DForFurniture(AFurniture* FurnitureActor)
{
	if (!IsValid(FurnitureActor) || CurrentSyncSource == EInteRealFloorPlanSyncSource::From2D)
	{
		return;
	}

	FGuid InstanceGuid;
	if (!FindFloorPlan2DGuidForFurniture(FurnitureActor, InstanceGuid))
	{
		return;
	}

	UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = GetFloorPlan2DWidget();
	if (!FloorPlan2DWidget)
	{
		return;
	}

	SetSyncSource(EInteRealFloorPlanSyncSource::From3D);
	FloorPlan2DWidget->SelectPlacedFurnitureByGuid(InstanceGuid);
	ClearSyncSource(EInteRealFloorPlanSyncSource::From3D);
}

void UInteRealFloorPlanPlacementSyncComponent::BindFloorPlan2DEvents()
{
	UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = GetFloorPlan2DWidget();
	if (!FloorPlan2DWidget)
	{
		return;
	}

	FloorPlan2DWidget->OnFurniturePlacementRequested2D.AddUniqueDynamic(this, &UInteRealFloorPlanPlacementSyncComponent::HandleFloorPlan2DFurniturePlacementRequested);
	FloorPlan2DWidget->OnFurniturePreviewMoved2D.AddUniqueDynamic(this, &UInteRealFloorPlanPlacementSyncComponent::HandleFloorPlan2DFurniturePreviewMoved);
	FloorPlan2DWidget->OnPlacedFurnitureSelected2D.AddUniqueDynamic(this, &UInteRealFloorPlanPlacementSyncComponent::HandleFloorPlan2DPlacedFurnitureSelected);
	FloorPlan2DWidget->OnPlacedFurnitureMoved2D.AddUniqueDynamic(this, &UInteRealFloorPlanPlacementSyncComponent::HandleFloorPlan2DPlacedFurnitureMoved);
	FloorPlan2DWidget->OnPlacedFurnitureMoveEnded2D.AddUniqueDynamic(this, &UInteRealFloorPlanPlacementSyncComponent::HandleFloorPlan2DPlacedFurnitureMoveEnded);
	FloorPlan2DWidget->OnPlacedFurnitureDeleted2D.AddUniqueDynamic(this, &UInteRealFloorPlanPlacementSyncComponent::HandleFloorPlan2DPlacedFurnitureDeleted);
	FloorPlan2DWidget->OnPlacedFurnituresCleared2D.AddUniqueDynamic(this, &UInteRealFloorPlanPlacementSyncComponent::HandleFloorPlan2DPlacedFurnituresCleared);
	FloorPlan2DWidget->OnPlacedFurnitureSelectionCleared2D.AddUniqueDynamic(this, &UInteRealFloorPlanPlacementSyncComponent::HandleFloorPlan2DPlacedFurnitureSelectionCleared);
}

void UInteRealFloorPlanPlacementSyncComponent::RegisterFloorPlan2DFurnitureActor(const FGuid& FloorPlanFurnitureGuid, AFurniture* FurnitureActor)
{
	if (!FloorPlanFurnitureGuid.IsValid() || !IsValid(FurnitureActor))
	{
		return;
	}

	FloorPlan2DFurnitureActors.Add(FloorPlanFurnitureGuid, FurnitureActor);
}

bool UInteRealFloorPlanPlacementSyncComponent::FindFloorPlan2DGuidForFurniture(const AFurniture* FurnitureActor, FGuid& OutInstanceGuid) const
{
	OutInstanceGuid.Invalidate();

	if (!IsValid(FurnitureActor))
	{
		return false;
	}

	for (const TPair<FGuid, TWeakObjectPtr<AFurniture>>& Pair : FloorPlan2DFurnitureActors)
	{
		if (Pair.Value.Get() == FurnitureActor)
		{
			OutInstanceGuid = Pair.Key;
			return true;
		}
	}

	return false;
}

void UInteRealFloorPlanPlacementSyncComponent::RegisterConfirmedFurnitureToFloorPlan(const FFurnitureDataRow& FurnitureRow, const FVector& ConfirmedWorldLocation, float ConfirmedYaw, AFurniture* ConfirmedFurniture)
{
	UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = GetFloorPlan2DWidget();
	if (!FloorPlan2DWidget)
	{
		return;
	}

	BindFloorPlan2DEvents();

	if (!IsValid(ConfirmedFurniture))
	{
		RebuildFloorPlan2DFromPlacedFurniture();
		return;
	}

	const FGuid AddedFloorPlanGuid = FloorPlan2DWidget->AddPlacedFurnitureAtDocumentPosition(FurnitureRow, FVector2D(ConfirmedWorldLocation.X, ConfirmedWorldLocation.Y), ConfirmedYaw);
	RegisterFloorPlan2DFurnitureActor(AddedFloorPlanGuid, ConfirmedFurniture);
}

void UInteRealFloorPlanPlacementSyncComponent::HandleFloorPlan2DFurniturePlacementRequested(FVector2D DocumentPosition)
{
	if (!OwnerController || !IsEditMode())
	{
		return;
	}

    UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
    if (!PS || !PS->HasActivePreview())
    {
        return;
    }

    AFurniture* PreviewFurniture = PS->GetPreviewFurniture();
    if (!PreviewFurniture)
    {
        return;
    }

    const int32 FurnitureID = PreviewFurniture->FurnitureID;
    const FFurnitureDataRow* FurnitureRow = PS->FindFurnitureRowByID(FurnitureID);
    if (!FurnitureRow)
    {
        return;
    }

    const float FloorZ = PreviewFurniture->GetActorLocation().Z;
    const FVector RequestedWorldLocation(DocumentPosition.X, DocumentPosition.Y, FloorZ);

    FHitResult FloorHit;
    FloorHit.bBlockingHit = true;
    FloorHit.Location = RequestedWorldLocation;
    FloorHit.ImpactPoint = RequestedWorldLocation;
    FloorHit.ImpactNormal = FVector::UpVector;
    FloorHit.Normal = FVector::UpVector;

    PS->UpdatePreviewLocation(FloorHit);

    PreviewFurniture = PS->GetPreviewFurniture();
    if (!PreviewFurniture)
    {
        return;
    }

    if (PS->InvalidReason != EPlacementInvalidReason::None)
    {
        return;
    }

    const FVector ConfirmedWorldLocation = PreviewFurniture->GetActorLocation();
    const float ConfirmedYaw = PreviewFurniture->GetActorRotation().Yaw;

    TSet<TObjectKey<AFurniture>> PreviouslyPlacedFurnitureKeys;
	OwnerController->SnapshotPlacedFurnitureActorsForFloorPlanSync(PreviouslyPlacedFurnitureKeys);

    AFurniture* PreviewFurnitureBeforeConfirm = PreviewFurniture;

    PS->ConfirmFurniture(false);

	AFurniture* ConfirmedFurniture = OwnerController->ResolveConfirmedFurnitureActorForFloorPlanSync(PreviewFurnitureBeforeConfirm, PreviouslyPlacedFurnitureKeys);
	
    RegisterConfirmedFurnitureToFloorPlan(*FurnitureRow, ConfirmedWorldLocation, ConfirmedYaw, ConfirmedFurniture);

    UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = GetFloorPlan2DWidget();
    if (FloorPlan2DWidget)
    {
        FloorPlan2DWidget->CancelFurniturePlacement();
        FloorPlan2DWidget->ClearSelectedFurniture();
    }
}

void UInteRealFloorPlanPlacementSyncComponent::HandleFloorPlan2DFurniturePreviewMoved(FVector2D DocumentPosition)
{
	if (!IsEditMode())
	{
		return;
	}

	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS || !PS->HasActivePreview())
	{
		return;
	}

	AFurniture* PreviewFurniture = PS->GetPreviewFurniture();
	if (!PreviewFurniture)
	{
		return;
	}

	const float FloorZ = PreviewFurniture->GetActorLocation().Z;
	const FVector RequestedWorldLocation(DocumentPosition.X, DocumentPosition.Y, FloorZ);

	FHitResult FloorHit;
	FloorHit.bBlockingHit = true;
	FloorHit.Location = RequestedWorldLocation;
	FloorHit.ImpactPoint = RequestedWorldLocation;
	FloorHit.ImpactNormal = FVector::UpVector;
	FloorHit.Normal = FVector::UpVector;

	PS->UpdatePreviewLocation(FloorHit);

	PreviewFurniture = PS->GetPreviewFurniture();
	if (!PreviewFurniture)
	{
		return;
	}

	const FVector SnappedWorldLocation = PreviewFurniture->GetMeshBounds().GetCenter();
	const float SnappedYaw = PreviewFurniture->GetActorRotation().Yaw;

	UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = GetFloorPlan2DWidget();
	if (FloorPlan2DWidget)
	{
		FloorPlan2DWidget->SetFurniturePreviewAtDocumentPosition(FVector2D(SnappedWorldLocation.X, SnappedWorldLocation.Y), SnappedYaw);
		FloorPlan2DWidget->SetFurniturePreviewPlacementValid(PS->InvalidReason == EPlacementInvalidReason::None);
	}
}

void UInteRealFloorPlanPlacementSyncComponent::HandleFloorPlan2DPlacedFurnitureSelected(int32 FurnitureIndex, FInteReal2DPlacedFurniture Furniture)
{
	if (!OwnerController || !IsEditMode() || CurrentSyncSource == EInteRealFloorPlanSyncSource::From3D || CurrentSyncSource == EInteRealFloorPlanSyncSource::From2D)
	{
		return;
	}

	TWeakObjectPtr<AFurniture>* FurnitureActorPtr = FloorPlan2DFurnitureActors.Find(Furniture.InstanceGuid);
	if (!FurnitureActorPtr || !FurnitureActorPtr->IsValid())
	{
		RebuildFloorPlan2DFromPlacedFurniture();
		return;
	}

	SetSyncSource(EInteRealFloorPlanSyncSource::From2D);
	OwnerController->SelectFurnitureForFloorPlanSync(FurnitureActorPtr->Get());
	ClearSyncSource(EInteRealFloorPlanSyncSource::From2D);
}

void UInteRealFloorPlanPlacementSyncComponent::HandleFloorPlan2DPlacedFurnitureMoved(int32 FurnitureIndex, FInteReal2DPlacedFurniture Furniture)
{
	if (!OwnerController || !IsEditMode())
	{
		return;
	}

	TWeakObjectPtr<AFurniture>* FurnitureActorPtr = FloorPlan2DFurnitureActors.Find(Furniture.InstanceGuid);
	if (!FurnitureActorPtr || !FurnitureActorPtr->IsValid())
	{
		return;
	}

	AFurniture* FurnitureActor = FurnitureActorPtr->Get();

	if (OwnerController->GetSelectedFurniture() != FurnitureActor)
	{
		SetSyncSource(EInteRealFloorPlanSyncSource::From2D);
		OwnerController->SelectFurnitureForFloorPlanSync(FurnitureActor);
		ClearSyncSource(EInteRealFloorPlanSyncSource::From2D);
	}

	if (!bIsMovingFurnitureFromFloorPlan2D)
	{
		bIsMovingFurnitureFromFloorPlan2D = true;

		if (UInteriorPlacementSubsystem* PS = GetPlacementSubsystem())
		{
			PS->BeginGizmoMove(FurnitureActor);
		}
	}

	SyncFurnitureActorFromFloorPlan2D(Furniture);
}

void UInteRealFloorPlanPlacementSyncComponent::HandleFloorPlan2DPlacedFurnitureMoveEnded(int32 FurnitureIndex, FInteReal2DPlacedFurniture Furniture)
{
	if (!IsEditMode())
	{
		return;
	}

	TWeakObjectPtr<AFurniture>* FurnitureActorPtr = FloorPlan2DFurnitureActors.Find(Furniture.InstanceGuid);
	if (!FurnitureActorPtr || !FurnitureActorPtr->IsValid())
	{
		bIsMovingFurnitureFromFloorPlan2D = false;
		return;
	}

	AFurniture* FurnitureActor = FurnitureActorPtr->Get();

	SyncFurnitureActorFromFloorPlan2D(Furniture);

	if (bIsMovingFurnitureFromFloorPlan2D)
	{
		if (UInteriorPlacementSubsystem* PS = GetPlacementSubsystem())
		{
			PS->FinalizeGizmoMove(FurnitureActor);
		}
	}

	bIsMovingFurnitureFromFloorPlan2D = false;
	SyncFloorPlan2DFromFurniture(FurnitureActor);

	if (FurnitureActor)
	{
		FurnitureActor->SetSelected(true);
	}
}

void UInteRealFloorPlanPlacementSyncComponent::SyncFloorPlan2DFromFurniture(AFurniture* FurnitureActor)
{
	if (!IsValid(FurnitureActor) || CurrentSyncSource == EInteRealFloorPlanSyncSource::From2D)
	{
		return;
	}

	FGuid InstanceGuid;
	if (!FindFloorPlan2DGuidForFurniture(FurnitureActor, InstanceGuid))
	{
		return;
	}

	UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = GetFloorPlan2DWidget();
	if (!FloorPlan2DWidget)
	{
		return;
	}

	const FVector WorldLocation = FurnitureActor->GetMeshBounds().GetCenter();
	const float WorldYaw = FurnitureActor->GetActorRotation().Yaw;

	SetSyncSource(EInteRealFloorPlanSyncSource::From3D);
	FloorPlan2DWidget->UpdatePlacedFurnitureByGuid(InstanceGuid, FVector2D(WorldLocation.X, WorldLocation.Y), WorldYaw);
	ClearSyncSource(EInteRealFloorPlanSyncSource::From3D);
}

void UInteRealFloorPlanPlacementSyncComponent::SyncFurnitureActorFromFloorPlan2D(const FInteReal2DPlacedFurniture& Furniture2D)
{
	if (CurrentSyncSource == EInteRealFloorPlanSyncSource::From3D || !Furniture2D.InstanceGuid.IsValid())
	{
		return;
	}

	TWeakObjectPtr<AFurniture>* FurnitureActorPtr = FloorPlan2DFurnitureActors.Find(Furniture2D.InstanceGuid);
	if (!FurnitureActorPtr || !FurnitureActorPtr->IsValid())
	{
		return;
	}

	AFurniture* FurnitureActor = FurnitureActorPtr->Get();
	if (!IsValid(FurnitureActor))
	{
		return;
	}

	SetSyncSource(EInteRealFloorPlanSyncSource::From2D);

	const FVector CurrentLocation = FurnitureActor->GetActorLocation();
	const FVector RequestedLocation(Furniture2D.CenterDocumentPosition.X, Furniture2D.CenterDocumentPosition.Y, CurrentLocation.Z);

	FRotator NewRotation = FurnitureActor->GetActorRotation();
	NewRotation.Yaw = Furniture2D.RotationDegrees;

	if (UInteriorPlacementSubsystem* PlacementSubsystem = GetPlacementSubsystem())
	{
		FurnitureActor->SetRotationPreservingPlacement(NewRotation);

		if (FurnitureActor->GetPlacedSurfaceType() == EPlacementSurfaceType::Floor)
		{
			PlacementSubsystem->UpdateGizmoRotation(FurnitureActor);
			PlacementSubsystem->UpdateGizmoMoveLocation(RequestedLocation, FurnitureActor, EGizmoTransformAxis::None);
		}
		else
		{
			PlacementSubsystem->UpdateGizmoMoveFree(RequestedLocation, FurnitureActor);
		}
	}

	ClearSyncSource(EInteRealFloorPlanSyncSource::From2D);
}

bool UInteRealFloorPlanPlacementSyncComponent::RemoveFloorPlan2DForFurniture(AFurniture* FurnitureActor)
{
	if (!IsValid(FurnitureActor))
	{
		return false;
	}

	FGuid InstanceGuid;
	if (!FindFloorPlan2DGuidForFurniture(FurnitureActor, InstanceGuid))
	{
		return false;
	}

	UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = GetFloorPlan2DWidget();
	if (FloorPlan2DWidget)
	{
		const bool bRemoved = FloorPlan2DWidget->RemovePlacedFurnitureByGuid(InstanceGuid);
		if (bRemoved)
		{
			return true;
		}
	}

	FloorPlan2DFurnitureActors.Remove(InstanceGuid);
	return false;
}

void UInteRealFloorPlanPlacementSyncComponent::HandleFloorPlan2DPlacedFurnitureDeleted(int32 FurnitureIndex, FGuid InstanceGuid)
{
	if (!OwnerController)
	{
		FloorPlan2DFurnitureActors.Remove(InstanceGuid);
		return;
	}
	
	if (CurrentSyncSource == EInteRealFloorPlanSyncSource::DeleteFrom3D)
	{
		FloorPlan2DFurnitureActors.Remove(InstanceGuid);
		return;
	}

	TWeakObjectPtr<AFurniture>* FurnitureActorPtr = FloorPlan2DFurnitureActors.Find(InstanceGuid);
	if (!FurnitureActorPtr || !FurnitureActorPtr->IsValid())
	{
		FloorPlan2DFurnitureActors.Remove(InstanceGuid);
		return;
	}

	AFurniture* FurnitureActor = FurnitureActorPtr->Get();

	FloorPlan2DFurnitureActors.Remove(InstanceGuid);

	SetSyncSource(EInteRealFloorPlanSyncSource::DeleteFrom2D);
	OwnerController->DeleteFurnitureForFloorPlanSync(FurnitureActor);
	ClearSyncSource(EInteRealFloorPlanSyncSource::DeleteFrom2D);
}

void UInteRealFloorPlanPlacementSyncComponent::HandleFloorPlan2DPlacedFurnituresCleared()
{
	if (CurrentSyncSource == EInteRealFloorPlanSyncSource::DeleteFrom3D || CurrentSyncSource == EInteRealFloorPlanSyncSource::Rebuild)
	{
		FloorPlan2DFurnitureActors.Empty();
		return;
	}

	TArray<TWeakObjectPtr<AFurniture>> FurnitureActors;
	FloorPlan2DFurnitureActors.GenerateValueArray(FurnitureActors);
	FloorPlan2DFurnitureActors.Empty();

	SetSyncSource(EInteRealFloorPlanSyncSource::DeleteFrom2D);

	for (const TWeakObjectPtr<AFurniture>& FurnitureActorPtr : FurnitureActors)
	{
		if (FurnitureActorPtr.IsValid())
		{
			OwnerController->DeleteFurnitureForFloorPlanSync(FurnitureActorPtr.Get());
		}
	}

	ClearSyncSource(EInteRealFloorPlanSyncSource::DeleteFrom2D);
}

void UInteRealFloorPlanPlacementSyncComponent::HandleFloorPlan2DPlacedFurnitureSelectionCleared()
{
	if (!OwnerController || !IsEditMode() || CurrentSyncSource == EInteRealFloorPlanSyncSource::From3D || CurrentSyncSource == EInteRealFloorPlanSyncSource::From2D)
	{
		return;
	}

	SetSyncSource(EInteRealFloorPlanSyncSource::From2D);
	OwnerController->ClearFurnitureSelectionForFloorPlanSync();
	ClearSyncSource(EInteRealFloorPlanSyncSource::From2D);
}

void UInteRealFloorPlanPlacementSyncComponent::RebuildFloorPlan2DFromPlacedFurniture()
{
	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = GetFloorPlan2DWidget();

	if (!PS || !FloorPlan2DWidget)
	{
		return;
	}

	BindFloorPlan2DEvents();

	SetSyncSource(EInteRealFloorPlanSyncSource::Rebuild);

	FloorPlan2DFurnitureActors.Empty();
	FloorPlan2DWidget->ClearPlacedFurnitures();

	for (AFurniture* FurnitureActor : PS->GetPlacedFurnitures())
	{
		if (!IsValid(FurnitureActor))
		{
			continue;
		}

		const FFurnitureDataRow* FurnitureRow = PS->FindFurnitureRowByID(FurnitureActor->FurnitureID);
		if (!FurnitureRow)
		{
			continue;
		}

		const FVector WorldLocation = FurnitureActor->GetMeshBounds().GetCenter();
		const float WorldYaw = FurnitureActor->GetActorRotation().Yaw;

		const FGuid AddedFloorPlanGuid = FloorPlan2DWidget->AddPlacedFurnitureAtDocumentPosition(*FurnitureRow, FVector2D(WorldLocation.X, WorldLocation.Y), WorldYaw);
		RegisterFloorPlan2DFurnitureActor(AddedFloorPlanGuid, FurnitureActor);
	}

	ClearSyncSource(EInteRealFloorPlanSyncSource::Rebuild);

	if (IsValid(OwnerController->GetSelectedFurniture()))
	{
		FGuid SelectedGuid;
		if (FindFloorPlan2DGuidForFurniture(OwnerController->GetSelectedFurniture(), SelectedGuid))
		{
			SetSyncSource(EInteRealFloorPlanSyncSource::From3D);
			FloorPlan2DWidget->SelectPlacedFurnitureByGuid(SelectedGuid);
			ClearSyncSource(EInteRealFloorPlanSyncSource::From3D);
		}
		else
		{
			FloorPlan2DWidget->ClearSelectedFurniture();
		}
	}
	else
	{
		FloorPlan2DWidget->ClearSelectedFurniture();
	}
}

void UInteRealFloorPlanPlacementSyncComponent::StartFloorPlan2DPlacement(const FFurnitureDataRow& FurnitureRow)
{
	BindFloorPlan2DEvents();

	UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = GetFloorPlan2DWidget();
	if (!FloorPlan2DWidget)
	{
		return;
	}

	FloorPlan2DWidget->StartFurniturePlacement(FurnitureRow);
}

void UInteRealFloorPlanPlacementSyncComponent::CancelFloorPlan2DPlacement()
{
	UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = GetFloorPlan2DWidget();
	if (!FloorPlan2DWidget)
	{
		return;
	}

	FloorPlan2DWidget->CancelFurniturePlacement();
}

void UInteRealFloorPlanPlacementSyncComponent::ClearFloorPlan2DSelection()
{
	UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = GetFloorPlan2DWidget();
	if (!FloorPlan2DWidget)
	{
		return;
	}

	FloorPlan2DWidget->ClearSelectedFurniture();
}

void UInteRealFloorPlanPlacementSyncComponent::SetFloorPlan2DPreviewFromFurniture(AFurniture* PreviewFurniture)
{
	if (!IsValid(PreviewFurniture))
	{
		return;
	}

	UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = GetFloorPlan2DWidget();
	if (!FloorPlan2DWidget)
	{
		return;
	}

	const FVector PreviewLocation = PreviewFurniture->GetMeshBounds().GetCenter();

	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	FloorPlan2DWidget->SetFurniturePreviewAtDocumentPosition(FVector2D(PreviewLocation.X, PreviewLocation.Y), PreviewFurniture->GetActorRotation().Yaw);
	FloorPlan2DWidget->SetFurniturePreviewPlacementValid(!PS || PS->InvalidReason == EPlacementInvalidReason::None);
}

void UInteRealFloorPlanPlacementSyncComponent::SyncPreview2DFromActivePreview()
{
	UInteriorPlacementSubsystem* PS = GetPlacementSubsystem();
	if (!PS || !PS->HasActivePreview())
	{
		return;
	}

	AFurniture* PreviewFurniture = PS->GetPreviewFurniture();
	if (!PreviewFurniture)
	{
		return;
	}

	SetFloorPlan2DPreviewFromFurniture(PreviewFurniture);
}