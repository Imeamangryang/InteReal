#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InteReal/ViewMode/ViewModeData.h"
#include "InteReal/EditMode/Furnitures/FFurnitureDataRow.h"
#include "InteReal/EditMode/Furnitures/Furniture.h"
#include "InputActionValue.h"
#include "InteRealPlayerController.generated.h"

class AInteriorPlacementManager;
class AViewModeManager;
class AInteRealHUD;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditMode|Gizmo")
	TSubclassOf<AActor> GizmoActorClass;
	
protected:
	void UpdateCursorHit();
	void ToggleGrid();

	void OnPlaceKey();
	void OnPlaceReleasedKey();
	void OnRemoveKey();
	void OnRotatePreviewKey();
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

	void InitGizmoAxisMaterials();
	void UpdateGizmoHover();
	void SetGizmoAxisOpacity(const FString& Axis, float Opacity);
	
	void SelectSurface(UMeshComponent* SurfaceComponent);
	void DeselectSurface();

	UFUNCTION(BlueprintPure, Category = "EditMode|Input")
	FVector GetCurrentCursorWorldLocation() const { return CurrentCursorWorldLoc; }

	UFUNCTION(BlueprintPure, Category = "EditMode|Input")
	bool GetIsHitting() const { return bIsHitting; }

	void FindPlacementManager();

	// ===== View Mode =====
public:
	UFUNCTION(BlueprintCallable, Category = "ViewMode")
	void SetViewMode(EHarnessViewMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "ViewMode|UI")
	void SetupMinimapHUD(
		UHarnessMinimapCaptureComponent* InCaptureComp,
		UTextureRenderTarget2D* InRT,
		TSubclassOf<UHarnessCaptureMinimapWidget> InWidgetClass
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
	UPROPERTY(EditAnywhere, Category = "EditMode")
	TObjectPtr<AInteriorPlacementManager> PlacementManager = nullptr;
	
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

	// 연속 배치 모디파이어 (Shift) — 누르고 있는 동안 배치 시 프리뷰가 즉시 재생성됨
	UPROPERTY(EditAnywhere, Category = "EditMode|Input")
	TObjectPtr<UInputAction> IA_Shift = nullptr;

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

	UPROPERTY(EditAnywhere, Category = "EditMode|Gizmo")
	float GizmoRotationSensitivity = 1.5f;
	
	UPROPERTY(EditAnywhere, Category = "EditMode|Gizmo")
	FName GizmoOpacityParamName = TEXT("Opacity");
	
	UPROPERTY(EditAnywhere, Category = "EditMode|Gizmo")
	float GizmoDefaultOpacity = 0.3f;
	
	UPROPERTY(EditAnywhere, Category = "EditMode|Gizmo")
	float GizmoHighlightOpacity = 1.0f;

	// ===== View Mode Input =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputMappingContext> IMC_ViewMode = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputAction> IA_SwitchToTopDown = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputAction> IA_SwitchToIsometric = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputAction> IA_SwitchToFirstPerson = nullptr;

private:
	// ===== Edit Mode State =====
	bool bIsHitting = false;
	FHitResult LastCursorHit;
	bool bGridVisible = false;
	bool bContinuousModifierHeld = false;

	bool bIsMovingFurniture = false;
	FVector MoveDragOffset = FVector::ZeroVector;

	UPROPERTY()
	TObjectPtr<AFurniture> SelectedFurniture = nullptr;

	UPROPERTY()
	TObjectPtr<UMeshComponent> SelectedSurfaceComponent = nullptr;
	
	UPROPERTY()
	AActor* SpawnedGizmo = nullptr;
	
	bool bHasCopiedFurniture = false;
	FFurnitureDataRow CopiedFurnitureRow;
	FRotator CopiedFurnitureRotation = FRotator::ZeroRotator;

	bool bIsDraggingGizmo = false;
	FString CurrentDraggingAxis;
	float DragStartAngleDeg = 0.0f;
	FRotator DragStartFurnitureRot = FRotator::ZeroRotator;
	FVector DragStartFurnitureLocation = FVector::ZeroVector;
	
	// Gizmo
	TMap<FString, TArray<TObjectPtr<UMaterialInstanceDynamic>>> GizmoAxisMaterials;
	FString HoveredGizmoAxis;

	// ===== View Mode State =====
	UPROPERTY()
	TObjectPtr<AViewModeManager> CachedViewModeManager = nullptr;
};