#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EditModeLayoutWidget.generated.h"

class UButton;
class USizeBox;
class UInteReal2DFloorPlanViewportWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFloorPlanPanelOpenChangedSignature, bool, bOpen);

UCLASS(BlueprintType, Blueprintable)
class INTEREAL_API UEditModeLayoutWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="InteReal|EditModeLayout")
	void SetFloorPlanPanelOpen(bool bOpen);

	UFUNCTION(BlueprintCallable, Category="InteReal|EditModeLayout")
	void ToggleFloorPlanPanel();

	UFUNCTION(BlueprintCallable, Category="InteReal|EditModeLayout")
	void SetFloorPlanPanelWidth(float NewWidth);

	UFUNCTION(BlueprintCallable, Category="InteReal|EditModeLayout")
	void SetFloorPlan2DWidgetClass(TSubclassOf<UInteReal2DFloorPlanViewportWidget> InWidgetClass);

	UFUNCTION(BlueprintPure, Category="InteReal|EditModeLayout")
	bool IsFloorPlanPanelOpen() const { return bIsFloorPlanPanelOpen; }

	UFUNCTION(BlueprintPure, Category="InteReal|EditModeLayout")
	float GetCurrentFloorPlanPanelWidth() const { return CurrentFloorPlanPanelWidth; }

	UFUNCTION(BlueprintPure, Category="InteReal|EditModeLayout")
	UInteReal2DFloorPlanViewportWidget* GetFloorPlan2DWidget() const { return FloorPlan2DWidget; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category="InteReal|EditModeLayout")
	void BP_OnFloorPlanPanelOpenChanged(bool bOpen);

	UFUNCTION(BlueprintImplementableEvent, Category="InteReal|EditModeLayout")
	void BP_OnFloorPlanPanelWidthChanged(float NewWidth);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal|EditModeLayout|Class")
	TSubclassOf<UInteReal2DFloorPlanViewportWidget> FloorPlan2DWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal|EditModeLayout|Panel")
	bool bIsFloorPlanPanelOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal|EditModeLayout|Panel", meta=(ClampMin="0.0"))
	float OpenPanelWidth = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal|EditModeLayout|Panel", meta=(ClampMin="0.0"))
	float CollapsedPanelWidth = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal|EditModeLayout|Panel", meta=(ClampMin="0.0"))
	float MinOpenPanelWidth = 320.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal|EditModeLayout|Panel", meta=(ClampMin="0.0"))
	float MaxOpenPanelWidth = 1100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal|EditModeLayout|Animation")
	bool bAnimatePanelWidth = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal|EditModeLayout|Animation", meta=(ClampMin="0.0"))
	float PanelWidthInterpSpeed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal|EditModeLayout|FloorPlan")
	bool bAutoFitFloorPlanDrawArea = true;
	
	UPROPERTY(BlueprintAssignable, Category="InteReal|EditModeLayout")
	FOnFloorPlanPanelOpenChangedSignature OnFloorPlanPanelOpenChanged;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION()
	void HandleToggleButtonClicked();

private:
	void ApplyLayout();
	void ApplyFloorPlanDrawArea();
	float GetTargetPanelWidth() const;

private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<USizeBox> FloorPlanPanelSizeBox = nullptr;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> ToggleButton = nullptr;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UInteReal2DFloorPlanViewportWidget> FloorPlan2DWidget = nullptr;

	UPROPERTY(VisibleAnywhere, Category="InteReal|EditModeLayout|Runtime")
	float CurrentFloorPlanPanelWidth = 800.0f;

	bool bPendingInitialDrawArea = true;
};