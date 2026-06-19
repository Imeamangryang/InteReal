#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "InteReal/Network/InteRealDataTypes.h"
#include "SearchListControllers.generated.h"

class UInteRealPlanViewModel;
class USearchListWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProjectControllerItemSelected, const FUnrealProjectItem&, ProjectItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlanControllerItemSelected, const FUnrealPlanItem&, PlanItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVersionControllerItemSelected, const FUnrealDeltaVersionItem&, VersionItem);

/** 
 * Base controller for handling search list logic, decoupling it from the UI component. 
 */
UCLASS(Abstract)
class INTEREAL_API UInteRealSearchListControllerBase : public UObject
{
    GENERATED_BODY()
public:
    virtual void InitializeController(UInteRealPlanViewModel* InViewModel, USearchListWidget* InWidget);
    virtual void DeinitializeController();
    virtual void Refresh() PURE_VIRTUAL(UInteRealSearchListControllerBase::Refresh, );

protected:
    UPROPERTY()
    TObjectPtr<UInteRealPlanViewModel> ViewModel;

    UPROPERTY()
    TObjectPtr<USearchListWidget> Widget;

    FString CurrentSearchQuery;

    UFUNCTION()
    virtual void OnSearchStringChanged(const FString& SearchString);

    UFUNCTION()
    virtual void OnItemClicked(int32 ItemId) PURE_VIRTUAL(UInteRealSearchListControllerBase::OnItemClicked, );

    UFUNCTION()
    virtual void OnListFocused();

    bool MatchesFilter(const FString& Title) const;
};

/** Controller for Project Search List */
UCLASS()
class INTEREAL_API UInteRealProjectListController : public UInteRealSearchListControllerBase
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable)
    FOnProjectControllerItemSelected OnProjectSelected;

    virtual void InitializeController(UInteRealPlanViewModel* InViewModel, USearchListWidget* InWidget) override;
    virtual void DeinitializeController() override;
    virtual void Refresh() override;

protected:
    virtual void OnItemClicked(int32 ItemId) override;

    UFUNCTION()
    void HandleProjectListUpdated(bool bSuccess, const FUnrealProjectListResponse& Response);

private:
    int32 SelectedId = 0;
    TArray<FUnrealProjectItem> CachedItems;
    bool bHasLoaded = false;
    void UpdateWidget();
};

/** Controller for Plan Search List */
UCLASS()
class INTEREAL_API UInteRealPlanListController : public UInteRealSearchListControllerBase
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable)
    FOnPlanControllerItemSelected OnPlanSelected;

    virtual void InitializeController(UInteRealPlanViewModel* InViewModel, USearchListWidget* InWidget) override;
    virtual void DeinitializeController() override;
    virtual void Refresh() override;

    void SetProjectIdFilter(int32 InProjectId);

protected:
    virtual void OnItemClicked(int32 ItemId) override;

    UFUNCTION()
    void HandlePlanListUpdated(bool bSuccess, const FUnrealPlanListResponse& Response);

private:
    int32 SelectedId = 0;
    int32 ProjectIdFilter = 0;
    TArray<FUnrealPlanItem> CachedItems;
    bool bHasLoaded = false;
    void UpdateWidget();
};

/** Controller for Version Search List */
UCLASS()
class INTEREAL_API UInteRealVersionListController : public UInteRealSearchListControllerBase
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable)
    FOnVersionControllerItemSelected OnVersionSelected;

    virtual void InitializeController(UInteRealPlanViewModel* InViewModel, USearchListWidget* InWidget) override;
    virtual void DeinitializeController() override;
    virtual void Refresh() override;

    void SetPlanIdFilter(int32 InPlanId);
    void RefreshAndSelectLatest();

protected:
    virtual void OnItemClicked(int32 ItemId) override;

    UFUNCTION()
    void HandleVersionListUpdated(bool bSuccess, const FUnrealDeltaVersionListResponse& Response);

private:
    int32 SelectedVersion = INDEX_NONE;
    int32 PlanIdFilter = 0;
    TArray<FUnrealDeltaVersionItem> CachedItems;
    bool bHasLoaded = false;
    bool bAutoSelectLatestOnNextUpdate = false;
    void SelectVersionItem(const FUnrealDeltaVersionItem& Item);
    bool TryAutoSelectLatestVersion();
    void UpdateWidget();
};
