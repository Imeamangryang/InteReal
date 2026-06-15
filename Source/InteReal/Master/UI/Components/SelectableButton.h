#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "SelectableButton.generated.h"

class UTextBlock;
class UInteRealThemeData;

/**
 * USelectableButton
 * 상태 토글형 아웃라인 버튼 베이스 클래스 (하이브리드 패턴)
 */
UCLASS(Abstract)
class INTEREAL_API USelectableButton : public UUserWidget
{
	GENERATED_BODY()

public:
	/** meta = (BindWidget)을 사용하여 WBP의 버튼과 바인딩 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> Btn_Main;

	/** meta = (BindWidget)을 사용하여 WBP의 텍스트와 바인딩 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI")
	TObjectPtr<UTextBlock> Txt_Label;

	/** 버튼에 표시될 텍스트 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FText ButtonText;

	/** 선택 상태 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bIsSelected = false;

	/** 버튼 내부 여백 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FMargin ContentPadding = FMargin(24.f, 12.f);

	/** 테마 데이터 에셋 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TObjectPtr<UInteRealThemeData> ThemeData;

public:
	/** 런타임에 선택 상태를 변경하고 UI를 업데이트하는 함수 */
	UFUNCTION(BlueprintCallable, Category = "Logic")
	void SetIsSelected(bool bNewSelected);

protected:
	virtual void NativePreConstruct() override;

private:
	/** 상태에 따른 비주얼 업데이트 로직 */
	void UpdateSelectionUI();
};
