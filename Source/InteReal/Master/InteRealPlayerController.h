#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InteReal/ViewMode/ViewModeData.h"
#include "InteReal/EditMode/Furniture/FFurnitureDataRow.h"
#include "InteReal/EditMode/Furniture/Furniture.h"
#include "InteReal/EditMode/2D/InteReal2DFloorPlanViewportWidget.h"
#include "InputActionValue.h"
#include "InteRealPlayerController.generated.h"

class UInteRealMinimap;
class UInteriorPlacementSubsystem;
class AViewModeManager;
class AInteRealHUD;
class AInteRealGizmoActor;
class UFurnitureGizmoComponent;
class UHarnessMinimapCaptureComponent;
class UHarnessCaptureMinimapWidget;
class UInputMappingContext;
class UInputAction;
class UUserWidget;
class UTextureRenderTarget2D;
class UDynamicMeshComponent;
class UMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UInteRealFloorPlanPlacementSyncComponent;

UENUM(BlueprintType)
enum class EInteRealControlMode : uint8
{
	View UMETA(DisplayName = "View"),
	Edit UMETA(DisplayName = "Edit")
};

UCLASS()
class INTEREAL_API AInteRealPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AInteRealPlayerController();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

	// ===== Common / Mode =====
	UFUNCTION(BlueprintCallable, Category = "InteReal|Mode")
	void SetControlMode(EInteRealControlMode NewMode);

	UFUNCTION()
	void HandleModeChanged(bool bIsEditMode);

	UFUNCTION()
	void HandleIconClicked(FName command);

	UFUNCTION()
	void HandleFurnitureSpawn(FFurnitureDataRow FurnitureData);
	
	UFUNCTION()
	void HandleWallMaterialChanged(UMaterialInterface* NewMaterial);
	
	UFUNCTION()
	void HandlePipelineLoadFinished();

	UFUNCTION()
	void HandleFloorPlanPanelOpenChanged(bool bOpen);

	UFUNCTION(BlueprintPure, Category = "InteReal|Mode")
	EInteRealControlMode GetControlMode() const { return CurrentControlMode; }

protected:
	void ApplyCurrentControlMode();
	void UpdateMappingContexts();
	void UpdateInputModeForCurrentControlMode();

	AInteRealHUD* GetInteRealHUD() const;

	// ===== Edit Mode =====
public:
	UFUNCTION(BlueprintCallable, Category = "EditMode|Furniture")
	void StartFurniturePlacement(const FFurnitureDataRow& FurnitureRow);

	UFUNCTION(BlueprintCallable, Category = "EditMode|Web")
	void ReceiveWebCommand(const FString& JsonString);

	UFUNCTION(BlueprintCallable, Category = "EditMode|Wall")
	void ApplyMaterialToSelectedWall(UMaterialInterface* NewMaterial);
	
	UFUNCTION(BlueprintCallable, Category = "EditMode|Surface")
	void ApplyMaterialToSelectedSurface(UMaterialInterface* NewMaterial);
	
	UFUNCTION(BlueprintPure, Category = "InteReal|FloorPlanSync")
	AFurniture* GetSelectedFurniture() const { return SelectedFurniture.Get(); }

	UFUNCTION(BlueprintCallable, Category = "InteReal|FloorPlanSync")
	void SelectFurnitureForFloorPlanSync(AFurniture* Furniture);

	UFUNCTION(BlueprintCallable, Category = "InteReal|FloorPlanSync")
	void DeleteFurnitureForFloorPlanSync(AFurniture* Furniture);
	
	UFUNCTION(BlueprintCallable, Category = "InteReal|FloorPlanSync")
	void ClearFurnitureSelectionForFloorPlanSync();

	void SnapshotPlacedFurnitureActorsForFloorPlanSync(TSet<TObjectKey<AFurniture>>& OutPlacedFurnitureKeys) const;
	AFurniture* ResolveConfirmedFurnitureActorForFloorPlanSync(AFurniture* PreviousPreviewFurniture, const TSet<TObjectKey<AFurniture>>& PreviouslyPlacedFurnitureKeys) const;
	AFurniture* ConfirmActivePreviewFurnitureForFloorPlanSync(bool bContinuePlacement);
	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditMode|Gizmo")
	TSubclassOf<AInteRealGizmoActor> GizmoActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditMode|Gizmo|FirstPerson")
	float FirstPersonGizmoScaleMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditMode|Gizmo")
	float GizmoTraceRadius = 18.0f;
	
	UFUNCTION(BlueprintCallable, Category = "EditMode|Gizmo")
	void ToggleFreePlacementMode();

	UFUNCTION(BlueprintCallable, Category = "EditMode|Gizmo")
	void ToggleGizmoDisplayMode();

	UFUNCTION(BlueprintCallable, Category = "EditMode|Gizmo")
	void SetGizmoShowMove(bool bShow);

	UFUNCTION(BlueprintCallable, Category = "EditMode|Gizmo")
	void SetGizmoShowRotate(bool bShow);

	UFUNCTION(BlueprintPure, Category = "EditMode|Gizmo")
	bool IsGizmoShowingMove() const;

	UFUNCTION(BlueprintPure, Category = "EditMode|Gizmo")
	bool IsGizmoShowingRotate() const;
	
