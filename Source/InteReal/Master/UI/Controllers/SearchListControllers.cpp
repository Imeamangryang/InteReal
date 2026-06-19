#include "SearchListControllers.h"
#include "InteReal/Master/UI/Components/SearchListWidget.h"
#include "InteReal/Network/ViewModel/InteRealPlanViewModel.h"

// --- Base Controller ---
void UInteRealSearchListControllerBase::InitializeController(UInteRealPlanViewModel* InViewModel, USearchListWidget* InWidget)
{
    ViewModel = InViewModel;
    Widget = InWidget;

    if (Widget)
    {
        Widget->OnSearchStringChanged.AddUniqueDynamic(this, &UInteRealSearchListControllerBase::OnSearchStringChanged);
        Widget->OnItemClicked.AddUniqueDynamic(this, &UInteRealSearchListControllerBase::OnItemClicked);
        Widget->OnListFocused.AddUniqueDynamic(this, &UInteRealSearchListControllerBase::OnListFocused);
    }
}

void UInteRealSearchListControllerBase::DeinitializeController()
{
    if (Widget)
    {
        Widget->OnSearchStringChanged.RemoveDynamic(this, &UInteRealSearchListControllerBase::OnSearchStringChanged);
        Widget->OnItemClicked.RemoveDynamic(this, &UInteRealSearchListControllerBase::OnItemClicked);
        Widget->OnListFocused.RemoveDynamic(this, &UInteRealSearchListControllerBase::OnListFocused);
    }
    Widget = nullptr;
    ViewModel = nullptr;
}

void UInteRealSearchListControllerBase::OnSearchStringChanged(const FString& SearchString)
{
    CurrentSearchQuery = SearchString;
    Refresh();
}

void UInteRealSearchListControllerBase::OnListFocused()
{
    if (Widget)
    {
        Widget->SetResultsVisible(true);
    }
    Refresh();
}

bool UInteRealSearchListControllerBase::MatchesFilter(const FString& Title) const
{
    return CurrentSearchQuery.IsEmpty() || Title.Contains(CurrentSearchQuery, ESearchCase::IgnoreCase);
}

// --- Project Controller ---
void UInteRealProjectListController::InitializeController(UInteRealPlanViewModel* InViewModel, USearchListWidget* InWidget)
{
    Super::InitializeController(InViewModel, InWidget);
    if (ViewModel)
    {
        ViewModel->OnProjectListUpdated.AddUniqueDynamic(this, &UInteRealProjectListController::HandleProjectListUpdated);
    }
    if (Widget)
    {
        Widget->SetHeaderText(FText::FromString(TEXT("Projects")));
    }
}

void UInteRealProjectListController::DeinitializeController()
{
    if (ViewModel)
    {
        ViewModel->OnProjectListUpdated.RemoveDynamic(this, &UInteRealProjectListController::HandleProjectListUpdated);
    }
    Super::DeinitializeController();
}

void UInteRealProjectListController::Refresh()
{
    if (!bHasLoaded && ViewModel)
    {
        ViewModel->FetchProjectList();
    }
    else
    {
        UpdateWidget();
    }
}

void UInteRealProjectListController::HandleProjectListUpdated(bool bSuccess, const FUnrealProjectListResponse& Response)
{
    if (bSuccess)
    {
        CachedItems = Response.items;
        bHasLoaded = true;
        UpdateWidget();
    }
}

void UInteRealProjectListController::OnItemClicked(int32 ItemId)
{
    for (const FUnrealProjectItem& Item : CachedItems)
    {
        if (Item.id == ItemId)
        {
            SelectedId = ItemId;
            CurrentSearchQuery = Item.name;
            OnProjectSelected.Broadcast(Item);
            if (Widget)
            {
                Widget->SetSearchText(Item.name);
            }
            break;
        }
    }
    UpdateWidget();
}

void UInteRealProjectListController::UpdateWidget()
{
    if (!Widget) return;
    Widget->ClearItems();
    for (const FUnrealProjectItem& Item : CachedItems)
    {
        if (MatchesFilter(Item.name))
        {
            Widget->AddItem(Item.id, Item.name, Item.id == SelectedId);
        }
    }
}

// --- Plan Controller ---
void UInteRealPlanListController::InitializeController(UInteRealPlanViewModel* InViewModel, USearchListWidget* InWidget)
{
    Super::InitializeController(InViewModel, InWidget);
    if (ViewModel)
    {
        ViewModel->OnPlanListUpdated.AddUniqueDynamic(this, &UInteRealPlanListController::HandlePlanListUpdated);
    }
    if (Widget)
    {
        Widget->SetHeaderText(FText::FromString(TEXT("Plans")));
    }
}

void UInteRealPlanListController::DeinitializeController()
{
    if (ViewModel)
    {
        ViewModel->OnPlanListUpdated.RemoveDynamic(this, &UInteRealPlanListController::HandlePlanListUpdated);
    }
    Super::DeinitializeController();
}

void UInteRealPlanListController::SetProjectIdFilter(int32 InProjectId)
{
    if (ProjectIdFilter != InProjectId)
    {
        ProjectIdFilter = InProjectId;
        bHasLoaded = false;
        CachedItems.Reset();
        SelectedId = 0;
        CurrentSearchQuery.Empty();
        if (Widget)
        {
            Widget->SetSearchText(TEXT(""));
        }
        Refresh();
    }
}

void UInteRealPlanListController::Refresh()
{
    if (!bHasLoaded && ViewModel && ProjectIdFilter != 0)
    {
        FUnrealPlanSearchParams Params;
        Params.limit = 50;
        Params.executable_only = true;
        Params.project_id = FString::FromInt(ProjectIdFilter);
        ViewModel->FetchExecutablePlanList(Params);
    }
    else
    {
        UpdateWidget();
    }
}

