#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MaterialCatalogWidget.generated.h"

class UDataTable;
class UEditableText;
class UWrapBox;
class UMaterialItemWidget;

UCLASS(BlueprintType, Blueprintable)
class INTEREAL_API UMaterialCatalogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "InteReal|MaterialCatalog")
	void SetMaterialDataTable(UDataTable* InMaterialDataTable);

	UFUNCTION(BlueprintCallable, Category = "InteReal|MaterialCatalog")
	void RebuildMaterialList();

	UFUNCTION(BlueprintCallable, Category = "InteReal|MaterialCatalog")
	void ApplyMaterialFilters();

protected:
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void HandleSearchTextChanged(const FText& InText);

	bool DoesItemMatchFilter(const UMaterialItemWidget* ItemWidget) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InteReal|MaterialCatalog", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDataTable> MaterialDataTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InteReal|MaterialCatalog", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UMaterialItemWidget> MaterialItemWidgetClass;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableText> EditText_Search = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWrapBox> WrapBox_Material = nullptr;

	FString CurrentSearchText;
};