protected:
	void UpdateCursorHit();
	void ToggleGrid();
	void OnToggleFreePlacementKey();
	void OnToggleGizmoDisplayModeKey();
	void ApplyGizmoDisplayFlags(bool bShowMove, bool bShowRotate);


	void OnPlaceKey();
	void OnPlaceReleasedKey();
	void OnRemoveKey();
	void RotateEditFurniture(float AngleDeg);
	void OnRotateKey();
	void OnRotate15Key();
	void OnContinuousPressed();
	void OnContinuousReleased();
	void OnToggleModeKey();
	void OnFocusSelectionKey();
	void OnUndoKey();
	void OnRedoKey();
	void OnCopyKey();
	void OnPasteKey();
	void OnDuplicateKey();
	void OnSaveKey();
	
	void SelectFurniture(AFurniture* Furniture);
	void DeselectFurniture();

	UFUNCTION()
	void OnGizmoRotationChanged(float NewYawDegrees);

	void SelectSurface(UMeshComponent* SurfaceComponent);
	void DeselectSurface();

	UFUNCTION(BlueprintPure, Category = "EditMode|Input")
	FVector GetCurrentCursorWorldLocation() const { return CurrentCursorWorldLoc; }

	UFUNCTION(BlueprintPure, Category = "EditMode|Input")
	bool GetIsHitting() const { return bIsHitting; }

	UInteriorPlacementSubsystem* GetPlacementSubsystem() const;

	// ===== View Mode =====
public:
	UFUNCTION(BlueprintCallable, Category = "ViewMode")
	void SetViewMode(EHarnessViewMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "ViewMode|UI")
	void SetupMinimapHUD(
		UHarnessMinimapCaptureComponent* InCaptureComp,
		UTextureRenderTarget2D* InRT,
		TSubclassOf<UInteRealMinimap> InWidgetClass
	);

	UFUNCTION(BlueprintCallable, Category = "Harness|Minimap")
	void ShowMinimap();

protected:
	void FindViewModeManager();

	void OnTopDownKey();
	void OnIsometricKey();
	void OnFirstPersonKey();

	void OnMoveKey(const FInputActionValue& Value);
	void OnMoveVerticalKey(const FInputActionValue& Value);
	void OnLookKey(const FInputActionValue& Value);
	void OnZoomKey(const FInputActionValue& Value);

