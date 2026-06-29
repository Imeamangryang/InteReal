// Fill out your copyright notice in the Description page of Project Settings.

#include "LightsItemWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "InteReal/SubSystems/InteRealUISubSystem.h"

void ULightsItemWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Display)
	{
		Button_Display->OnClicked.RemoveDynamic(this, &ULightsItemWidget::HandleDisplayButtonClicked);
		Button_Display->OnClicked.AddDynamic(this, &ULightsItemWidget::HandleDisplayButtonClicked);
	}

	if (!LightRowName.IsNone())
	{
		SetupLightItem(LightRowName, LightData);
	}
}

void ULightsItemWidget::SetupLightItem(const FName& InRowName, const FLightsDataRow& InLightData)
{
	LightRowName = InRowName;
	LightData = InLightData;

	if (TextBlock_Name)
	{
		TextBlock_Name->SetText(LightData.DisplayName);
	}
}

void ULightsItemWidget::HandleDisplayButtonClicked()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UInteRealUISubSystem* UISubsystem = GameInstance->GetSubsystem<UInteRealUISubSystem>())
		{
			UISubsystem->NotifyFurnitureSpawn(LightData.ToFurnitureDataRow());
		}
	}
}
