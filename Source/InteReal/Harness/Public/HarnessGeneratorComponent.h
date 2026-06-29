#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "HarnessData.h"
#include "HarnessGeneratorComponent.generated.h"


UENUM(BlueprintType)
enum class EHarnessSpaceBoundaryPolicy : uint8
{
    AsProvided UMETA(DisplayName="Use space.boundary as provided"),
    ExpandUnderWalls UMETA(DisplayName="Expand floor under wall thickness")
};

class UStaticMesh;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class INTEREAL_API UHarnessGeneratorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHarnessGeneratorComponent();

    // 예외 처리를 위한 기본 방어용 머티리얼
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness")
    TObjectPtr<UMaterialInterface> DefaultFallbackMaterial = nullptr;

    // v3.2 spaces.boundary is currently centerline-based, so expand floor/ceiling slabs under wall thickness by default.

    // 건물 생성 시 opening 데이터(Door/Window)에 자동으로 붙일 기본 메시입니다.
    // 비워두면 opening 컴포넌트만 생성하지 않고, 추후 카탈로그에서 선택한 문/창문 에셋으로 교체할 수 있습니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness|Openings")
    TObjectPtr<UStaticMesh> DefaultDoorMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness|Openings")
    TObjectPtr<UStaticMesh> DefaultWindowMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness|Openings")
    TObjectPtr<UStaticMesh> DefaultEntranceDoorMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness|Openings")
    TObjectPtr<UStaticMesh> DefaultSlidingDoorMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness|Openings")
    bool bGenerateOpeningAssets = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness|Spaces")
    EHarnessSpaceBoundaryPolicy SpaceBoundaryPolicy = EHarnessSpaceBoundaryPolicy::ExpandUnderWalls;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness|Spaces", meta=(ClampMin="0.0", UIMin="0.0", UIMax="20.0"))
    float FloorBoundaryExpansionCm = 10.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness|Spaces")
    bool bLogSpaceBoundaryDiagnostics = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness|Spaces|Validation")
    bool bValidateSpacePolygons = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness|Spaces|Validation", meta=(ClampMin="0.0", UIMin="0.0", UIMax="100.0"))
    float MinSpacePolygonAreaCm2 = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness|Spaces|Validation", meta=(ClampMin="0.0", UIMin="0.0", UIMax="10.0"))
    float MinSpacePolygonEdgeLengthCm = 1.0f;
    
    // 도면 생성 요청 진입점 (기존 도면이 있으면 애니메이션 처리 후 내부 함수 호출)
    UFUNCTION(BlueprintCallable, Category="Harness")
    void BuildHarness(const FHarnessFloorData& FloorData);

    UFUNCTION(BlueprintCallable, Category="Harness")
    void ClearHarness();

    UFUNCTION(BlueprintCallable, CallInEditor, Category="Harness")
    void RebuildHarnessWithCurrentScale();

    UFUNCTION(BlueprintCallable, Category="Harness")
    void SetCeilingVisibility(bool bVisible);

    // 💡 천장 높이 동적 수정 기능
    UFUNCTION(BlueprintCallable, Category="Harness")
    void UpdateCeilingHeight(FString FaceId, float NewHeight);
    
    TMap<FString, FVector2D> VertexCache;
    TMap<FString, FTopologyHalfEdge> EdgeCache;

private:
    void RebuildHarnessFromRuntimeData(const FHarnessFloorData& FloorData);

    void BuildTopologyCaches(const FHarnessFloorData& FloorData);
    void AssembleStructuralWalls(const FHarnessFloorData& FloorData);
    void FabricateDynamicPlanes(const FHarnessFloorData& FloorData);
    void InstallOpeningComponents(const FHarnessFloorData& FloorData);
    void AddGeneratedComponentTags(UActorComponent* Component, const FString& ComponentType, const FString& EntityId = FString(), const TArray<FString>& ExtraMetadataTags = TArray<FString>()) const;

    // 생성된 모든 컴포넌트를 추적하여 도면 교체 시 메모리 누수 방지
    UPROPERTY(Transient)
    TArray<TObjectPtr<UActorComponent>> SpawnedComponents;

    // 텔레포트 시 방 좌표를 찾기 위해 원본 JSON 데이터를 저장해둘 멤버 변수
    FHarnessFloorData CachedFloorData;
    FHarnessFloorData SourceFloorData;

public:
    // 카메라나 위젯 등 외부 모듈에서 도면의 전체 크기를 알 수 있도록 Bounds 반환
    UFUNCTION(BlueprintCallable, Category="Harness|Data")
    void GetFloorBounds(FVector2D& OutMin, FVector2D& OutMax) const;

    UFUNCTION(BlueprintPure, Category="Harness|Data")
    FVector GetSafeSpawnLocation() const;

    UFUNCTION(BlueprintPure, Category="Harness|Data")
    const FHarnessFloorData& GetCachedFloorData() const { return CachedFloorData; }
};
