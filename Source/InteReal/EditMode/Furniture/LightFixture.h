#pragma once

#include "CoreMinimal.h"
#include "Furniture.h"
#include "LightFixture.generated.h"

class USpotLightComponent;
class URectLightComponent;

UCLASS()
class INTEREAL_API ALightFixture : public AFurniture
{
	GENERATED_BODY()

public:
	ALightFixture();

	virtual void ApplyFurnitureRow(const FFurnitureDataRow& InFurnitureRow) override;
	
	UFUNCTION(BlueprintPure, Category = "LightFixture")
	FLightAttributes GetLightAttributes() const { return CurrentLightAttributes; }

	UFUNCTION(BlueprintCallable, Category = "LightFixture")
	void SetLightAttributes(const FLightAttributes& InAttributes);

private:
	UPROPERTY(VisibleAnywhere, Category = "LightFixture")
	TObjectPtr<USpotLightComponent> SpotLightComponent;

	UPROPERTY(VisibleAnywhere, Category = "LightFixture")
	TObjectPtr<URectLightComponent> RectLightComponent;

	FLightAttributes CurrentLightAttributes;
};
