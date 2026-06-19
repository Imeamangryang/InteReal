#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "HarnessData.h"
#include "HarnessGeneratorComponent.generated.h"

// 전방 선언 (헤더 컴파일 속도 최적화)
class UDynamicMeshComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class INTEREAL_API UHarnessGeneratorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHarnessGeneratorComponent();

    // 💡 [추가] 스르륵 사라지고 나타나는 애니메이션 처리를 위한 Tick 오버라이드
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // 예외 처리를 위한 기본 방어용 머티리얼
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness")
    TObjectPtr<UMaterialInterface> DefaultFallbackMaterial = nullptr;

    // 에디터에서 즉시 끄고 켤 수 있는 인테리어 조명 토글
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness|Lighting")
    bool bEnableInteriorLights = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness|Scale", meta=(ClampMin="0.01", UIMin="0.1", UIMax="10.0", DisplayName="Editor Plan Scale"))
    float EditorPlanScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Harness|Scale", meta=(ClampMin="0.01", DisplayName="Additional Plan Scale"))
    float OverallPlanScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness|Scale")
    bool bAutoScaleFromDoorWidth = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness|Scale", meta=(ClampMin="1.0"))
    float DoorReferenceWidthCm = 90.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Harness|Scale")
    float LastAppliedPlanScale = 1.0f;
    
    // 도면 생성 요청 진입점 (기존 도면이 있으면 애니메이션 처리 후 내부 함수 호출)
    UFUNCTION(BlueprintCallable, Category="Harness")
    void BuildHarness(const FHarnessFloorData& FloorData);

    UFUNCTION(BlueprintCallable, Category="Harness")
    void ClearHarness();

    UFUNCTION(BlueprintCallable, CallInEditor, Category="Harness|Scale")
    void RebuildHarnessWithCurrentScale();

    UFUNCTION(BlueprintCallable, Category="Harness|Scale")
    void SetEditorPlanScale(float NewScale, bool bRebuild = true);

    // 💡 천장 높이 동적 수정 기능
    UFUNCTION(BlueprintCallable, Category="Harness")
    void UpdateCeilingHeight(FString FaceId, float NewHeight);
    
    TMap<FString, FVector2D> VertexCache;
    TMap<FString, FTopologyHalfEdge> EdgeCache;
    TArray<FTopologyWallSideMeasurement> WallSideMeasurementCache;
    TArray<FTopologySurfaceMeasurement> SurfaceMeasurementCache;

private:
    float CalculateEffectivePlanScale(const FHarnessFloorData& FloorData) const;
    FHarnessFloorData MakeRuntimeFloorData(const FHarnessFloorData& FloorData, float PlanScale) const;
    void RebuildHarnessFromRuntimeData(const FHarnessFloorData& FloorData);

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    void BuildTopologyCaches(const FHarnessFloorData& FloorData);
    void AssembleStructuralWalls(const FHarnessFloorData& FloorData);
    void FabricateDynamicPlanes(const FHarnessFloorData& FloorData);
    void InstallOpeningComponents(const FHarnessFloorData& FloorData);

    // 생성된 모든 컴포넌트를 추적하여 도면 교체 시 메모리 누수 방지
    UPROPERTY(Transient)
    TArray<TObjectPtr<UActorComponent>> SpawnedComponents;
    
    // 💡 [추가] 스케일 애니메이션(Z축 솟아오름)을 적용할 벽 메쉬 컴포넌트 전용 추적 배열
    UPROPERTY(Transient)
    TArray<TObjectPtr<UDynamicMeshComponent>> AnimatedWalls;

    // 텔레포트 시 방 좌표를 찾기 위해 원본 JSON 데이터를 저장해둘 멤버 변수
    FHarnessFloorData CachedFloorData;
    FHarnessFloorData SourceFloorData;

    // 각 방의 중앙에 임시 조명을 배치하는 함수
    void InstallInteriorLights(const FHarnessFloorData& FloorData);

    // 💡 [추가] 애니메이션 상태 머신 추적 변수들
    bool bIsSpawning = false;       // 새로운 벽이 솟아오르는 중인가?
    float WallAnimationProgress = 0.0f; // 애니메이션 진행도 (0.0 ~ 1.0)

public:
    // 카메라나 위젯 등 외부 모듈에서 도면의 전체 크기를 알 수 있도록 Bounds 반환
    UFUNCTION(BlueprintCallable, Category="Harness|Data")
    void GetFloorBounds(FVector2D& OutMin, FVector2D& OutMax) const;

    UFUNCTION(BlueprintPure, Category="Harness|Data")
    const FHarnessFloorData& GetCachedFloorData() const { return CachedFloorData; }
};
