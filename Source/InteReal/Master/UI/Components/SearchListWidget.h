#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SearchListWidget.generated.h"

class UBorder;
class UButton;
class UEditableText;
class UScrollBox;
class UTextBlock;
class UInteRealThemeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSearchListStringChanged, const FString&, SearchString);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSearchListItemClicked, int32, ItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSearchListFocused);

class USearchListWidget;

/** 
 * Helper object to handle button clicks for individual list rows.
 */
UCLASS()
class INTEREAL_API UInteRealSearchListItemClickHandler : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY()
    TObjectPtr<USearchListWidget> Owner;

    int32 ItemId = INDEX_NONE;

    UFUNCTION()
    void HandleClicked();
};

/** 
 * A pure, generic UI component for displaying a searchable list of items.
 * Use AddItem() and ClearItems() to manage content, and subscribe to delegates for interactions.
 */
UCLASS(BlueprintType, Blueprintable)
class INTEREAL_API USearchListWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    USearchListWidget(const FObjectInitializer& ObjectInitializer);

    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;

    // --- Bound Widgets ---
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "InteReal|Search")
    TObjectPtr<UEditableText> EditText_Search;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "InteReal|Search")
    TObjectPtr<UBorder> Border_Search;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "InteReal|Search")
    TObjectPtr<UScrollBox> ScrollBox_Items;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "InteReal|Search")
    TObjectPtr<UTextBlock> Text_Header;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "InteReal|Search")
    TObjectPtr<UBorder> Panel_Results;

    // --- Configuration ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteReal|Search")
    FText SearchHintText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteReal|Search|Theme")
    TObjectPtr<UInteRealThemeData> ThemeData;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteReal|Search|Style", meta = (ClampMin = "1"))
    int32 ItemFontSize = 12;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteReal|Search|Style", meta = (ClampMin = "1"))
    int32 SelectedMarkFontSize = 12;

    // --- Delegates ---
    UPROPERTY(BlueprintAssignable, Category = "InteReal|Search")
    FOnSearchListStringChanged OnSearchStringChanged;

    UPROPERTY(BlueprintAssignable, Category = "InteReal|Search")
    FOnSearchListItemClicked OnItemClicked;

    UPROPERTY(BlueprintAssignable, Category = "InteReal|Search")
    FOnSearchListFocused OnListFocused;

    // --- Public API ---
    UFUNCTION(BlueprintCallable, Category = "InteReal|Search")
    void SetHeaderText(const FText& InHeader);

    UFUNCTION(BlueprintCallable, Category = "InteReal|Search")
    void SetSearchText(const FString& InText);

    UFUNCTION(BlueprintCallable, Category = "InteReal|Search")
    void ClearItems();

    UFUNCTION(BlueprintCallable, Category = "InteReal|Search")
    void AddItem(int32 ItemId, const FString& Label, bool bIsSelected);

    UFUNCTION(BlueprintCallable, Category = "InteReal|Search")
    void SetResultsVisible(bool bVisible);

    /** Called internally by row buttons to notify selection */
    void NotifyItemClicked(int32 ItemId);

private:
    struct FSearchListEntry
    {
        int32 ItemId = INDEX_NONE;
        FString Label;
        bool bIsSelected = false;
    };

    UPROPERTY()
    TArray<TObjectPtr<UInteRealSearchListItemClickHandler>> ItemClickHandlers;

    TArray<FSearchListEntry> Items;

    bool bWasInputFocused = false;
    bool bIsResultsVisible = false;
    bool bSuppressSearchTextChanged = false;

    UFUNCTION()
    void HandleSearchTextChanged(const FText& Text);

    UInteRealThemeData* ResolveThemeData();
    void ApplyThemeStyle();
    void HandleListFocused();
    void ClearRows();
    void RebuildFilteredItems();
    bool MatchesFilter(const FString& Label) const;
    UButton* BuildRowButton(const FText& Label, bool bSelected, int32 ItemId);
};
