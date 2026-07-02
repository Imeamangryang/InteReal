#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteReal/EditMode/2D/InteReal2DFloorPlanTypes.h"
#include "InteReal/EditMode/Furniture/FFurnitureDataRow.h"
#include "InteRealFloorPlanPlacementSyncComponent.generated.h"

class AInteRealPlayerController;
class AInteRealHUD;
class AFurniture;
class UInteriorPlacementSubsystem;
class UInteReal2DFloorPlanViewportWidget;

UENUM()
enum class EInteRealFloorPlanSyncSource : uint8
{
    None,
    From2D,
    From3D,
    Rebuild,
    DeleteFrom2D,
    DeleteFrom3D
};

UCLASS(ClassGroup=(InteReal), meta=(BlueprintSpawnableComponent))
class INTEREAL_API UInteRealFloorPlanPlacementSyncComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInteRealFloorPlanPlacementSyncComponent();

    void Initialize(AInteRealPlayerController* InOwnerController);
    void BindFloorPlan2DEvents();

    void StartFloorPlan2DPlacement(const FFurnitureDataRow& FurnitureRow);
    void CancelFloorPlan2DPlacement();
    void ClearFloorPlan2DSelection();

    void RebuildFloorPlan2DFromPlacedFurniture();
    void RegisterFloorPlan2DFurnitureActor(const FGuid& FloorPlanFurnitureGuid, AFurniture* FurnitureActor);
    bool FindFloorPlan2DGuidForFurniture(const AFurniture* FurnitureActor, FGuid& OutInstanceGuid) const;
    bool RemoveFloorPlan2DForFurniture(AFurniture* FurnitureActor);

    void SyncFloorPlan2DFromFurniture(AFurniture* FurnitureActor);
    void SyncFurnitureActorFromFloorPlan2D(const FInteReal2DPlacedFurniture& Furniture2D);
    void SetFloorPlan2DPreviewFromFurniture(AFurniture* PreviewFurniture);
    void SyncPreview2DFromActivePreview();
    void SelectFloorPlan2DForFurniture(AFurniture* FurnitureActor);
    void RequestRebuildFloorPlan2DFromPlacedFurniture();

    void RegisterConfirmedFurnitureToFloorPlan(const FFurnitureDataRow& FurnitureRow, const FVector& ConfirmedWorldLocation, float ConfirmedYaw, AFurniture* ConfirmedFurniture);

    void SetDeletingFrom3D(bool bDeleting);
    bool IsSyncingFurniture3DFrom2D() const;
    bool IsSyncingFloorPlan2DFrom3D() const;

private:
    UFUNCTION()
    void HandleFloorPlan2DFurniturePlacementRequested(FVector2D DocumentPosition);

    UFUNCTION()
    void HandleFloorPlan2DFurniturePreviewMoved(FVector2D DocumentPosition);

    UFUNCTION()
    void HandleFloorPlan2DPlacedFurnitureSelected(int32 FurnitureIndex, FInteReal2DPlacedFurniture Furniture);

    UFUNCTION()
    void HandleFloorPlan2DPlacedFurnitureMoved(int32 FurnitureIndex, FInteReal2DPlacedFurniture Furniture);

    UFUNCTION()
    void HandleFloorPlan2DPlacedFurnitureMoveEnded(int32 FurnitureIndex, FInteReal2DPlacedFurniture Furniture);

    UFUNCTION()
    void HandleFloorPlan2DPlacedFurnitureDeleted(int32 FurnitureIndex, FGuid InstanceGuid);

    UFUNCTION()
    void HandleFloorPlan2DPlacedFurnituresCleared();
    
    UFUNCTION()
    void HandleFloorPlan2DPlacedFurnitureSelectionCleared();

private:
    AInteRealHUD* GetInteRealHUD() const;
    UInteriorPlacementSubsystem* GetPlacementSubsystem() const;
    UInteReal2DFloorPlanViewportWidget* GetFloorPlan2DWidget() const;
    bool IsEditMode() const;
    void SetSyncSource(EInteRealFloorPlanSyncSource NewSource);
    void ClearSyncSource(EInteRealFloorPlanSyncSource ExpectedSource);
    
    bool SyncFloorPlan2DEntryFromFurniture(UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget, AFurniture* FurnitureActor);
    void CollectFloorPlan2DDescendantsRecursive(const AFurniture* ParentFurniture, TArray<AFurniture*>& OutDescendants, TSet<TObjectKey<AFurniture>>& Visited) const;
    void CollectFloorPlan2DDescendants(const AFurniture* ParentFurniture, TArray<AFurniture*>& OutDescendants) const;
    bool RemoveSingleFloorPlan2DForFurniture(AFurniture* FurnitureActor);
    bool RemoveFloorPlan2DDescendantsForFurniture(AFurniture* FurnitureActor);

private:
    UPROPERTY()
    TObjectPtr<AInteRealPlayerController> OwnerController = nullptr;

    TMap<FGuid, TWeakObjectPtr<AFurniture>> FloorPlan2DFurnitureActors;

    EInteRealFloorPlanSyncSource CurrentSyncSource = EInteRealFloorPlanSyncSource::None;
    bool bIsMovingFurnitureFromFloorPlan2D = false;
    
    bool bPendingRebuildFloorPlan2D = false;
};