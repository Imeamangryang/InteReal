#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HarnessPipelineManager.generated.h"

class UHarnessNetworkComponent;
class UHarnessSaveManagerComponent;
class UHarnessGeneratorComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPipelineLoadFinished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWorldStateChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFloorPlanDataReady, const FHarnessFloorData&, FloorData);

/**
 * 프로젝트 로드/저장 및 전체 파이프라인을 총괄하는 서브시스템
 */
UCLASS()
class INTEREAL_API UHarnessPipelineManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:	
	// 서브시스템 초기화/해제 (생성자 대신 사용)
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category="Harness|Pipeline")
	void InitializePipeline(UHarnessNetworkComponent* InNetwork, UHarnessSaveManagerComponent* InSaveManager, UHarnessGeneratorComponent* InGenerator);

	UFUNCTION(BlueprintCallable, Category="Harness|Pipeline")
	void LoadProject(const FString& PlanId);

	UFUNCTION(BlueprintCallable, Category="Harness|Pipeline")
	void SaveCurrentProject();

	// 월드 상태(가구, 재질 등)가 변경되었음을 알리는 이벤트
	UFUNCTION(BlueprintCallable, Category="Harness|Pipeline")
	void BroadcastWorldStateChanged();

	UFUNCTION(BlueprintPure, Category="Harness|Pipeline")
	UHarnessNetworkComponent* GetNetworkComp() const { return NetworkComp; }

	UFUNCTION(BlueprintPure, Category="Harness|Pipeline")
	UHarnessSaveManagerComponent* GetSaveManagerComp() const { return SaveManagerComp; }

	UFUNCTION(BlueprintPure, Category="Harness|Pipeline")
	UHarnessGeneratorComponent* GetGeneratorComp() const { return GeneratorComp; }

	UPROPERTY(BlueprintAssignable, Category="Harness|Pipeline")
	FOnPipelineLoadFinished OnPipelineLoadFinished;

	UPROPERTY(BlueprintAssignable, Category="Harness|Pipeline")
	FOnWorldStateChanged OnWorldStateChanged;
	
	UPROPERTY(BlueprintAssignable, Category="Harness|Pipeline")
	FOnFloorPlanDataReady OnFloorPlanDataReady;

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
