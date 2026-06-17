#pragma once

#include "CoreMinimal.h"
#include "PlacementTooltipWidget.h"
#include "Blueprint/UserWidget.h"
#include "Subsystem/InteriorPlacementSubsystem.h"
#include "UserGuideWidget.generated.h"

UCLASS()
class INTEREAL_API UUserGuideWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional))
	TObjectPtr<UPlacementTooltipWidget> PlacementTooltip;

	void UpdateTooltip(EPlacementInvalidReason Reason);
};
