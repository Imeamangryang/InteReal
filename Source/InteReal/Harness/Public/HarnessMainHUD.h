#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HarnessDataTypes.h"
#include "HarnessMainHUD.generated.h"

class UScrollBox;
class UButton;
class UHarnessPipelineManager;
struct FUnrealPlanListResponse;

/**
 * 버튼 클릭 이벤트를 중계하기 위한 헬퍼 클래스
 */
UCLASS()
class UProjectItemWrapper : public UObject
{
	GENERATED_BODY()
public:
	int32 PlanId;

	UPROPERTY()
	TObjectPtr<class UHarnessMainHUD> OwnerHUD;

	UFUNCTION()
	void OnClick();
};

UCLASS()
class INTEREAL_API UHarnessMainHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Harness|UI")
	void SetupHUD(class UInteRealPlanViewModel* InViewModel);

	void OnProjectButtonClicked(int32 PlanId);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_LoadProjectList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ScrollBox_ProjectList;

private:
	UPROPERTY()
	TObjectPtr<class UInteRealPlanViewModel> PlanViewModel;

	// GC 방지를 위해 Wrapper 객체들을 보관합니다.
	UPROPERTY()
	TArray<TObjectPtr<UProjectItemWrapper>> ProjectItemWrappers;

	UFUNCTION()
	void OnLoadProjectListClicked();

	UFUNCTION()
	void OnPlanListReceived(bool bSuccess, const FUnrealPlanListResponse& Response);
};
