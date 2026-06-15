#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Slider.h"
#include "BaseSlider.generated.h"

class UInteRealThemeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBaseSliderValueChanged, float, NewValue);

/**
 * UBaseSlider
 * 테마가 적용된 베이스 슬라이더 클래스
 */
UCLASS(Abstract)
class INTEREAL_API UBaseSlider : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI")
	TObjectPtr<USlider> Slider_Main;

	UPROPERTY(meta = (BindWidget))
	class UProgressBar* Progress_Track;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TObjectPtr<UInteRealThemeData> ThemeData;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnBaseSliderValueChanged OnBaseValueChanged;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void HandleOnValueChanged(float NewValue);
};
