#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "InteReal/Network/InteRealDataTypes.h"
#include "InteRealPlanViewModel.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlanListUpdated, bool, bSuccess, const FUnrealPlanListResponse&, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnProjectListUpdated, bool, bSuccess, const FUnrealProjectListResponse&, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDeltaVersionListUpdated, bool, bSuccess, const FUnrealDeltaVersionListResponse&, Response);

/**
 * InteReal 프로젝트의 모든 네트워크 API와 UI 상태를 중계하는 마스터 ViewModel
 */
UCLASS(BlueprintType)
class INTEREAL_API UInteRealPlanViewModel : public UMVVMViewModelBase
{
    GENERATED_BODY()

public:
    /** C++ 클래스에서 데이터 수신을 감지하기 위한 이벤트 */
    UPROPERTY(BlueprintAssignable, Category = "InteReal|UI")
    FOnPlanListUpdated OnPlanListUpdated;

    UPROPERTY(BlueprintAssignable, Category = "InteReal|UI")
    FOnProjectListUpdated OnProjectListUpdated;

    UPROPERTY(BlueprintAssignable, Category = "InteReal|UI")
    FOnDeltaVersionListUpdated OnDeltaVersionListUpdated;

    // --- UI 바인딩용 프로퍼티 (FieldNotify) ---

    /** 현재 선택/활성화된 도면 정보 */
    UPROPERTY(BlueprintReadWrite, FieldNotify, Setter = "SetCurrentPlan", Getter = "GetCurrentPlan", Category = "InteReal|UI")
    FUnrealPlanItem CurrentPlan;
    void SetCurrentPlan(FUnrealPlanItem InPlan);
    FUnrealPlanItem GetCurrentPlan() const { return CurrentPlan; }

    /** 활성화된 버전 */
    UPROPERTY(BlueprintReadWrite, FieldNotify, Setter = "SetActiveVersion", Getter = "GetActiveVersion", Category = "InteReal|UI")
    int32 ActiveVersion = 0;
    void SetActiveVersion(int32 InVersion);
    int32 GetActiveVersion() const { return ActiveVersion; }

    /** 네트워크 처리 중 여부 (로딩 바 연동) */
    UPROPERTY(BlueprintReadWrite, FieldNotify, Setter = "SetIsBusy", Getter = "GetIsBusy", Category = "InteReal|UI")
    bool bIsBusy = false;
    void SetIsBusy(bool bInBusy);
    bool GetIsBusy() const { return bIsBusy; }

    /** 마지막 수신된 에셋 리스트 */
    UPROPERTY(BlueprintReadWrite, FieldNotify, Setter = "SetAssetList", Getter = "GetAssetList", Category = "InteReal|UI")
    FUnrealAssetListResponse AssetList;
    void SetAssetList(FUnrealAssetListResponse InRes);
    FUnrealAssetListResponse GetAssetList() const { return AssetList; }

    /** 마지막 수신된 도면 리스트 */
    UPROPERTY(BlueprintReadWrite, FieldNotify, Setter = "SetPlanList", Getter = "GetPlanList", Category = "InteReal|UI")
    FUnrealPlanListResponse PlanList;
    void SetPlanList(FUnrealPlanListResponse InRes);
    FUnrealPlanListResponse GetPlanList() const { return PlanList; }

    /** 마지막 수신된 프로젝트 리스트 */
    UPROPERTY(BlueprintReadWrite, FieldNotify, Setter = "SetProjectList", Getter = "GetProjectList", Category = "InteReal|UI")
    FUnrealProjectListResponse ProjectList;
    void SetProjectList(FUnrealProjectListResponse InRes);
    FUnrealProjectListResponse GetProjectList() const { return ProjectList; }

    UPROPERTY(BlueprintReadWrite, FieldNotify, Setter = "SetDeltaVersionList", Getter = "GetDeltaVersionList", Category = "InteReal|UI")
    FUnrealDeltaVersionListResponse DeltaVersionList;
    void SetDeltaVersionList(FUnrealDeltaVersionListResponse InRes);
    FUnrealDeltaVersionListResponse GetDeltaVersionList() const { return DeltaVersionList; }

    // --- API Wrapper Functions ---

    /** 1. 에셋 전체 목록 조회 */
    UFUNCTION(BlueprintCallable, Category = "InteReal|UI")
    void FetchAllAssets(int32 Skip = 0, int32 Limit = 50);

