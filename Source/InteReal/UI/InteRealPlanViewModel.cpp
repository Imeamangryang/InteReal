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

UInteRealNetworkSubsystem* UInteRealPlanViewModel::GetNetworkSubsystem() const
{
    UGameInstance* GI = GetWorld()->GetGameInstance();
    return GI ? GI->GetSubsystem<UInteRealNetworkSubsystem>() : nullptr;
}

// --- API Wrappers ---

void UInteRealPlanViewModel::FetchPlanList(const FUnrealPlanSearchParams& Params)
{
    auto* Network = GetNetworkSubsystem();
    if (!Network) return;
    SetIsBusy(true);
    FOnPlansReceived Delegate;
    Delegate.BindDynamic(this, &UInteRealPlanViewModel::OnPlansReceived);
    Network->FetchPlans(Params, Delegate);
}

void UInteRealPlanViewModel::LoadPlanProject(const FUnrealPlanItem& PlanItem)
{
    SetCurrentPlan(PlanItem);
    SetIsBusy(true);
    
    if (auto* Pipeline = GetWorld()->GetSubsystem<UHarnessPipelineManager>()) {
        Pipeline->ClearWorld();
    }

    FOnTopologyReceived Delegate;
    Delegate.BindDynamic(this, &UInteRealPlanViewModel::OnBaseTopologyReceived);
    GetNetworkSubsystem()->FetchBaseTopology(PlanItem.id, FUeTopologyExportRequest(), Delegate);
}

void UInteRealPlanViewModel::RefreshLatestDelta()
{
    SetIsBusy(true);
    FOnDeltaReceived Delegate;
    Delegate.BindDynamic(this, &UInteRealPlanViewModel::OnDeltaReceived);
    GetNetworkSubsystem()->FetchLatestDelta(CurrentPlan.id, Delegate);
}

void UInteRealPlanViewModel::SaveCurrentState(const FString& DeltaJson)
{
    SetIsBusy(true);
    FOnDeltaSaved Delegate;
    Delegate.BindDynamic(this, &UInteRealPlanViewModel::OnDeltaSaved);
    GetNetworkSubsystem()->SaveDelta(CurrentPlan.id, DeltaJson, Delegate);
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

void UInteRealPlanViewModel::OnBaseTopologyReceived(bool bSuccess, const FString& TopologyJson)
{
    if (bSuccess) {
        if (auto* Pipeline = GetWorld()->GetSubsystem<UHarnessPipelineManager>()) {
            Pipeline->AssembleBase(TopologyJson);
        }
        RefreshLatestDelta(); // 2단계: 가구 데이터 로드
    } else SetIsBusy(false);
}

void UInteRealPlanViewModel::OnDeltaReceived(bool bSuccess, const FString& DeltaJson)
{
    SetIsBusy(false);
    if (bSuccess) {
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
