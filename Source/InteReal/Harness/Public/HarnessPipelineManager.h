#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HarnessPipelineManager.generated.h"

class UHarnessNetworkComponent;
class UHarnessSaveManagerComponent;
class UHarnessGeneratorComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPipelineLoadFinished);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class INTEREAL_API UHarnessPipelineManager : public UActorComponent
{
	GENERATED_BODY()

public:	
	UHarnessPipelineManager();

	UFUNCTION(BlueprintCallable, Category="Harness|Pipeline")
	void InitializePipeline(UHarnessNetworkComponent* InNetwork, UHarnessSaveManagerComponent* InSaveManager, UHarnessGeneratorComponent* InGenerator);

	UFUNCTION(BlueprintCallable, Category="Harness|Pipeline")
	void LoadProject(const FString& PlanId);

	UFUNCTION(BlueprintCallable, Category="Harness|Pipeline")
	void SaveCurrentProject();

	UPROPERTY(BlueprintAssignable, Category="Harness|Pipeline")
	FOnPipelineLoadFinished OnPipelineLoadFinished;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UHarnessNetworkComponent> NetworkComp;

	UPROPERTY()
	TObjectPtr<UHarnessSaveManagerComponent> SaveManagerComp;

	UPROPERTY()
	TObjectPtr<UHarnessGeneratorComponent> GeneratorComp;

	FString CurrentPlanId;

	UFUNCTION()
	void OnBaseDownloaded(const FString& BaseJson);

	UFUNCTION()
	void OnDeltaDownloaded(const FString& DeltaJson);
};
