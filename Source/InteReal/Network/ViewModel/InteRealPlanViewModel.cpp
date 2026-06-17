#include "InteRealPlanViewModel.h"

#include "InteReal/Network/InteRealNetworkSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Public/HarnessPipelineManager.h"

// --- Setters ---
void UInteRealPlanViewModel::SetCurrentPlan(FUnrealPlanItem InPlan) { UE_MVVM_SET_PROPERTY_VALUE(CurrentPlan, InPlan); }
void UInteRealPlanViewModel::SetActiveVersion(int32 InVersion) { UE_MVVM_SET_PROPERTY_VALUE(ActiveVersion, InVersion); }
void UInteRealPlanViewModel::SetIsBusy(bool bInBusy) { UE_MVVM_SET_PROPERTY_VALUE(bIsBusy, bInBusy); }
void UInteRealPlanViewModel::SetAssetList(FUnrealAssetListResponse InRes) { UE_MVVM_SET_PROPERTY_VALUE(AssetList, InRes); }
void UInteRealPlanViewModel::SetPlanList(FUnrealPlanListResponse InRes) { UE_MVVM_SET_PROPERTY_VALUE(PlanList, InRes); }
void UInteRealPlanViewModel::SetDeltaVersionList(FUnrealDeltaVersionListResponse InRes) { UE_MVVM_SET_PROPERTY_VALUE(DeltaVersionList, InRes); }

UInteRealNetworkSubsystem* UInteRealPlanViewModel::GetNetworkSubsystem() const
{
    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    return GI ? GI->GetSubsystem<UInteRealNetworkSubsystem>() : nullptr;
}

// --- API Wrappers ---

void UInteRealPlanViewModel::FetchPlanList(const FUnrealPlanSearchParams& Params)
{
    FetchExecutablePlanList(Params);
}

void UInteRealPlanViewModel::FetchExecutablePlanList(const FUnrealPlanSearchParams& Params)
{
    auto* Network = GetNetworkSubsystem();
    if (!Network) return;
    SetIsBusy(true);
    FOnPlansReceived Delegate;
    Delegate.BindDynamic(this, &UInteRealPlanViewModel::OnPlansReceived);
    Network->FetchExecutablePlans(Params, Delegate);
}

void UInteRealPlanViewModel::LoadPlanProject(const FUnrealPlanItem& PlanItem)
{
    bLoadLatestDeltaAfterTopology = true;
    LoadPlanTopology(PlanItem);
}

void UInteRealPlanViewModel::LoadPlanTopology(const FUnrealPlanItem& PlanItem)
{
    SetCurrentPlan(PlanItem);
    SetIsBusy(true);
    
    if (auto* Pipeline = GetWorld()->GetSubsystem<UHarnessPipelineManager>()) {
        Pipeline->LoadProject(PlanItem.id);
    }

    FOnTopologyReceived Delegate;
    Delegate.BindDynamic(this, &UInteRealPlanViewModel::OnBaseTopologyReceived);
    if (UInteRealNetworkSubsystem* Network = GetNetworkSubsystem())
    {
        Network->FetchUeTopology(PlanItem.id, FUeTopologyExportRequest(), Delegate);
    }
    else
    {
        bLoadLatestDeltaAfterTopology = false;
        SetIsBusy(false);
    }
}

void UInteRealPlanViewModel::RefreshLatestDelta()
{
    if (CurrentPlan.id == 0) return;
    UInteRealNetworkSubsystem* Network = GetNetworkSubsystem();
    if (!Network) return;

    SetIsBusy(true);
    FOnDeltaReceived Delegate;
    Delegate.BindDynamic(this, &UInteRealPlanViewModel::OnDeltaReceived);
    Network->FetchLatestDelta(CurrentPlan.id, Delegate);
}

void UInteRealPlanViewModel::FetchDeltaVersionList(int32 PlanId)
{
    UInteRealNetworkSubsystem* Network = GetNetworkSubsystem();
    if (!Network || PlanId == 0) return;

    SetIsBusy(true);
    FOnDeltaVersionsReceived Delegate;
    Delegate.BindDynamic(this, &UInteRealPlanViewModel::OnDeltaVersionsReceived);
    Network->FetchDeltaVersions(PlanId, Delegate);
}

void UInteRealPlanViewModel::LoadDeltaByVersion(int32 Version)
{
    if (CurrentPlan.id == 0) return;
    UInteRealNetworkSubsystem* Network = GetNetworkSubsystem();
    if (!Network) return;

    SetIsBusy(true);
    FOnDeltaReceived Delegate;
    Delegate.BindDynamic(this, &UInteRealPlanViewModel::OnDeltaReceived);
    Network->FetchDeltaByVersion(CurrentPlan.id, Version, Delegate);
}

void UInteRealPlanViewModel::LoadDeltaVersion(const FUnrealDeltaVersionItem& VersionItem)
{
    const int32 PlanId = VersionItem.plan_id != 0 ? VersionItem.plan_id : CurrentPlan.id;
    if (PlanId == 0) return;

    SetActiveVersion(VersionItem.version);

    UInteRealNetworkSubsystem* Network = GetNetworkSubsystem();
    if (!Network) return;

    SetIsBusy(true);
    FOnDeltaReceived Delegate;
    Delegate.BindDynamic(this, &UInteRealPlanViewModel::OnDeltaReceived);

    if (VersionItem.version > 0)
    {
        Network->FetchDeltaByVersion(PlanId, VersionItem.version, Delegate);
    }
    else
    {
        Network->FetchLatestDelta(PlanId, Delegate);
    }
}

