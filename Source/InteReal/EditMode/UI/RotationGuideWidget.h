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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RotationGuide")
	float WidgetRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RotationGuide")
	FVector2D GizmoScreenOffset = FVector2D(16.0f, 16.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RotationGuide")
	FVector2D GuideScreenSize = FVector2D(120.0f, 48.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RotationGuide")
	bool bShowRadialSlider = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RotationGuide")
	float RadialSliderRevealThresholdDegrees = 0.5f;

private:
	UPROPERTY(meta=(BindWidget))
	UTextBlock* Txt_Angle;

	UPROPERTY(meta=(BindWidgetOptional))
	URadialSlider* RS_Angle;

	float BaseYawDegrees = 0.0f;
	bool bUpdatingRadialSliderFromCode = false;

	UFUNCTION()
	void HandleSliderValueChanged(float Value);
};
