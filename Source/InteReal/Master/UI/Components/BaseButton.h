#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "BaseButton.generated.h"

class UTextBlock;
class UInteRealThemeData;

/**
 * UBaseButton
 * 기본 텍스트 버튼 베이스 클래스 (하이브리드 패턴)
 */
UCLASS(Abstract)
class INTEREAL_API UBaseButton : public UUserWidget
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

	/** 주 버튼(Navy) 여부. false일 경우 보조 버튼(White) 스타일 적용 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bIsPrimary = true;

	/** 버튼 내부 여백 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FMargin ContentPadding = FMargin(24.f, 12.f);

	/** 테마 데이터 에셋 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TObjectPtr<UInteRealThemeData> ThemeData;

	/** 외부에서 바인딩 가능한 버튼 클릭 델리게이트 */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnButtonClickedEvent OnButtonClicked;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

private:
	/** 내부 버튼 클릭 핸들러 */
	UFUNCTION()
	void HandleButtonClicked();

	/** 테마 및 스타일 적용 로직 */
	void ApplyThemeStyle();
};
