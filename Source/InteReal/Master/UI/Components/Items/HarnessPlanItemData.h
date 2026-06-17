#pragma once

#include "CoreMinimal.h"
#include "HarnessPlanItemData.generated.h"

UCLASS(BlueprintType, Blueprintable)
class INTEREAL_API UHarnessPlanItemData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plan Data")
	FString PlanName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plan Data")
	FString DateString;
};