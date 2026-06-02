#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ViewModeData.h"
#include "ViewModeWidget.generated.h"

class UButton;

/**
 * UI Widget for switching view modes.
 */
UCLASS()
class INTEREAL_API UViewModeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ViewMode")
	void ChangeViewMode(EHarnessViewMode NewMode);

protected:
	virtual void NativeConstruct() override;

	// Buttons (to be bound in Widget Blueprint)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_TopDown;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Isometric;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_FirstPerson;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_RotateCanvas;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Save;

private:
	UFUNCTION()
	void OnTopDownClicked();

	UFUNCTION()
	void OnIsometricClicked();

	UFUNCTION()
	void OnFirstPersonClicked();

	UFUNCTION()
	void OnRotateCanvasClicked();

	UFUNCTION()
	void OnSaveClicked();
};
