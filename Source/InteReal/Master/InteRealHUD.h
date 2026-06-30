#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "InteReal/ViewMode/ViewModeData.h"
#include "InteReal/EditMode/UI/PlacementTooltipWidget.h"
#include "InteReal/EditMode/UI/RotationGuideWidget.h"
#include "InteReal/EditMode/UI/UserGuideWidget.h"
#include "InteReal/EditMode/UI/EditModeToolbarWidget.h"
#include "InteReal/EditMode/UI/FurnitureSizePanelWidget.h"
#include "InteReal/EditMode/UI/Lights/LightAttributesPanelWidget.h"
#include "InteReal/Master/UI/SubWidgets/MaterialAttributesPanelWidget.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "InteRealPlayerController.h"
#include "InteRealHUD.generated.h"

class AFurniture;
class UEnvironmentPanel;
class UUserWidget;
class UHarnessMinimapCaptureComponent;
class UInteRealMinimap;
class UTextureRenderTarget2D;
class UInteReal2DFloorPlanViewportWidget;
class UHarnessPipelineManager;
class UEditModeLayoutWidget;
class UTopBarWidget;
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
			TSubclassOf<UInteRealMinimap> InWidgetClass,
			EHarnessViewMode CurrentViewMode,
			EInteRealControlMode CurrentControlMode
		);

	void UpdateUserGuide(bool bVisible, EPlacementInvalidReason Reason, const FVector2D& MousePosition);
	void UpdateRotationGuide(bool bVisible, float DeltaAngle, const FVector2D& AnchorScreenPosition);
	void ShowRotationGuideForInput(float InitialYawDegrees, const FVector2D& GizmoCenterScreenPos);
	void UpdateRotationGuideForInput(float DeltaAngle);
	URotationGuideWidget* GetRotationGuideInstance() const { return RotationGuideInstance; }
	void ShowMinimap(EInteRealControlMode CurrentMode);
	void UpdateMinimapIconVisibility(EHarnessViewMode NewMode);

	UUserWidget* GetPlacementTabInstance() const { return PlacementTabInstance; }

	UFUNCTION(BlueprintCallable, Category="InteReal2D|UI")
	void ShowFloorPlan2D(bool bVisible);

	UFUNCTION(BlueprintCallable, Category="InteReal2D|UI")
	void LoadFloorPlan2DFromHarnessData(const FHarnessFloorData& InFloorData);

	UFUNCTION(BlueprintPure, Category="InteReal2D|UI")
	UInteReal2DFloorPlanViewportWidget* GetFloorPlan2DWidget() const;

	UFUNCTION(BlueprintCallable, Category="Harness|UI")
	void BindHarnessPipeline(UHarnessPipelineManager* InPipelineManager);

public:
	// ===== Common UI =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Master|UI")
	TSubclassOf<UTopBarWidget> TopBarWidgetClass;

	UPROPERTY()
	TObjectPtr<UTopBarWidget> TopBarWidgetInstance = nullptr;

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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditMode|UI")
	TSubclassOf<UEditModeLayoutWidget> EditModeLayoutWidgetClass;

	UPROPERTY()
	TObjectPtr<UEditModeLayoutWidget> EditModeLayoutWidgetInstance = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditMode|UI")
	TSubclassOf<UEditModeToolbarWidget> EditModeToolbarWidgetClass;

	UPROPERTY()
	TObjectPtr<UEditModeToolbarWidget> EditModeToolbarInstance = nullptr;

	void ShowEditModeToolbar(bool bVisible);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditMode|UI")
	TSubclassOf<UFurnitureSizePanelWidget> FurnitureSizePanelWidgetClass;

	UPROPERTY()
	TObjectPtr<UFurnitureSizePanelWidget> FurnitureSizePanelInstance = nullptr;

	// Furniture가 nullptr이면 패널을 숨기고, 아니면 보여주면서 RefreshForFurniture를 호출한다
	UFUNCTION(BlueprintCallable, Category = "EditMode|UI")
	void ShowFurnitureSizePanel(AFurniture* Furniture);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditMode|UI")
	TSubclassOf<ULightAttributesPanelWidget> LightAttributesPanelWidgetClass;

	UPROPERTY()
	TObjectPtr<ULightAttributesPanelWidget> LightAttributesPanelInstance = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditMode|UI")
	TSubclassOf<UMaterialAttributesPanelWidget> MaterialAttributesPanelWidgetClass;

	UPROPERTY()
	TObjectPtr<UMaterialAttributesPanelWidget> MaterialAttributesPanelInstance = nullptr;

	UFUNCTION(BlueprintCallable, Category = "EditMode|UI")
	void ShowMaterialAttributesPanel(bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "EditMode|UI")
	void RefreshMaterialAttributesPanel(const FMaterialDataRow& MaterialData);
	
	UFUNCTION(BlueprintCallable, Category = "EditMode|UI")
	void ResetMaterialAttributesPanel();
	
	// Furniture가 ALightFixture가 아니거나 nullptr이면 패널을 숨기고, 맞으면 보여주면서 RefreshForFurniture를 호출한다
	UFUNCTION(BlueprintCallable, Category = "EditMode|UI")
	void ShowLightAttributesPanel(AFurniture* Furniture);

	// ===== View Mode UI =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|UI")
	TSubclassOf<UInteRealMinimap> MinimapWidgetClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|UI")
	TSubclassOf<UEnvironmentPanel> EnvironmentPanelClass;

	UPROPERTY()
	TObjectPtr<UInteRealMinimap> MinimapWidgetInstance = nullptr;
	
	UPROPERTY()
	TObjectPtr<UEnvironmentPanel> EnvironmentPanelInstance = nullptr;

private:
	UFUNCTION()
	void OnFloorPlanDataReady(const FHarnessFloorData& InFloorData);
};
