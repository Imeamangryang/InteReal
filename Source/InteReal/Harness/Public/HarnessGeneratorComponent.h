#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "HarnessData.h"
#include "HarnessGeneratorComponent.generated.h"

class UDynamicMeshComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class INTEREAL_API UHarnessGeneratorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHarnessGeneratorComponent();

    // DataAsset 대신 DataTable을 직접 참조합니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness")
    TObjectPtr<UDataTable> StyleDataTable = nullptr;

    // 예외 처리를 위한 기본 방어용 머티리얼
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness")
    TObjectPtr<UMaterialInterface> DefaultFallbackMaterial = nullptr;

    // 에디터에서 즉시 끄고 켤 수 있는 인테리어 조명 토글
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness|Lighting")
    bool bEnableInteriorLights = true;
    
    UFUNCTION(BlueprintCallable, Category="Harness")
    void BuildHarness(const FHarnessFloorData& FloorData);

    UFUNCTION(BlueprintCallable, Category="Harness")
    void ClearHarness();
    
    TMap<FString, FVector2D> VertexCache;
    TMap<FString, FTopologyHalfEdge> EdgeCache;
private:
    void BuildTopologyCaches(const FHarnessFloorData& FloorData);
    void AssembleStructuralWalls(const FHarnessFloorData& FloorData);
    void FabricateDynamicPlanes(const FHarnessFloorData& FloorData);
    void InstallOpeningComponents(const FHarnessFloorData& FloorData);

    UPROPERTY(Transient)
    TArray<TObjectPtr<UActorComponent>> SpawnedComponents;
    
    // 텔레포트 시 방 좌표를 찾기 위해 원본 JSON 데이터를 저장해둘 멤버 변수 추가
    FHarnessFloorData CachedFloorData;
    
    // 💡 각 방의 중앙에 임시 조명을 배치하는 함수
    void InstallInteriorLights(const FHarnessFloorData& FloorData);
public:
    // 카메라나 위젯 등 외부 모듈에서 도면의 전체 크기를 알 수 있도록 Bounds 반환
    UFUNCTION(BlueprintCallable, Category="Harness|Data")
    void GetFloorBounds(FVector2D& OutMin, FVector2D& OutMax) const;
};