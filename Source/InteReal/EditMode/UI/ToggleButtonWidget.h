#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ToggleButtonWidget.generated.h"

class UButton;
class UBorder;
class UImage;
class UTextBlock;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnToggleButtonClicked);

UCLASS()
class INTEREAL_API UToggleButtonWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

public:
	// 켜짐 상태를 계속 유지하는 토글 (Move/Rotate on-off 버튼용)
	UFUNCTION(BlueprintCallable, Category = "ToggleButton")
	void SetActiveVisual(bool bActive);

	// 아이콘+라벨 자체를 바꾸는 모드 전환 버튼용 (Grid Snap/Free Placement 병합 버튼)
	UFUNCTION(BlueprintCallable, Category = "ToggleButton")
	void SetIconAndLabel(UTexture2D* NewIcon, const FText& NewLabel);

	UPROPERTY(BlueprintAssignable, Category = "ToggleButton")
	FOnToggleButtonClicked OnToggled;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToggleButton")
	FLinearColor ActiveColor = FLinearColor(0.550f, 0.430f, 0.330f, 0.6f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToggleButton")
	FLinearColor InactiveColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToggleButton")
	FLinearColor HoverColor = FLinearColor(0.850f, 0.810f, 0.760f, 0.25f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToggleButton")
	FLinearColor PressedColor = FLinearColor(0.700f, 0.620f, 0.530f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToggleButton")
	UTexture2D* IconTexture = nullptr;

private:
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Icon;

	UPROPERTY(meta = (BindWidget))
	UBorder* B_Color;

	UPROPERTY(meta = (BindWidget))
	UImage* Img_Icon;

	bool bIsActive = false;
	bool bIsHovered = false;
	bool bIsPressed = false;

	void RefreshBorderColor();

	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();

	UFUNCTION()
	void HandleUnhovered();

	UFUNCTION()
	void HandlePressed();

	UFUNCTION()
	void HandleReleased();
};
