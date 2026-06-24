#include "FurnitureItemWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "InteReal/SubSystems/InteRealUISubSystem.h"

void UFurnitureItemWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Display)
	{
		Button_Display->OnClicked.RemoveDynamic(this, &UFurnitureItemWidget::HandleDisplayButtonClicked);
		Button_Display->OnClicked.AddDynamic(this, &UFurnitureItemWidget::HandleDisplayButtonClicked);
	}

	if (!FurnitureRowName.IsNone())
	{
		SetupFurnitureItem(FurnitureRowName, FurnitureData);
	}
}

void UFurnitureItemWidget::SetupFurnitureItem(const FName& InRowName, const FFurnitureDataRow& InFurnitureData)
{
	FurnitureRowName = InRowName;
	FurnitureData = InFurnitureData;

	if (TextBlock_Name)
	{
		TextBlock_Name->SetText(FurnitureData.DisplayName);
	}

	if (TextBlock_Width)
	{
		TextBlock_Width->SetText(MakeSizeText(TEXT("W"), FurnitureData.Width));
	}

	if (TextBlock_Depth)
	{
		TextBlock_Depth->SetText(MakeSizeText(TEXT("D"), FurnitureData.Depth));
	}

	if (TextBlock_Height)
	{
		TextBlock_Height->SetText(MakeSizeText(TEXT("H"), FurnitureData.Height));
	}
}

void UFurnitureItemWidget::HandleDisplayButtonClicked()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UInteRealUISubSystem* UISubsystem = GameInstance->GetSubsystem<UInteRealUISubSystem>())
		{
			UISubsystem->NotifyFurnitureSpawn(FurnitureData);
		}
	}
}

FText UFurnitureItemWidget::MakeSizeText(const TCHAR* Prefix, float Value) const
{
	return FText::FromString(FString::Printf(TEXT("%s %.0f"), Prefix, Value));
}