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
class UMaterialInterface;

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
	
protected:
	void UpdateCursorHit();
	void ToggleGrid();

	void OnPlace();
	void OnPlaceReleased();
	void OnRemove();
	void OnRotatePreview();
	void OnRotate15();

	void SelectFurniture(AFurniture* Furniture);
	void DeselectFurniture();
	
	void SelectWall(UDynamicMeshComponent* WallComponent);
	void DeselectWall();

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

	void EnhancedMove(const FInputActionValue& Value);
	void EnhancedLook(const FInputActionValue& Value);
	void EnhancedZoom(const FInputActionValue& Value);

public:
	// ===== Mode =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteReal|Mode")
	EInteRealControlMode CurrentControlMode = EInteRealControlMode::View;

	// ===== Edit Mode References =====
	UPROPERTY(EditAnywhere, Category = "EditMode")
	TObjectPtr<AInteriorPlacementManager> PlacementManager = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "EditMode|Input")
	FVector CurrentCursorWorldLoc = FVector::ZeroVector;

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

	UPROPERTY(EditAnywhere, Category = "EditMode|Gizmo")
	float GizmoRotationSensitivity = 1.5f;

	// ===== View Mode Input =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputMappingContext> ViewModeMappingContext = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputAction> IA_SwitchToTopDown = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputAction> IA_SwitchToIsometric = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputAction> IA_SwitchToFirstPerson = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputAction> IA_Move = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputAction> IA_Look = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputAction> IA_Zoom = nullptr;

private:
	// ===== Edit Mode State =====
	bool bIsHitting = false;
	FHitResult LastCursorHit;
	bool bGridVisible = false;
	
	bool bIsMovingFurniture = false;
	FVector MoveDragOffset = FVector::ZeroVector;

	UPROPERTY()
	TObjectPtr<AFurniture> SelectedFurniture = nullptr;
	
	UPROPERTY()
	TObjectPtr<UDynamicMeshComponent> SelectedWallComponent = nullptr;

	bool bIsDraggingGizmo = false;
	FString CurrentDraggingAxis;
	float DragStartAngleDeg = 0.0f;
	FRotator DragStartFurnitureRot = FRotator::ZeroRotator;
	FVector DragStartFurnitureLocation = FVector::ZeroVector;
	
	// ===== View Mode State =====
	UPROPERTY()
	TObjectPtr<AViewModeManager> CachedViewModeManager = nullptr;
};
