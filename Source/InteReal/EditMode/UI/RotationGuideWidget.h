#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RotationGuideWidget.generated.h"

class UTextBlock;
class URadialSlider;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGizmoRotationChanged, float, NewYawDegrees);

UCLASS()
class INTEREAL_API URotationGuideWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

public:
	void ShowForRotation(float InitialYawDegrees);
	void HideGuide();
	
	void UpdateRotation(float DeltaAngle);
	void ShowGuide();

	UPROPERTY(BlueprintAssignable)
	FOnGizmoRotationChanged OnRotationChanged;

	// WBP에서 RadialSlider 크기에 맞게 조정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RotationGuide")
	float WidgetRadius = 100.0f;

	// Projected gizmo center에서 각도 텍스트까지의 화면 픽셀 오프셋.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RotationGuide")
	FVector2D GizmoScreenOffset = FVector2D(16.0f, 16.0f);

	// WBP가 Fill Screen이어도 런타임에서는 작은 오버레이로 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RotationGuide")
	FVector2D GuideScreenSize = FVector2D(120.0f, 48.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RotationGuide")
	bool bShowRadialSlider = false;

private:
	UPROPERTY(meta=(BindWidget))
	UTextBlock* Txt_Angle;

	UPROPERTY(meta=(BindWidgetOptional))
	URadialSlider* RS_Angle;

	float BaseYawDegrees = 0.0f;

	UFUNCTION()
	void HandleSliderValueChanged(float Value);
};
