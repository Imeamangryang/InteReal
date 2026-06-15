#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "Types/SlateEnums.h"
#include "BaseComboBox.generated.h"

class UInteRealThemeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBaseComboBoxSelectionChanged, FString, SelectedItem, ESelectInfo::Type, SelectionType);

/**
 * UBaseComboBox
 * 테마가 적용된 베이스 콤보박스 클래스
 */
UCLASS(Abstract)
class INTEREAL_API UBaseComboBox : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI")
	TObjectPtr<UComboBoxString> ComboBox_Main;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TArray<FString> DefaultOptions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TObjectPtr<UInteRealThemeData> ThemeData;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnBaseComboBoxSelectionChanged OnBaseSelectionChanged;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void HandleSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
};
