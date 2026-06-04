#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "InteReal/EditMode/UI/PlacementTooltipWidget.h"
#include "InteReal/EditMode/UI/RotationGuideWidget.h"
#include "Furnitures/FFurnitureDataRow.h"
#include "Furnitures/Furniture.h"
#include "EditModePlayerController.generated.h"

class AInteriorPlacementManager;
class UInputMappingContext;
class UInputAction;

UCLASS()
class INTEREAL_API AEditModePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AEditModePlayerController();

	UFUNCTION(BlueprintCallable, Category = "EditMode | Furniture")
	void StartFurniturePlacement(const FFurnitureDataRow& FurnitureRow);

	UFUNCTION(BlueprintCallable, Category = "EditMode | Web")
	void ReceiveWebCommand(const FString& JsonString);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

	UPROPERTY(BlueprintReadOnly, Category = "EditMode | Input")
	FVector CurrentCursorWorldLoc;

	bool bIsHitting = false;
	FHitResult LastCursorHit;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> PlacementTabWidget;

	UPROPERTY()
	UUserWidget* PlacementTabInstance;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UPlacementTooltipWidget> TooltipWidgetClass;

	UPROPERTY()
	UPlacementTooltipWidget* TooltipInstance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<URotationGuideWidget> RotationGuideWidgetClass;

	UPROPERTY()
	URotationGuideWidget* RotationGuideInstance;

	UPROPERTY(EditAnywhere, Category = "EditMode")
	AInteriorPlacementManager* PlacementManager;

	UPROPERTY(EditAnywhere, Category = "EditMode | Input")
	UInputMappingContext* IMC_EditMode;

	UPROPERTY(EditAnywhere, Category = "EditMode | Input")
	UInputAction* IA_Place;

	UPROPERTY(EditAnywhere, Category = "EditMode | Input")
	UInputAction* IA_Remove;

	UPROPERTY(EditAnywhere, Category = "EditMode | Input")
	UInputAction* IA_Rotate;

	// 기즈모 드래그 회전 감도 (에디터에서 조절 가능)
	UPROPERTY(EditAnywhere, Category = "EditMode | Gizmo")
	float GizmoRotationSensitivity = 1.5f;

private:
	bool bGridVisible = false;

	// 선택된 가구 (nullptr = 선택 없음)
	AFurniture* SelectedFurniture = nullptr;

	// 기즈모 링 드래그 중 여부
	bool bIsDraggingGizmo = false;

	// 래디얼 회전 추적용 — 드래그 시작 시점의 각도와 가구 회전값 저장
	float DragStartAngleDeg = 0.0f;
	FRotator DragStartFurnitureRot = FRotator::ZeroRotator;

	UFUNCTION(BlueprintPure, Category = "EditMode | Input")
	FVector GetCurrentCursorWorldLocation() const { return CurrentCursorWorldLoc; }

	UFUNCTION(BlueprintPure, Category = "EditMode | Input")
	bool GetIsHitting() const { return bIsHitting; }

	void UpdateCursorHit();
	void UpdateTooltip();
	void ToggleGrid();

	void OnPlace();
	void OnPlaceReleased();
	void OnRemove();
	void OnRotatePreview();

	void SelectFurniture(AFurniture* Furniture);
	void DeselectFurniture();
};
