#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HarnessData.h" // FHarnessFloorData 사용을 위함
#include "HarnessPipelineManager.generated.h"

class UHarnessSaveManagerComponent;
class UHarnessGeneratorComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPipelineLoadFinished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWorldStateChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFloorPlanDataReady, const FHarnessFloorData&, FloorData);

/**
 * 월드 액터 생성 및 배치를 담당하는 엔진 서브시스템
 */
UCLASS()
class INTEREAL_API UHarnessPipelineManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:	
	void InitializePipeline(UHarnessSaveManagerComponent* InSaveManager, UHarnessGeneratorComponent* InGenerator);

	/** 컴포넌트 접근자 (기존 시스템 호환용) */
	UFUNCTION(BlueprintPure, Category="Harness|Pipeline")
	UHarnessGeneratorComponent* GetGeneratorComp() const { return GeneratorComp; }

	UFUNCTION(BlueprintPure, Category="Harness|Pipeline")
	UHarnessSaveManagerComponent* GetSaveManagerComp() const { return SaveManagerComp; }

	/** 월드 상태 초기화 */
	UFUNCTION(BlueprintCallable, Category="Harness|Pipeline")
	void ClearWorld();

	/** 도면 로드 (기존 시스템 호출 대응용 - 이제 내부적으로 ViewModel 등을 사용하거나 직접 저장할 수 있음) */
	UFUNCTION(BlueprintCallable, Category="Harness|Pipeline")
	void LoadProject(int32 PlanId);

	/** 현재 프로젝트 상태 저장 */
	UFUNCTION(BlueprintCallable, Category="Harness|Pipeline")
	void SaveCurrentProject();

	/** 수신된 Base JSON으로 벽/바닥 생성 */
	UFUNCTION(BlueprintCallable, Category="Harness|Pipeline")
	void AssembleBase(const FString& BaseJson);

	/** 수신된 Delta JSON으로 가구 배치 */
	UFUNCTION(BlueprintCallable, Category="Harness|Pipeline")
	void ApplyDelta(const FString& DeltaJson);

	UPROPERTY(BlueprintAssignable, Category="Harness|Pipeline")
	FOnPipelineLoadFinished OnPipelineLoadFinished;

	UPROPERTY(BlueprintAssignable, Category="Harness|Pipeline")
	FOnWorldStateChanged OnWorldStateChanged;

	UPROPERTY(BlueprintAssignable, Category="Harness|Pipeline")
	FOnFloorPlanDataReady OnFloorPlanDataReady;

private:
	UPROPERTY()
	TObjectPtr<UHarnessSaveManagerComponent> SaveManagerComp;

	UPROPERTY()
	TObjectPtr<UHarnessGeneratorComponent> GeneratorComp;

	int32 CurrentPlanId = 0;
};
