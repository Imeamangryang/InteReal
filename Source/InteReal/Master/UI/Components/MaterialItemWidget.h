#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Materials/FMaterialDataRow.h"
#include "MaterialItemWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMaterialItemClickedSignature, FName, RowName, FMaterialDataRow, MaterialData);

UCLASS(BlueprintType, Blueprintable)
class INTEREAL_API UMaterialItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "InteReal|MaterialItem")
	void SetupMaterialItem(const FName& InRowName, const FMaterialDataRow& InMaterialData);

	UFUNCTION(BlueprintPure, Category = "InteReal|MaterialItem")
	FName GetMaterialRowName() const { return MaterialRowName; }

	UFUNCTION(BlueprintPure, Category = "InteReal|MaterialItem")
	const FMaterialDataRow& GetMaterialData() const { return MaterialData; }

	UPROPERTY(BlueprintAssignable, Category = "InteReal|MaterialItem")
	FMaterialItemClickedSignature OnMaterialItemClicked;

protected:
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void HandleDisplayButtonClicked();

private:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Display = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TextBlock_Name = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "InteReal|MaterialItem", meta = (AllowPrivateAccess = "true"))
	FName MaterialRowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InteReal|MaterialItem", meta = (ExposeOnSpawn = "true", AllowPrivateAccess = "true"))
	FMaterialDataRow MaterialData;
};