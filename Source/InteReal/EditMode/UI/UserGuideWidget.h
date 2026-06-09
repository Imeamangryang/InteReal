#pragma once

#include "CoreMinimal.h"
#include "PlacementTooltipWidget.h"
#include "Blueprint/UserWidget.h"
#include "InteReal/EditMode/Managers/InteriorPlacementManager.h"
#include "UserGuideWidget.generated.h"

UCLASS()
class INTEREAL_API UUserGuideWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
	TObjectPtr<UPlacementTooltipWidget> PlacementTooltip;

	void UpdateTooltip(EPlacementInvalidReason Reason);
};
