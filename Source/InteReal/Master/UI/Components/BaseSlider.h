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

	// Progress_Track을 감싸는 SizeBox가 있으면 TrackThickness로 두께(높이)를 조절할 수 있음 (없으면 무시됨)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class USizeBox> SizeBox_Track;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TObjectPtr<UInteRealThemeData> ThemeData;

	// 손잡이(원형 노브) 크기. 기본값은 기존과 동일한 20 / 28(호버 시)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float ThumbSize = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float HoveredThumbSize = 28.0f;

	// 트랙(막대) 두께. SizeBox_Track이 바인딩되어 있을 때만 적용됨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float TrackThickness = 8.0f;

	// Slider_Main이 WBP_BaseSlider 내부에 감춰져 있어 바깥(이 위젯을 쓰는 패널)에서 직접 못 건드리므로 여기로 노출
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float MinValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float MaxValue = 1.0f;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnBaseSliderValueChanged OnBaseValueChanged;

	// Slider_Main의 Min/Max 범위 그대로 값을 설정/조회 (Progress_Track 채움 비율은 내부에서 자동 계산)
	UFUNCTION(BlueprintCallable, Category = "Logic")
	void SetValue(float NewValue);

	UFUNCTION(BlueprintCallable, Category = "Logic")
	float GetValue() const;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void HandleOnValueChanged(float NewValue);

	// Slider_Main의 Min/Max 범위를 반영해 Progress_Track 채움 비율을 갱신
	void UpdateProgressPercent();
};
