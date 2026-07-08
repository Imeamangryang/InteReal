// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LightsCatalogWidget.generated.h"

class UBaseToggleSwitch;
class UDataTable;
class UEditableText;
class UWrapBox;
class ULightsItemWidget;

UCLASS(BlueprintType, Blueprintable)
class INTEREAL_API ULightsCatalogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "InteReal|LightsCatalog")
	void SetLightsDataTable(UDataTable* InLightsDataTable);

	UFUNCTION(BlueprintCallable, Category = "InteReal|LightsCatalog")
	void RebuildLightsList();

	UFUNCTION(BlueprintCallable, Category = "InteReal|LightsCatalog")
	void ApplyLightsFilters();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION()
	void HandleSearchTextChanged(const FText& InText);

	bool DoesItemMatchFilter(const ULightsItemWidget* ItemWidget) const;

	UFUNCTION()
	void HandleEnabledToggled(bool bIsOn);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InteReal|LightsCatalog", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDataTable> LightsDataTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InteReal|LightsCatalog", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ULightsItemWidget> LightsItemWidgetClass;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableText> EditText_Search = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBaseToggleSwitch> Toggle_Enabled;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWrapBox> WrapBox_Lights = nullptr;

	FString CurrentSearchText;
};