    /** 2. 견적 기반 에셋 조회 */
    UFUNCTION(BlueprintCallable, Category = "InteReal|UI")
    void FetchAssetsByEstimate(int32 EstimateId);

    /** 3. 유저 기반 에셋 조회 */
    UFUNCTION(BlueprintCallable, Category = "InteReal|UI")
    void FetchAssetsByUser(int32 UserId);

    /** 4. 도면 목록 조회 (검색/필터) */
    UFUNCTION(BlueprintCallable, Category = "InteReal|UI")
    void FetchPlanList(const FUnrealPlanSearchParams& Params);

    UFUNCTION(BlueprintCallable, Category = "InteReal|UI")
    void FetchExecutablePlanList(const FUnrealPlanSearchParams& Params);

    UFUNCTION(BlueprintCallable, Category = "InteReal|UI")
    void FetchProjectList();

    UFUNCTION(BlueprintCallable, Category = "InteReal|UI")
    void FetchProjectPlanList(const FString& ProjectId);

    /** 5. 하네스 파이프라인 통합: 특정 도면 시작 (Base -> Delta 자동 연쇄 로드) */
    UFUNCTION(BlueprintCallable, Category = "InteReal|UI")
    void LoadPlanProject(const FUnrealPlanItem& PlanItem);

    UFUNCTION(BlueprintCallable, Category = "InteReal|UI")
    void LoadPlanTopology(const FUnrealPlanItem& PlanItem);

    UFUNCTION(BlueprintCallable, Category = "InteReal|UI")
    void GenerateUeTopologyJson(int32 EditableFloorplanId, const FUeTopologyExportRequest& ExportParams);

    UFUNCTION(BlueprintCallable, Category = "InteReal|UI")
    void DownloadUeTopologyJson(int32 EditableFloorplanId, const FUeTopologyExportRequest& ExportParams);

    /** 6. 최신 변경사항(Delta)만 별도 로드 */
    UFUNCTION(BlueprintCallable, Category = "InteReal|UI")
    void RefreshLatestDelta();

    UFUNCTION(BlueprintCallable, Category = "InteReal|UI")
    void FetchDeltaVersionList(int32 PlanId);

    /** 특정 버전 Delta를 가져오기 위한 래퍼 함수 */
    UFUNCTION(BlueprintCallable, Category = "InteReal|UI")
    void LoadDeltaByVersion(int32 Version);

    UFUNCTION(BlueprintCallable, Category = "InteReal|UI")
    void LoadDeltaVersion(const FUnrealDeltaVersionItem& VersionItem);

    /** 7. 현재 도면 상태 저장 (Delta POST) */
    UFUNCTION(BlueprintCallable, Category = "InteReal|UI")
    void SaveCurrentState(const FString& DeltaJson);

    /** 비교를 위한 이전 버전 Delta 상태 (메모리 보관) */
    UPROPERTY(BlueprintReadWrite, Category = "InteReal|State")
    FString PreviousVersionDeltaJson;

    /** 비교를 위한 현재 버전 Delta 상태 (메모리 보관) */
    UPROPERTY(BlueprintReadWrite, Category = "InteReal|State")
    FString CurrentVersionDeltaJson;

    /** 두 버전 비교 로직 (Diff 뼈대) */
    UFUNCTION(BlueprintCallable, Category = "InteReal|UI")
    void CompareVersions();

private:
    // 내부 델리게이트 콜백
    UFUNCTION()
    void OnAssetsReceived(bool bSuccess, const FUnrealAssetListResponse& Response);

    UFUNCTION()
    void OnPlansReceived(bool bSuccess, const FUnrealPlanListResponse& Response);

    UFUNCTION()
    void OnProjectsReceived(bool bSuccess, const FUnrealProjectListResponse& Response);

    UFUNCTION()
    void OnDeltaVersionsReceived(bool bSuccess, const FUnrealDeltaVersionListResponse& Response);

    UFUNCTION()
    void OnBaseTopologyReceived(bool bSuccess, const FString& TopologyJson);

    UFUNCTION()
    void OnDeltaReceived(bool bSuccess, const FString& DeltaJson);

    UFUNCTION()
    void OnDeltaSaved(bool bSuccess, const FUnrealOkResponse& Response);

    /** 서브시스템 헬퍼 */
    class UInteRealNetworkSubsystem* GetNetworkSubsystem() const;

    bool bLoadLatestDeltaAfterTopology = false;
};
