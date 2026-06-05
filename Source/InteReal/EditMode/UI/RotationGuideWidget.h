#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RotationGuideWidget.generated.h"

class UImage;
class UTextBlock;
class UMaterialInstanceDynamic;

UCLASS()
class INTEREAL_API URotationGuideWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

public:
	void UpdateRotation(float DeltaAngle);

	void ShowGuide();
	void HideGuide();

private:
	UPROPERTY(meta=(BindWidget))
	UTextBlock* Txt_Angle;
	
};
