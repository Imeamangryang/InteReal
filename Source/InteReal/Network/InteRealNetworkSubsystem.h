#pragma once

#include "CoreMinimal.h"
#include "InteRealDataTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/IHttpRequest.h"
#include "InteRealNetworkSubsystem.generated.h"

/** API 응답을 위한 델리게이트 정의 */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnAssetsReceived, bool, bSuccess, const FUnrealAssetListResponse&, Response);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnPlansReceived, bool, bSuccess, const FUnrealPlanListResponse&, Response);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnDeltaVersionsReceived, bool, bSuccess, const FUnrealDeltaVersionListResponse&, Response);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnTopologyReceived, bool, bSuccess, const FString&, TopologyJson);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnDeltaReceived, bool, bSuccess, const FString&, DeltaJson);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnDeltaSaved, bool, bSuccess, const FUnrealOkResponse&, Response);

/**
 * InteReal 프로젝트 전용 네트워크 서브시스템
 * 가이드 버전: 1.0 (7개 엔드포인트)
 */
UCLASS()
class INTEREAL_API UInteRealNetworkSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Mock Data 사용 여부 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteReal|Network")
    bool bUseMockData = true;

    /** 서버 주소 */
    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString ServerUrl = TEXT("http://15.164.49.175:8000");

    /** JWT 인증 토큰 */
    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString AuthToken = TEXT("mock_jwt_token");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteReal|Network|Endpoints")
    FString PlansEndpoint = TEXT("/plans");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteReal|Network|Endpoints")
    FString UeTopologyEndpointFormat = TEXT("/plans/{PlanId}/base");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteReal|Network|Endpoints")
    FString LatestDeltaEndpointFormat = TEXT("/plans/{PlanId}/delta");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteReal|Network|Endpoints")
    FString DeltaVersionsEndpointFormat = TEXT("/plans/{PlanId}/delta/versions");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteReal|Network|Endpoints")
    FString DeltaVersionEndpointFormat = TEXT("/plans/{PlanId}/delta/versions/{Version}");

    // --- 1. 에셋 API ---

    /** 에셋 전체 목록 조회 (GET /api/unreal/assets) */
    UFUNCTION(BlueprintCallable, Category = "InteReal|Network")
    void FetchAllAssets(int32 Skip, int32 Limit, FOnAssetsReceived OnComplete);

    /** 특정 견적의 에셋 목록 조회 (GET /api/unreal/estimates/{id}/assets) */
    UFUNCTION(BlueprintCallable, Category = "InteReal|Network")
    void FetchAssetsByEstimate(int32 EstimateId, int32 Skip, int32 Limit, FOnAssetsReceived OnComplete);

    /** 특정 유저의 에셋 목록 조회 (GET /api/unreal/users/{id}/assets) */
    UFUNCTION(BlueprintCallable, Category = "InteReal|Network")
    void FetchAssetsByUser(int32 UserId, int32 Skip, int32 Limit, FOnAssetsReceived OnComplete);

    // --- 2. 도면 API ---

    /** 언리얼 실행 가능 도면 목록 조회 (GET /api/unreal/plans) */
    UFUNCTION(BlueprintCallable, Category = "InteReal|Network")
    void FetchPlans(const FUnrealPlanSearchParams& Params, FOnPlansReceived OnComplete);

    UFUNCTION(BlueprintCallable, Category = "InteReal|Network")
    void FetchExecutablePlans(const FUnrealPlanSearchParams& Params, FOnPlansReceived OnComplete);

    /** Base 토폴로지 JSON 반환 (GET /api/unreal/plans/{id}/base) */
    UFUNCTION(BlueprintCallable, Category = "InteReal|Network")
    void FetchBaseTopology(int32 PlanId, const FUeTopologyExportRequest& ExportParams, FOnTopologyReceived OnComplete);

    UFUNCTION(BlueprintCallable, Category = "InteReal|Network")
    void FetchUeTopology(int32 PlanId, const FUeTopologyExportRequest& ExportParams, FOnTopologyReceived OnComplete);

    // --- 3. Delta API ---

    UFUNCTION(BlueprintCallable, Category = "InteReal|Network")
    void FetchDeltaVersions(int32 PlanId, FOnDeltaVersionsReceived OnComplete);

    /** 최신 Delta JSON 반환 (GET /api/unreal/plans/{id}/delta) */
    UFUNCTION(BlueprintCallable, Category = "InteReal|Network")
    void FetchLatestDelta(int32 PlanId, FOnDeltaReceived OnComplete);

    /** 특정 버전의 Delta JSON 반환 (GET /api/unreal/plans/{id}/delta/{version}) */
    UFUNCTION(BlueprintCallable, Category = "InteReal|Network")
    void FetchDeltaByVersion(int32 PlanId, int32 Version, FOnDeltaReceived OnComplete);

    /** Delta JSON 저장 (POST /api/unreal/plans/{id}/delta) */
    UFUNCTION(BlueprintCallable, Category = "InteReal|Network")
    void SaveDelta(int32 PlanId, const FString& DeltaJson, FOnDeltaSaved OnComplete);

private:
    /** 공통 HTTP 요청 생성 로직 */
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateRequest(const FString& Verb, const FString& Endpoint);

    FString GetBaseApiUrl() const { return ServerUrl + TEXT("/api/unreal"); }
    FString BuildEndpoint(FString EndpointFormat, int32 PlanId, int32 Version = INDEX_NONE) const;
};
