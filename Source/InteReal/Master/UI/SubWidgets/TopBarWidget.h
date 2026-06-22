#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteReal/ViewMode/ViewModeData.h"
#include "InteReal/Network/ViewModel/InteRealPlanViewModel.h"
#include "InteReal/Master/UI/Controllers/SearchListControllers.h"
#include "TopBarWidget.generated.h"

class USearchListWidget;

UCLASS()
class INTEREAL_API UTopBarWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "ViewMode")
	void ChangeViewMode(EHarnessViewMode NewMode);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USearchListWidget> SearchList_Project;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USearchListWidget> SearchList_Plan;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USearchListWidget> SearchList_Version;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UIconTextButtonWidget> IconTextButton_Capture;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UButton> Btn_Save;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UButton> Btn_SaveAsNewVersion;

	UFUNCTION()
	void OnProjectSelected(const FUnrealProjectItem& ProjectItem);

	UFUNCTION()
	void OnPlanSelected(const FUnrealPlanItem& PlanItem);

	UFUNCTION()
	void OnVersionSelected(const FUnrealDeltaVersionItem& VersionItem);

	UFUNCTION()
	void HandleCaptureClicked(FName ButtonId, class UIconTextButtonWidget* ButtonWidget);

	UFUNCTION()
	void HandleSaveClicked();

	UFUNCTION()
	void HandleSaveAsNewVersionClicked();

	UFUNCTION()
	void HandlePipelineSaveFinished(bool bSuccess, const FUnrealOkResponse& Response);

private:
	UPROPERTY()
	TObjectPtr<class UInteRealPlanViewModel> PlanViewModel;

	UPROPERTY()
	TObjectPtr<UInteRealProjectListController> ProjectController;

	UPROPERTY()
	TObjectPtr<UInteRealPlanListController> PlanController;

	UPROPERTY()
	TObjectPtr<UInteRealVersionListController> VersionController;

	class UInteRealPlanViewModel* GetPlanViewModel();
	bool bLastSaveRequestedNewVersion = false;
};
