#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FurnitureSizePanelWidget.generated.h"

class AFurniture;
class UEditableText;
class UButton;
class UImage;
class UTextBlock;

UCLASS()
class INTEREAL_API UFurnitureSizePanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	
	UFUNCTION(BlueprintCallable, Category = "FurnitureSizePanel")
	void RefreshForFurniture(AFurniture* Furniture);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_SelectedFurniture;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_SelectedFurnitureName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SelectedFurnitureInfo;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableText> ET_Value_X;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableText> ET_Value_Y;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableText> ET_Value_Z;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_ApplySize;

private:
	UFUNCTION()
	void HandleApplyClicked();

	void RefreshFurnitureSummary(AFurniture* Furniture);
	void ClearFurnitureSummary();

	TWeakObjectPtr<AFurniture> TargetFurniture;
};