void UInteRealPlanListController::HandlePlanListUpdated(bool bSuccess, const FUnrealPlanListResponse& Response)
{
    if (bSuccess)
    {
        CachedItems = Response.items;
        bHasLoaded = true;
        UpdateWidget();
    }
}

void UInteRealPlanListController::OnItemClicked(int32 ItemId)
{
    for (const FUnrealPlanItem& Item : CachedItems)
    {
        if (Item.id == ItemId)
        {
            SelectedId = ItemId;
            CurrentSearchQuery = Item.GetDisplayTitle();
            if (Widget)
            {
                Widget->SetSearchText(Item.GetDisplayTitle());
            }
            if (ViewModel)
            {
                ViewModel->LoadPlanTopology(Item);
            }
            OnPlanSelected.Broadcast(Item);
            break;
        }
    }
    UpdateWidget();
}

void UInteRealPlanListController::UpdateWidget()
{
    if (!Widget) return;
    Widget->ClearItems();
    for (const FUnrealPlanItem& Item : CachedItems)
    {
        if (MatchesFilter(Item.GetDisplayTitle()))
        {
            Widget->AddItem(Item.id, Item.GetDisplayTitle(), Item.id == SelectedId);
        }
    }
}

// --- Version Controller ---
void UInteRealVersionListController::InitializeController(UInteRealPlanViewModel* InViewModel, USearchListWidget* InWidget)
{
    Super::InitializeController(InViewModel, InWidget);
    if (ViewModel)
    {
        ViewModel->OnDeltaVersionListUpdated.AddUniqueDynamic(this, &UInteRealVersionListController::HandleVersionListUpdated);
    }
    if (Widget)
    {
        Widget->SetHeaderText(FText::FromString(TEXT("Versions")));
    }
}

void UInteRealVersionListController::DeinitializeController()
{
    if (ViewModel)
    {
        ViewModel->OnDeltaVersionListUpdated.RemoveDynamic(this, &UInteRealVersionListController::HandleVersionListUpdated);
    }
    Super::DeinitializeController();
}

void UInteRealVersionListController::SetPlanIdFilter(int32 InPlanId)
{
    if (PlanIdFilter != InPlanId)
    {
        PlanIdFilter = InPlanId;
        bHasLoaded = false;
        CachedItems.Reset();
        SelectedVersion = INDEX_NONE;
        CurrentSearchQuery.Empty();
        bAutoSelectLatestOnNextUpdate = PlanIdFilter != 0;
        if (Widget)
        {
            Widget->SetSearchText(TEXT(""));
        }
        Refresh();
    }
}

void UInteRealVersionListController::Refresh()
{
    if (!bHasLoaded && ViewModel && PlanIdFilter != 0)
    {
        ViewModel->FetchDeltaVersionList(PlanIdFilter);
    }
    else
    {
        UpdateWidget();
    }
}

void UInteRealVersionListController::RefreshAndSelectLatest()
{
    if (PlanIdFilter == 0)
    {
        return;
    }

    bHasLoaded = false;
    bAutoSelectLatestOnNextUpdate = true;
    Refresh();
}

void UInteRealVersionListController::HandleVersionListUpdated(bool bSuccess, const FUnrealDeltaVersionListResponse& Response)
{
    if (bSuccess)
    {
        CachedItems = Response.items;
        bHasLoaded = true;

        if (bAutoSelectLatestOnNextUpdate)
        {
            bAutoSelectLatestOnNextUpdate = false;
            if (TryAutoSelectLatestVersion())
            {
                return;
            }
        }

        UpdateWidget();
    }
}

void UInteRealVersionListController::OnItemClicked(int32 ItemId)
{
    for (const FUnrealDeltaVersionItem& Item : CachedItems)
    {
        if (Item.version == ItemId)
        {
            SelectVersionItem(Item);
            break;
        }
    }
    UpdateWidget();
}

void UInteRealVersionListController::SelectVersionItem(const FUnrealDeltaVersionItem& Item)
{
    SelectedVersion = Item.version;
    CurrentSearchQuery = Item.GetDisplayTitle();
    OnVersionSelected.Broadcast(Item);
    if (Widget)
    {
        Widget->SetSearchText(Item.GetDisplayTitle());
    }
    if (ViewModel)
    {
        ViewModel->LoadDeltaVersion(Item);
    }
}

bool UInteRealVersionListController::TryAutoSelectLatestVersion()
{
    if (CachedItems.IsEmpty())
    {
        return false;
    }

    const FUnrealDeltaVersionItem* LatestItem = nullptr;

    for (const FUnrealDeltaVersionItem& Item : CachedItems)
    {
        if (Item.is_latest)
        {
            LatestItem = &Item;
            break;
        }
    }

    if (!LatestItem)
    {
        for (const FUnrealDeltaVersionItem& Item : CachedItems)
        {
            if (Item.version == 0)
            {
                LatestItem = &Item;
                break;
            }
        }
    }

    if (!LatestItem)
    {
        LatestItem = &CachedItems[0];
        for (const FUnrealDeltaVersionItem& Item : CachedItems)
        {
            if (Item.version > LatestItem->version)
            {
                LatestItem = &Item;
            }
        }
    }

    SelectVersionItem(*LatestItem);
    UpdateWidget();
    return true;
}

void UInteRealVersionListController::UpdateWidget()
{
    if (!Widget) return;
    Widget->ClearItems();
    for (const FUnrealDeltaVersionItem& Item : CachedItems)
    {
        if (MatchesFilter(Item.GetDisplayTitle()))
        {
            Widget->AddItem(Item.version, Item.GetDisplayTitle(), Item.version == SelectedVersion);
        }
    }
}
