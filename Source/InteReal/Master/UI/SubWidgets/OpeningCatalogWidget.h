#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteReal/EditMode/Furniture/FFurnitureDataRow.h"
#include "InteReal/EditMode/Openings/FOpeningAssetDataRow.h"
#include "InteReal/Network/InteRealDataTypes.h"
#include "Styling/SlateTypes.h"
#include "OpeningCatalogWidget.generated.h"

class UButton;
class UDataTable;
class UEditableText;
class UWrapBox;
class UOpeningItemWidget;

UENUM(BlueprintType)
enum class EOpeningCatalogCategory : uint8
{
	All UMETA(DisplayName = "전체"),
	Door UMETA(DisplayName = "Door"),
	Window UMETA(DisplayName = "Window")
};

UCLASS(BlueprintType, Blueprintable)
class INTEREAL_API UOpeningCatalogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "InteReal|OpeningCatalog")
	void SetOpeningDataTable(UDataTable* InOpeningDataTable);

	UFUNCTION(BlueprintCallable, Category = "InteReal|OpeningCatalog")
	void RebuildOpeningList();

	UFUNCTION(BlueprintCallable, Category = "InteReal|OpeningCatalog")
	void ApplyOpeningFilters();

	UFUNCTION(BlueprintCallable, Category = "InteReal|OpeningCatalog")
	void SetCategoryFilter(EOpeningCatalogCategory InCategory);

protected:
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void HandleSearchTextChanged(const FText& InText);

	UFUNCTION()
	void HandleAllCategoryClicked();
	
	UFUNCTION()
	void HandleDoorCategoryClicked();

	UFUNCTION()
	void HandleWindowCategoryClicked();

	bool DoesItemMatchFilter(const UOpeningItemWidget* ItemWidget) const;
	bool TryResolveOpeningCategory(const FOpeningAssetDataRow& Row, EOpeningCatalogCategory& OutCategory) const;
	bool IsDoorRow(const FOpeningAssetDataRow& Row) const;
	bool IsWindowRow(const FOpeningAssetDataRow& Row) const;
	bool AddOpeningItem(const FName& RowName, const FOpeningAssetDataRow& Row);

	void RefreshCategoryButtonStyles();
	void ApplyCategoryButtonStyle(UButton* Button, bool bSelected) const;
	FButtonStyle MakeCategoryButtonStyle(bool bSelected) const;
	void ApplyCategoryButtonTextColor(UButton* Button, bool bSelected) const;

	UFUNCTION()
	void HandleApiAssetsReceived(bool bSuccess, const FUnrealAssetListResponse& Response);

	bool ShouldUseApiAssets() const;
	void RequestApiAssets();
	void RebuildOpeningListFromDataTable();
	void RebuildOpeningListFromApiAssets(const FUnrealAssetListResponse& Response);
	bool IsOpeningAsset(const FInteRealAssetData& Asset) const;
	FOpeningAssetDataRow ConvertAssetToOpeningRow(const FInteRealAssetData& Asset) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InteReal|OpeningCatalog", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDataTable> OpeningDataTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InteReal|OpeningCatalog", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UOpeningItemWidget> OpeningItemWidgetClass;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableText> EditText_Search = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWrapBox> WrapBox_Opening = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_All = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Door = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Window = nullptr;

	FString CurrentSearchText;
	EOpeningCatalogCategory CurrentCategoryFilter = EOpeningCatalogCategory::All;

	TMap<TObjectKey<UOpeningItemWidget>, EOpeningCatalogCategory> CategoryByItem;
	TMap<TObjectKey<UOpeningItemWidget>, FString> SearchTextByItem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteReal|OpeningCatalog|API", meta = (AllowPrivateAccess = "true"))
	int32 ApiAssetFetchLimit = 500;

	UPROPERTY(Transient)
	FUnrealAssetListResponse CachedApiAssetList;

	bool bIsFetchingApiAssets = false;
	bool bHasFetchedApiAssets = false;
};
