#pragma once

#include "CoreMinimal.h"
#include "Furniture.h"
#include "LightFixture.generated.h"

class USpotLightComponent;
class URectLightComponent;
class UMaterialBillboardComponent;
class USphereComponent;
class UTexture2D;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class INTEREAL_API ALightFixture : public AFurniture
{
	GENERATED_BODY()

public:
	ALightFixture();

	virtual void ApplyFurnitureRow(const FFurnitureDataRow& InFurnitureRow) override;
	virtual void SetPlacementState(EPlacementState NewState) override;
	virtual void SetSelected(bool bSelected) override;

	UFUNCTION(BlueprintPure, Category = "LightFixture")
	FLightAttributes GetLightAttributes() const { return CurrentLightAttributes; }

	UFUNCTION(BlueprintCallable, Category = "LightFixture")
	void SetLightAttributes(const FLightAttributes& InAttributes);
	
	UFUNCTION(BlueprintCallable, Category = "LightFixture")
	void SetIconForcedHidden(bool bInForcedHidden);
	
	void SetIconPixelSize(float InPixelSize);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightFixture|Icon")
	TObjectPtr<UTexture2D> IconTexture_Point = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightFixture|Icon")
	TObjectPtr<UTexture2D> IconTexture_Spot = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightFixture|Icon")
	TObjectPtr<UTexture2D> IconTexture_Rect = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightFixture|Icon")
	float IconPixelSize = 32.0f;

	// 아이콘 가로/세로 비율 (1.0 = 정사각형, 1.0보다 크면 가로로 더 넓게 표시)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightFixture|Icon")
	float IconAspectRatio = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightFixture|Icon")
	TObjectPtr<UMaterialInterface> IconBaseMaterial = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightFixture|Icon")
	FName IconTextureParamName = TEXT("IconTexture");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightFixture|Icon")
	FName IconTintParamName = TEXT("TintColor");
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightFixture|Icon")
	FLinearColor IconTintColor_Normal = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightFixture|Icon")
	FLinearColor IconTintColor_Selected = FLinearColor(1.0f, 0.85f, 0.0f, 1.0f);

private:
	void UpdateIndicatorVisibility();
	void UpdateIconMaterialParameters();
	void RebuildIconSpriteElement();
	UTexture2D* ResolveIconTexture() const;

	UPROPERTY(VisibleAnywhere, Category = "LightFixture")
	TObjectPtr<USpotLightComponent> SpotLightComponent;

	UPROPERTY(VisibleAnywhere, Category = "LightFixture")
	TObjectPtr<URectLightComponent> RectLightComponent;
	
	UPROPERTY(VisibleAnywhere, Category = "LightFixture")
	TObjectPtr<UMaterialBillboardComponent> IconBillboardComponent;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> IconMaterialInstance;
	
	UPROPERTY(VisibleAnywhere, Category = "LightFixture")
	TObjectPtr<USphereComponent> RadiusIndicatorComponent;

	FLightAttributes CurrentLightAttributes;
	bool bIsSelected = false;
	bool bIconForcedHidden = false;
};
