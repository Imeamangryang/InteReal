#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteReal/EditMode/Furniture/FFurnitureDataRow.h"
#include "FurnitureCatalogWidget.generated.h"

class UButton;
class UDataTable;
class UEditableText;
class UWrapBox;
class UFurnitureItemWidget;

UCLASS(BlueprintType, Blueprintable)
class INTEREAL_API UFurnitureCatalogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "InteReal|FurnitureCatalog")
	void SetFurnitureDataTable(UDataTable* InFurnitureDataTable);

	UFUNCTION(BlueprintCallable, Category = "InteReal|FurnitureCatalog")
	void RebuildFurnitureList();

	UFUNCTION(BlueprintCallable, Category = "InteReal|FurnitureCatalog")
	void ApplyFurnitureFilters();

	UFUNCTION(BlueprintCallable, Category = "InteReal|FurnitureCatalog")
	void SetCategoryFilter(EFurnitureAssetCategory InCategory);

	UFUNCTION(BlueprintCallable, Category = "InteReal|FurnitureCatalog")
	void ClearCategoryFilter();

protected:
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void HandleSearchTextChanged(const FText& InText);

	UFUNCTION()
	void HandleAllCategoryClicked();

	UFUNCTION()
	void HandleBedCategoryClicked();

	UFUNCTION()
	void HandleSeatingCategoryClicked();

	UFUNCTION()
	void HandleTableDeskCategoryClicked();

	UFUNCTION()
	void HandleStorageCategoryClicked();

	UFUNCTION()
	void HandleLightingCategoryClicked();

	UFUNCTION()
	void HandleElectronicsCategoryClicked();

	UFUNCTION()
	void HandleKitchenCategoryClicked();

	UFUNCTION()
	void HandleBathroomCategoryClicked();

	UFUNCTION()
	void HandleDecorCategoryClicked();

	UFUNCTION()
	void HandleMirrorCategoryClicked();

	UFUNCTION()
	void HandlePlantCategoryClicked();

	UFUNCTION()
	void HandleRugCategoryClicked();

	UFUNCTION()
	void HandleShelfCategoryClicked();

	bool DoesItemMatchFilter(const UFurnitureItemWidget* ItemWidget) const;
	
	void RefreshCategoryButtonStyles();
	void ApplyCategoryButtonStyle(UButton* Button, bool bSelected) const;
	FButtonStyle MakeCategoryButtonStyle(bool bSelected) const;
	void ApplyCategoryButtonTextColor(UButton* Button, bool bSelected) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InteReal|FurnitureCatalog", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDataTable> FurnitureDataTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InteReal|FurnitureCatalog", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UFurnitureItemWidget> FurnitureItemWidgetClass;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableText> EditText_Search = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWrapBox> WrapBox_Furniture = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_All = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Bed = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Seating = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_TableDesk = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Storage = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Lighting = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Electronics = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Kitchen = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Bathroom = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Decor = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Mirror = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Plant = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Rug = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Shelf = nullptr;

	FString CurrentSearchText;
	EFurnitureAssetCategory CurrentCategoryFilter = EFurnitureAssetCategory::None;
	bool bUseCategoryFilter = false;
};