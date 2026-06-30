#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteReal/EditMode/Materials/FMaterialDataRow.h"
#include "MaterialAttributesPanelWidget.generated.h"

class UBaseSlider;
class UBaseInput;
class UBaseButton;
class UButton;

UCLASS()
class INTEREAL_API UMaterialAttributesPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "MaterialAttributesPanel")
	void RefreshForMaterial(const FMaterialDataRow& MaterialData);

	UFUNCTION(BlueprintCallable, Category = "MaterialAttributesPanel")
	void ResetForSurfaceWithoutMaterial();
	
protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBaseSlider> Slider_Metallic;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBaseInput> Input_MetallicValue;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBaseSlider> Slider_Specular;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBaseInput> Input_SpecularValue;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBaseSlider> Slider_Roughness;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBaseInput> Input_RoughnessValue;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBaseSlider> Slider_Emissive;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBaseInput> Input_EmissiveValue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBaseButton> Btn_Cancel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBaseButton> Btn_Apply;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Close;

private:
	UFUNCTION()
	void HandleMetallicChanged(float NewValue);

	UFUNCTION()
	void HandleSpecularChanged(float NewValue);

	UFUNCTION()
	void HandleRoughnessChanged(float NewValue);

	UFUNCTION()
	void HandleEmissiveChanged(float NewValue);

	UFUNCTION()
	void HandleMetallicInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleSpecularInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleRoughnessInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleEmissiveInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleCancelClicked();

	UFUNCTION()
	void HandleApplyClicked();

	void ApplyToSelectedSurface();
	void RefreshValueReadouts();
	void SetSliderValueSilently(UBaseSlider* Slider, float Value);
	void SetInputText(UBaseInput* Input, float Value) const;
	
	void InitializeFromInputValues();
	float ReadInputValue(UBaseInput* Input, float DefaultValue) const;
	float ClampMaterialAttributeValue(float Value, float MinValue, float MaxValue) const;

private:
	FMaterialDataRow CurrentMaterialData;
	FMaterialDataRow SnapshotMaterialData;
	bool bHasMaterialData = false;
	bool bIsRefreshing = false;
};