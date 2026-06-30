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

	// 뷰 모드 전환이나 툴바 토글처럼 외부에서 일괄적으로 아이콘을 켜고 끌 때 사용
	UFUNCTION(BlueprintCallable, Category = "LightFixture")
	void SetIconForcedHidden(bool bInForcedHidden);

	// 라이트 타입별로 다른 월드 아이콘을 쓰고 싶을 때 지정 — 해당 타입 슬롯이 비어있으면
	// 카탈로그 썸네일(FFurnitureDataRow::DisplayImage)을 그대로 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightFixture|Icon")
	TObjectPtr<UTexture2D> IconTexture_Point = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightFixture|Icon")
	TObjectPtr<UTexture2D> IconTexture_Spot = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightFixture|Icon")
	TObjectPtr<UTexture2D> IconTexture_Rect = nullptr;

	// 화면상 아이콘 픽셀 크기 — UMaterialBillboardComponent 기본값(32px)과 동일.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightFixture|Icon")
	float IconPixelSize = 32.0f;

	// M_LightIcon처럼 IconTexture(Texture2D 파라미터)와 TintColor(Vector 파라미터)를 가진
	// Unlit/Translucent 머터리얼을 지정해야 아이콘이 보인다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightFixture|Icon")
	TObjectPtr<UMaterialInterface> IconBaseMaterial = nullptr;

	// 선택 안 했을 때 / 했을 때 TintColor에 들어가는 색
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightFixture|Icon")
	FLinearColor IconTintColor_Normal = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightFixture|Icon")
	FLinearColor IconTintColor_Selected = FLinearColor(1.0f, 0.85f, 0.0f, 1.0f);

private:
	void UpdateIndicatorVisibility();
	void EnsureIconMaterialInstance();
	void UpdateIconMaterialParameters();
	UTexture2D* ResolveIconTexture() const;

	UPROPERTY(VisibleAnywhere, Category = "LightFixture")
	TObjectPtr<USpotLightComponent> SpotLightComponent;

	UPROPERTY(VisibleAnywhere, Category = "LightFixture")
	TObjectPtr<URectLightComponent> RectLightComponent;

	// 배치된 라이트 위치를 알려주는 빌보드 아이콘 — 메시가 작거나 천장에 묻혀도 위치를 알 수 있게 함
	UPROPERTY(VisibleAnywhere, Category = "LightFixture")
	TObjectPtr<UMaterialBillboardComponent> IconBillboardComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> IconMID;

	// 선택했을 때만 빛이 닿는 범위를 보여주는 와이어프레임 구
	UPROPERTY(VisibleAnywhere, Category = "LightFixture")
	TObjectPtr<USphereComponent> RadiusIndicatorComponent;

	FLightAttributes CurrentLightAttributes;
	bool bIsSelected = false;
	bool bIconForcedHidden = false;
};
