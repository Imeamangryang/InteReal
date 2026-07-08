#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteReal/EditMode/Openings/FOpeningAssetDataRow.h"
#include "OpeningItemWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS(BlueprintType, Blueprintable)
class INTEREAL_API UOpeningItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "InteReal|OpeningItem")
	void SetupOpeningItem(const FName& InRowName, const FOpeningAssetDataRow& InOpeningData);

	UFUNCTION(BlueprintPure, Category = "InteReal|OpeningItem")
	FName GetOpeningRowName() const { return OpeningRowName; }

	UFUNCTION(BlueprintPure, Category = "InteReal|OpeningItem")
	const FOpeningAssetDataRow& GetOpeningData() const { return OpeningData; }

	UFUNCTION(BlueprintPure, Category = "InteReal|OpeningItem")
	EOpeningAssetKind GetOpeningKind() const { return OpeningData.OpeningKind; }

protected:
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void HandleDisplayButtonClicked();

	FText MakeOpeningKindText(EOpeningAssetKind InKind) const;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Display = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TextBlock_Name = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TextBlock_Kind = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TextBlock_YawOffset = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "InteReal|OpeningItem", meta = (AllowPrivateAccess = "true"))
	FName OpeningRowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InteReal|OpeningItem", meta = (ExposeOnSpawn = "true", AllowPrivateAccess = "true"))
	FOpeningAssetDataRow OpeningData;
};