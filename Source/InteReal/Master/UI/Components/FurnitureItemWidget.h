#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteReal/EditMode/Furniture/FFurnitureDataRow.h"
#include "FurnitureItemWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS(BlueprintType, Blueprintable)
class INTEREAL_API UFurnitureItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "InteReal|FurnitureItem")
	void SetupFurnitureItem(const FName& InRowName, const FFurnitureDataRow& InFurnitureData);

	UFUNCTION(BlueprintPure, Category = "InteReal|FurnitureItem")
	FName GetFurnitureRowName() const { return FurnitureRowName; }

	UFUNCTION(BlueprintPure, Category = "InteReal|FurnitureItem")
	const FFurnitureDataRow& GetFurnitureData() const { return FurnitureData; }

	UFUNCTION(BlueprintPure, Category = "InteReal|FurnitureItem")
	EFurnitureAssetCategory GetFurnitureCategory() const { return FurnitureData.Category; }

protected:
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void HandleDisplayButtonClicked();

	FText MakeSizeText(const TCHAR* Prefix, float Value) const;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Display = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TextBlock_Name = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TextBlock_Width = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TextBlock_Depth = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TextBlock_Height = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "InteReal|FurnitureItem", meta = (AllowPrivateAccess = "true"))
	FName FurnitureRowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InteReal|FurnitureItem", meta = (ExposeOnSpawn = "true", AllowPrivateAccess = "true"))
	FFurnitureDataRow FurnitureData;
};