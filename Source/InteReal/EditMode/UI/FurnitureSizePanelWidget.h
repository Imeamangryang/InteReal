#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FurnitureSizePanelWidget.generated.h"

class UBaseInput;
class UBaseButton;
class AFurniture;
class UImage;
class UTextBlock;

UCLASS()
class INTEREAL_API UFurnitureSizePanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	
	virtual void NativeDestruct() override;
	
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
	TObjectPtr<UBaseInput> Input_Value_X;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBaseInput> Input_Value_Y;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBaseInput> Input_Value_Z;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBaseButton> Button_ApplySize;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBaseButton> Button_CancelSize;

private:
	UFUNCTION()
	void HandleApplyClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UFUNCTION()
	void HandleSizeInputChanged(const FText& Text);

	void RefreshFurnitureSummary(AFurniture* Furniture);
	void ClearFurnitureSummary();
	void RefreshInputFieldsFromFurniture(AFurniture* Furniture);
	void ApplyPreviewSizeFromInputs();
	void CommitPreviewSize();
	void RevertPreviewSize();
	float ParseSizeInputMm(UBaseInput* Input) const;

	TWeakObjectPtr<AFurniture> TargetFurniture;

	FVector OriginalSizeCm = FVector::ZeroVector;
	bool bHasOriginalSize = false;
	bool bSizeChangeCommitted = false;
	bool bRefreshingInputs = false;
};