public:
	// ===== Mode =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteReal|Mode")
	EInteRealControlMode CurrentControlMode = EInteRealControlMode::View;

	// ===== Common Input =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common|Input")
	TObjectPtr<UInputMappingContext> IMC_Common = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common|Input")
	TObjectPtr<UInputAction> IA_ToggleMode = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common|Input")
	TObjectPtr<UInputAction> IA_Move = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common|Input")
	TObjectPtr<UInputAction> IA_MoveVertical = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common|Input")
	TObjectPtr<UInputAction> IA_Look = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common|Input")
	TObjectPtr<UInputAction> IA_Zoom = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common|Input")
	TObjectPtr<UInputAction> IA_FocusSelection = nullptr;

	// ===== Edit Mode References =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditMode|Input")
	TEnumAsByte<ECollisionChannel> WallTraceChannel = ECC_GameTraceChannel1;

	UPROPERTY(BlueprintReadOnly, Category = "EditMode|Input")
	FVector CurrentCursorWorldLoc = FVector::ZeroVector;

	// ===== Edit Mode Input =====
	UPROPERTY(EditAnywhere, Category = "EditMode|Input")
	TObjectPtr<UInputMappingContext> IMC_EditMode = nullptr;

	UPROPERTY(EditAnywhere, Category = "EditMode|Input")
	TObjectPtr<UInputAction> IA_Place = nullptr;

	UPROPERTY(EditAnywhere, Category = "EditMode|Input")
	TObjectPtr<UInputAction> IA_Remove = nullptr;

	UPROPERTY(EditAnywhere, Category = "EditMode|Input")
	TObjectPtr<UInputAction> IA_Rotate = nullptr;

	UPROPERTY(EditAnywhere, Category = "EditMode|Input")
	TObjectPtr<UInputAction> IA_Rotate15 = nullptr;

	UPROPERTY(EditAnywhere, Category = "EditMode|Input")
	TObjectPtr<UInputAction> IA_Continuous = nullptr;

	UPROPERTY(EditAnywhere, Category = "EditMode|Input")
	TObjectPtr<UInputAction> IA_Undo = nullptr;

	UPROPERTY(EditAnywhere, Category = "EditMode|Input")
	TObjectPtr<UInputAction> IA_Redo = nullptr;

	UPROPERTY(EditAnywhere, Category = "EditMode|Input")
	TObjectPtr<UInputAction> IA_Copy = nullptr;

	UPROPERTY(EditAnywhere, Category = "EditMode|Input")
	TObjectPtr<UInputAction> IA_Paste = nullptr;

	UPROPERTY(EditAnywhere, Category = "EditMode|Input")
	TObjectPtr<UInputAction> IA_Duplicate = nullptr;

	UPROPERTY(EditAnywhere, Category = "EditMode|Input")
	TObjectPtr<UInputAction> IA_Save = nullptr;

	// ===== View Mode Input =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputMappingContext> IMC_ViewMode = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputAction> IA_SwitchToTopDown = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputAction> IA_SwitchToIsometric = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputAction> IA_SwitchToFirstPerson = nullptr;

	UPROPERTY()
	TObjectPtr<UInteRealFloorPlanPlacementSyncComponent> FloorPlanPlacementSyncComponent;
	
private:
	// ===== Edit Mode State =====
	bool bIsHitting = false;
	FHitResult LastCursorHit;
	bool bGridVisible = false;
	bool bContinuousModifierHeld = false;

	bool bIsMovingFurniture = false;
	bool bPreviewHiddenUntilViewport = false;
	FVector MoveDragOffset = FVector::ZeroVector;
	bool bIsGizmoRotationWidgetActive = false;
	
	void SnapshotPlacedFurnitureActors(TSet<TObjectKey<AFurniture>>& OutPlacedFurnitureKeys) const;
	AFurniture* ResolveConfirmedFurnitureActor(
		AFurniture* PreviousPreviewFurniture,
		const TSet<TObjectKey<AFurniture>>& PreviouslyPlacedFurnitureKeys
	) const;
	void DeleteFurnitureActor(AFurniture* FurnitureActor);
	void ClearFurnitureSelectionInternal(bool bSyncFloorPlan2D);

	UPROPERTY()
	TObjectPtr<AFurniture> SelectedFurniture = nullptr;

	UPROPERTY()
	TObjectPtr<UMeshComponent> SelectedSurfaceComponent = nullptr;
	
	UPROPERTY()
	TObjectPtr<AInteRealGizmoActor> SpawnedGizmo = nullptr;

	bool bHasCopiedFurniture = false;
	FFurnitureDataRow CopiedFurnitureRow;
	FRotator CopiedFurnitureRotation = FRotator::ZeroRotator;

	FVector DragStartFurnitureLocation = FVector::ZeroVector;

	// 1인칭 포커스 보간
	bool bIsFocusingFirstPerson = false;
	FVector FirstPersonFocusTarget = FVector::ZeroVector;

	// ===== View Mode State =====
	UPROPERTY()
	TObjectPtr<AViewModeManager> CachedViewModeManager = nullptr;
	
	bool bFloorPlanOffsetInitialized = false;
};