void UInteRealPlanViewModel::SaveCurrentState(const FString& DeltaJson)
{
    UInteRealNetworkSubsystem* Network = GetNetworkSubsystem();
    if (!Network || CurrentPlan.id == 0) return;

    SetIsBusy(true);
    FOnDeltaSaved Delegate;
    Delegate.BindDynamic(this, &UInteRealPlanViewModel::OnDeltaSaved);
    Network->SaveDelta(CurrentPlan.id, DeltaJson, Delegate);
}

void UInteRealPlanViewModel::CompareVersions()
{
    // TODO: Compare PreviousVersionDeltaJson and CurrentVersionDeltaJson
    // Extract FInteriorDeltaList from both JSON strings and perform property-wise diff.
    // E.g., Identify moved furniture, changed materials, etc.
    UE_LOG(LogTemp, Log, TEXT("[InteReal] CompareVersions called. Prev: %d chars, Curr: %d chars"), PreviousVersionDeltaJson.Len(), CurrentVersionDeltaJson.Len());
}

// --- Callbacks ---

void UInteRealPlanViewModel::OnPlansReceived(bool bSuccess, const FUnrealPlanListResponse& Response)
{
    SetIsBusy(false);
    if (bSuccess)
    {
        SetPlanList(Response);
        UE_LOG(LogTemp, Log, TEXT("[InteReal] ViewModel: Successfully received %d plans."), Response.items.Num());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[InteReal] ViewModel: Failed to fetch plan list. Check server or mock data."));
    }

    // C++ 리스너(HUD 등)에게 데이터 도착 알림
    OnPlanListUpdated.Broadcast(bSuccess, Response);
}

void UInteRealPlanViewModel::OnDeltaVersionsReceived(bool bSuccess, const FUnrealDeltaVersionListResponse& Response)
{
    SetIsBusy(false);
    if (bSuccess)
    {
        SetDeltaVersionList(Response);
        UE_LOG(LogTemp, Log, TEXT("[InteReal] ViewModel: Successfully received %d delta versions."), Response.items.Num());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[InteReal] ViewModel: Failed to fetch delta versions."));
    }

    OnDeltaVersionListUpdated.Broadcast(bSuccess, Response);
}

void UInteRealPlanViewModel::OnBaseTopologyReceived(bool bSuccess, const FString& TopologyJson)
{
    if (bSuccess) {
        if (auto* Pipeline = GetWorld()->GetSubsystem<UHarnessPipelineManager>()) {
            Pipeline->AssembleBase(TopologyJson);
        }
        if (!bLoadLatestDeltaAfterTopology)
        {
            SetIsBusy(false);
            return;
        }
        bLoadLatestDeltaAfterTopology = false;
        RefreshLatestDelta(); // 2단계: 가구 데이터 로드
    } else { bLoadLatestDeltaAfterTopology = false; SetIsBusy(false); }
}

void UInteRealPlanViewModel::OnDeltaReceived(bool bSuccess, const FString& DeltaJson)
{
    SetIsBusy(false);
    if (bSuccess) {
        PreviousVersionDeltaJson = CurrentVersionDeltaJson;
        CurrentVersionDeltaJson = DeltaJson;
        if (auto* Pipeline = GetWorld()->GetSubsystem<UHarnessPipelineManager>()) {
            Pipeline->ApplyDelta(DeltaJson);
        }
    }
}

void UInteRealPlanViewModel::OnDeltaSaved(bool bSuccess, const FUnrealOkResponse& Response)
{
    SetIsBusy(false);
    if (bSuccess && Response.ok) SetActiveVersion(GetActiveVersion() + 1);
}

// (나머지 에셋 API들도 동일하게 Subsystem 호출하도록 구현됨)
void UInteRealPlanViewModel::FetchAllAssets(int32 Skip, int32 Limit) {
    FOnAssetsReceived D; D.BindDynamic(this, &UInteRealPlanViewModel::OnAssetsReceived);
    GetNetworkSubsystem()->FetchAllAssets(Skip, Limit, D);
}
void UInteRealPlanViewModel::FetchAssetsByEstimate(int32 id) {
    FOnAssetsReceived D; D.BindDynamic(this, &UInteRealPlanViewModel::OnAssetsReceived);
    GetNetworkSubsystem()->FetchAssetsByEstimate(id, 0, 100, D);
}
void UInteRealPlanViewModel::FetchAssetsByUser(int32 id) {
    FOnAssetsReceived D; D.BindDynamic(this, &UInteRealPlanViewModel::OnAssetsReceived);
    GetNetworkSubsystem()->FetchAssetsByUser(id, 0, 100, D);
}
void UInteRealPlanViewModel::OnAssetsReceived(bool bS, const FUnrealAssetListResponse& R) {
    SetIsBusy(false); if(bS) SetAssetList(R);
}
