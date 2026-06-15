#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableText.h" // 변경됨
#include "Components/Border.h"       // 추가됨
#include "BaseInput.generated.h"

class UInteRealThemeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBaseInputTextChanged, const FText&, Text);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBaseInputTextCommitted, const FText&, Text, ETextCommit::Type, CommitMethod);

UCLASS(Abstract)
class INTEREAL_API UBaseInput : public UUserWidget
{
	GENERATED_BODY()

public:
	// 배경과 테두리를 담당할 보더
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI")
	TObjectPtr<UBorder> Border_Bg;

	// 순수 텍스트 입력을 담당할 텍스트 (Box가 아님!)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI")
	TObjectPtr<UEditableText> Input_Main;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FText HintText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TObjectPtr<UInteRealThemeData> ThemeData;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnBaseInputTextChanged OnBaseTextChanged;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnBaseInputTextCommitted OnBaseTextCommitted;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void HandleTextChanged(const FText& Text);

	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);
};