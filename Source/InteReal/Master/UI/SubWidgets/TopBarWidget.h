#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteReal/ViewMode/ViewModeData.h"
#include "InteReal/Network/ViewModel/InteRealPlanViewModel.h"
#include "InteReal/Master/UI/Components/BaseComboBox.h"
#include "TopBarWidget.generated.h"

UCLASS()
class INTEREAL_API UTopBarWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "ViewMode")
	void ChangeViewMode(EHarnessViewMode NewMode);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBaseComboBox> ComboBox_PlanList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBaseComboBox> ComboBox_VersionList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UIconTextButtonWidget> IconTextButton_Capture;

	UFUNCTION()
	void OnPlanListUpdated(bool bSuccess, const FUnrealPlanListResponse& Response);

	UFUNCTION()
	void OnPlanSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnVersionSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleCaptureClicked(FName ButtonId, class UIconTextButtonWidget* ButtonWidget);

private:
	UPROPERTY()
	TObjectPtr<class UInteRealPlanViewModel> PlanViewModel;

	TMap<FString, FUnrealPlanItem> PlanMap;
	int32 CurrentSelectedPlanId = 0;

	class UInteRealPlanViewModel* GetPlanViewModel();
	void RefreshVersionList(int32 MaxVersion);
};
