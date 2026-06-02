#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HarnessDataTypes.h"
#include "HarnessMainHUD.generated.h"

class UScrollBox;
class UButton;
class UHarnessNetworkComponent;
class UHarnessPipelineManager;

/**
 * 버튼 클릭 이벤트를 중계하기 위한 헬퍼 클래스
 */
UCLASS()
class UProjectItemWrapper : public UObject
{
	GENERATED_BODY()
public:
	FString PlanId;

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
	void SetupHUD(UHarnessNetworkComponent* InNetwork, UHarnessPipelineManager* InPipeline);

	void OnProjectButtonClicked(FString PlanId);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_LoadProjectList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ScrollBox_ProjectList;

private:
	UPROPERTY()
	TObjectPtr<UHarnessNetworkComponent> NetworkComp;

	UPROPERTY()
	TObjectPtr<UHarnessPipelineManager> PipelineManager;

	// GC 방지를 위해 Wrapper 객체들을 보관합니다.
	UPROPERTY()
	TArray<TObjectPtr<UProjectItemWrapper>> ProjectItemWrappers;

	UFUNCTION()
	void OnLoadProjectListClicked();

	UFUNCTION()
	void OnPlanListReceived(const TArray<FFloorPlanInfo>& PlanList);
};
