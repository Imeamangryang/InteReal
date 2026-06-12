#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteReal/ViewMode/ViewModeData.h"
#include "TopBarWidget.generated.h"

UCLASS()
class INTEREAL_API UTopBarWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "ViewMode")
	void ChangeViewMode(EHarnessViewMode NewMode);
};
