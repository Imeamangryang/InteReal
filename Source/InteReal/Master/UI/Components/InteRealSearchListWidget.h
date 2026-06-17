#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteReal/Network/InteRealDataTypes.h"
#include "InteRealSearchListWidget.generated.h"

class UBorder;
class UButton;
class UEditableText;
class UScrollBox;
class UTextBlock;
class UInteRealPlanViewModel;
class UInteRealThemeData;

UENUM(BlueprintType)
enum class EInteRealSearchListMode : uint8
{
    Plan,
    DeltaVersion
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteRealSearchPlanSelected, const FUnrealPlanItem&, PlanItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteRealSearchDeltaVersionSelected, const FUnrealDeltaVersionItem&, DeltaVersionItem);

class UInteRealSearchListWidget;

UCLASS()
class INTEREAL_API UInteRealSearchListItemClickHandler : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY()
    TObjectPtr<UInteRealSearchListWidget> Owner;

    int32 ItemIndex = INDEX_NONE;

    UFUNCTION()
    void HandleClicked();
};

UCLASS(BlueprintType, Blueprintable)
class INTEREAL_API UInteRealSearchListWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UInteRealSearchListWidget(const FObjectInitializer& ObjectInitializer);

    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "InteReal|Search")
    TObjectPtr<UEditableText> EditText_Search;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "InteReal|Search")
    TObjectPtr<UBorder> Border_Search;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "InteReal|Search")
    TObjectPtr<UScrollBox> ScrollBox_Items;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "InteReal|Search")
    TObjectPtr<UTextBlock> Text_Header;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "InteReal|Search")
    TObjectPtr<UBorder> Panel_Results;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteReal|Search")
    EInteRealSearchListMode Mode = EInteRealSearchListMode::Plan;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteReal|Search")
    int32 DeltaPlanId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteReal|Search")
    int32 SearchLimit = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteReal|Search")
    bool bAutoLoadOnSelection = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteReal|Search")
    FText PlanHeaderText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteReal|Search")
    FText DeltaHeaderText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteReal|Search")
    FText SearchHintText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteReal|Search|Theme")
    TObjectPtr<UInteRealThemeData> ThemeData;

    UPROPERTY(BlueprintAssignable, Category = "InteReal|Search")
    FOnInteRealSearchPlanSelected OnPlanSelected;

    UPROPERTY(BlueprintAssignable, Category = "InteReal|Search")
    FOnInteRealSearchDeltaVersionSelected OnDeltaVersionSelected;

    UFUNCTION(BlueprintCallable, Category = "InteReal|Search")
    void SetViewModel(UInteRealPlanViewModel* InViewModel);

    UFUNCTION(BlueprintCallable, Category = "InteReal|Search")
    void SetupForPlanSearch(UInteRealPlanViewModel* InViewModel);

    UFUNCTION(BlueprintCallable, Category = "InteReal|Search")
    void SetupForDeltaVersionSearch(int32 InPlanId, UInteRealPlanViewModel* InViewModel);

    UFUNCTION(BlueprintCallable, Category = "InteReal|Search")
    void SetMode(EInteRealSearchListMode InMode);

    UFUNCTION(BlueprintCallable, Category = "InteReal|Search")
    void SetDeltaPlanId(int32 InPlanId);

    UFUNCTION(BlueprintCallable, Category = "InteReal|Search")
    void RefreshList();

    UFUNCTION(BlueprintCallable, Category = "InteReal|Search")
    void OpenRecentList();

    UFUNCTION(BlueprintCallable, Category = "InteReal|Search")
    void ClearSelection();

    void HandleItemClicked(int32 ItemIndex);

private:
    UPROPERTY()
    TObjectPtr<UInteRealPlanViewModel> PlanViewModel;

    UPROPERTY()
    TArray<TObjectPtr<UInteRealSearchListItemClickHandler>> ItemClickHandlers;

    TArray<FUnrealPlanItem> CachedPlans;
    TArray<FUnrealDeltaVersionItem> CachedDeltaVersions;

    int32 SelectedPlanId = 0;
    int32 SelectedDeltaVersion = INDEX_NONE;
    bool bHasLoadedPlans = false;
    bool bHasLoadedDeltaVersions = false;
    bool bWasInputFocused = false;

    UFUNCTION()
    void HandleSearchTextChanged(const FText& Text);

    UFUNCTION()
    void HandlePlanListUpdated(bool bSuccess, const FUnrealPlanListResponse& Response);

    UFUNCTION()
    void HandleDeltaVersionListUpdated(bool bSuccess, const FUnrealDeltaVersionListResponse& Response);

    void EnsureDefaultWidgetTree();
    UInteRealThemeData* ResolveThemeData();
    void ApplyThemeStyle();
    void BindViewModelEvents();
    void UnbindViewModelEvents();
    UInteRealPlanViewModel* ResolveViewModel();
    void HandleSearchFocused();
    void RebuildRows();
    void AddPlanRow(const FUnrealPlanItem& PlanItem, int32 ItemIndex);
    void AddDeltaVersionRow(const FUnrealDeltaVersionItem& VersionItem, int32 ItemIndex);
    UButton* BuildRowButton(const FText& Label, bool bSelected, int32 ItemIndex);
    bool MatchesFilter(const FString& Title) const;
    FString GetSearchString() const;
    void SetResultsVisible(bool bVisible);
    void UpdateHeaderText();
};
