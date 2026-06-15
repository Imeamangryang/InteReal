#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "IconTextButtonWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnIconTextButtonClicked,
	FName, ButtonId,
	UIconTextButtonWidget*, ButtonWidget
);

UCLASS(BlueprintType, Blueprintable)
class INTEREAL_API UIconTextButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 버튼 구분용 ID.
	 * 예: "Move", "Rotate", "Delete"
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Text Button", meta = (ExposeOnSpawn = true))
	FName ButtonId = NAME_None;

	/**
	 * 하단에 표시할 텍스트
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Text Button", meta = (ExposeOnSpawn = true))
	FText LabelText;

	/**
	 * 상단 아이콘 텍스처
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Text Button", meta = (ExposeOnSpawn = true))
	TObjectPtr<UTexture2D> IconTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Text Button|Layout")
	FVector2D IconSize = FVector2D(48.0f, 48.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Text Button|Layout")
	FMargin ContentPadding = FMargin(8.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Text Button|Layout")
	float IconTextGap = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Text Button|Style")
	FLinearColor TextColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Text Button|Style")
	FSlateFontInfo TextFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Text Button|Style")
	FButtonStyle ButtonStyle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Text Button|Behavior")
	bool bHideIconWhenTextureIsNull = true;

	UPROPERTY(BlueprintAssignable, Category = "Icon Text Button|Event")
	FOnIconTextButtonClicked OnIconTextButtonClicked;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButton> RootButton;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UVerticalBox> ContentBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> LabelTextBlock;

public:
	UFUNCTION(BlueprintCallable, Category = "Icon Text Button")
	void SetButtonId(FName InButtonId);

	UFUNCTION(BlueprintCallable, Category = "Icon Text Button")
	void SetLabelText(const FText& InText);

	UFUNCTION(BlueprintCallable, Category = "Icon Text Button")
	void SetIconTexture(UTexture2D* InTexture);

	UFUNCTION(BlueprintCallable, Category = "Icon Text Button")
	void SetIconSize(FVector2D InSize);

	UFUNCTION(BlueprintCallable, Category = "Icon Text Button")
	void SetButtonEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Icon Text Button")
	FName GetButtonId() const { return ButtonId; }

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;




private:
	void ApplyAppearance();

	UFUNCTION()
	void HandleButtonClicked();
};