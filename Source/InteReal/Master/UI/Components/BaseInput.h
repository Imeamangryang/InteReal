#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableText.h" // 변경됨
#include "Components/Border.h"       // 추가됨
#include "BaseInput.generated.h"

class UInteRealThemeData;
class UTextBlock;

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

	// PrefixText를 보여줄 고정 라벨 — Input_Main과 별개라 사용자가 지우거나 수정할 수 없음 (없으면 PrefixText는 무시됨)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "UI")
	TObjectPtr<UTextBlock> Text_Prefix;

	// SuffixText를 보여줄 고정 라벨 — Input_Main 뒤에 붙는 단위 등 (없으면 SuffixText는 무시됨)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "UI")
	TObjectPtr<UTextBlock> Text_Suffix;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FText HintText;

	// 입력칸 맨 앞에 고정으로 박아둘 글자(예: "#"). 비어있으면 표시 안 함
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FText PrefixText;

	// 입력칸 맨 뒤에 고정으로 박아둘 단위 글자(예: "cd", "cm"). 비어있으면 표시 안 함
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FText SuffixText;

	// 배경 라운딩 정도. 기본값은 기존과 동일한 10
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float CornerRadius = 10.0f;

	// false로 두면 배경/테두리를 그리지 않음 — 여러 개를 하나의 공유 Border 안에 테두리 없이 묶어 넣을 때 사용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bShowBackground = true;

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