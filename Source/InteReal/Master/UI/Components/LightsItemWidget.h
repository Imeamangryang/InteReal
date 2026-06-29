// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteReal/EditMode/Lights/FLightsDataRow.h"
#include "LightsItemWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS(BlueprintType, Blueprintable)
class INTEREAL_API ULightsItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "InteReal|LightsItem")
	void SetupLightItem(const FName& InRowName, const FLightsDataRow& InLightData);

	UFUNCTION(BlueprintPure, Category = "InteReal|LightsItem")
	FName GetLightRowName() const { return LightRowName; }

	UFUNCTION(BlueprintPure, Category = "InteReal|LightsItem")
	const FLightsDataRow& GetLightData() const { return LightData; }

protected:
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void HandleDisplayButtonClicked();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Display = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TextBlock_Name = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "InteReal|LightsItem", meta = (AllowPrivateAccess = "true"))
	FName LightRowName = NAME_None;

	// ExposeOnSpawn이라 WBP에서 DisplayImage 등을 직접 바인딩해서 쓸 수 있다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InteReal|LightsItem", meta = (ExposeOnSpawn = "true", AllowPrivateAccess = "true"))
	FLightsDataRow LightData;
};
