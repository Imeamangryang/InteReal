#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Subsystem/InteriorPlacementSubsystem.h"
#include "PlacementTooltipWidget.generated.h"

UCLASS()
class INTEREAL_API UPlacementTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* Txt_Message;

	// 컨트롤러가 이유만 넘기면 텍스트는 여기서 결정
	UFUNCTION(BlueprintCallable)
	void ShowReason(EPlacementInvalidReason Reason);
};
