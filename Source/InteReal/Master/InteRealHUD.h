#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "InteReal/ViewMode/ViewModeData.h"
#include "InteReal/EditMode/UI/PlacementTooltipWidget.h"
#include "InteReal/EditMode/UI/RotationGuideWidget.h"
#include "InteReal/EditMode/UI/UserGuideWidget.h"
#include "InteReal/EditMode/Managers/InteriorPlacementManager.h"
#include "InteRealPlayerController.h"
#include "InteRealHUD.generated.h"

class UUserWidget;
class UHarnessMinimapCaptureComponent;
class UHarnessCaptureMinimapWidget;
class UTextureRenderTarget2D;
class UInteReal2DFloorPlanViewportWidget;
class UHarnessPipelineManager;
struct FHarnessFloorData;

UCLASS()
class INTEREAL_API AInteRealHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	void InitializeHUDWidgets();
	void UpdateModeUIVisibility(EInteRealControlMode CurrentMode);
	void UpdatePlacementTooltip(
		bool bIsEditMode,
		bool bHasActivePreview,
		EPlacementInvalidReason InvalidReason,
		const FVector2D& MousePosition
	);

	void SetupMinimapHUD(
		UHarnessMinimapCaptureComponent* InCaptureComp,
		UTextureRenderTarget2D* InRT,
		TSubclassOf<UHarnessCaptureMinimapWidget> InWidgetClass,
		EHarnessViewMode CurrentViewMode,
		EInteRealControlMode CurrentControlMode
	);

	void UpdateUserGuide(bool bVisible, EPlacementInvalidReason Reason, const FVector2D& MousePosition);
	void ShowMinimap(EInteRealControlMode CurrentMode);
	void UpdateMinimapIconVisibility(EHarnessViewMode NewMode);

	UUserWidget* GetPlacementTabInstance() const { return PlacementTabInstance; }

	UFUNCTION(BlueprintCallable, Category="InteReal2D|UI")
	void ShowFloorPlan2D(bool bVisible);

	UFUNCTION(BlueprintCallable, Category="InteReal2D|UI")
	void LoadFloorPlan2DFromHarnessData(const FHarnessFloorData& InFloorData);

	UFUNCTION(BlueprintPure, Category="InteReal2D|UI")
	UInteReal2DFloorPlanViewportWidget* GetFloorPlan2DWidget() const { return FloorPlan2DWidgetInstance; }

	UFUNCTION(BlueprintCallable, Category="Harness|UI")
	void BindHarnessPipeline(UHarnessPipelineManager* InPipelineManager);

public:
	// ===== Edit Mode UI =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditMode|UI")
	TSubclassOf<UUserWidget> PlacementTabWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> PlacementTabInstance = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditMode|UI")
	TSubclassOf<UPlacementTooltipWidget> TooltipWidgetClass;

	UPROPERTY()
	TObjectPtr<UPlacementTooltipWidget> TooltipInstance = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditMode|UI")
	TSubclassOf<URotationGuideWidget> RotationGuideWidgetClass;

	UPROPERTY()
	TObjectPtr<URotationGuideWidget> RotationGuideInstance = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditMode|UI")
	TSubclassOf<UUserGuideWidget> UserGuideWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserGuideWidget> UserGuideInstance = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditMode|UI")
	TSubclassOf<UInteReal2DFloorPlanViewportWidget> FloorPlan2DWidgetClass;

	UPROPERTY()
	TObjectPtr<UInteReal2DFloorPlanViewportWidget> FloorPlan2DWidgetInstance = nullptr;

	// ===== View Mode UI =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|UI")
	TSubclassOf<UUserWidget> ViewModeWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|UI")
	TSubclassOf<UHarnessCaptureMinimapWidget> MinimapWidgetClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|UI")
	TSubclassOf<UUserWidget> WeatherWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> ViewModeWidgetInstance = nullptr;

	UPROPERTY()
	TObjectPtr<UHarnessCaptureMinimapWidget> MinimapWidgetInstance = nullptr;
	
	UPROPERTY()
	TObjectPtr<UUserWidget> WeatherWidgetInstance = nullptr;

private:
	UFUNCTION()
	void OnFloorPlanDataReady(const FHarnessFloorData& InFloorData);
};