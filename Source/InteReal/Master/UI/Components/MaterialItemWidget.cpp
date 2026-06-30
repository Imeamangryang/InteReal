#include "MaterialItemWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "InteReal/SubSystems/InteRealUISubSystem.h"

void UMaterialItemWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Display)
	{
		Button_Display->OnClicked.RemoveDynamic(this, &UMaterialItemWidget::HandleDisplayButtonClicked);
		Button_Display->OnClicked.AddDynamic(this, &UMaterialItemWidget::HandleDisplayButtonClicked);
	}

	if (!MaterialRowName.IsNone())
	{
		SetupMaterialItem(MaterialRowName, MaterialData);
	}
}

void UMaterialItemWidget::SetupMaterialItem(const FName& InRowName, const FMaterialDataRow& InMaterialData)
{
	MaterialRowName = InRowName;
	MaterialData = InMaterialData;

	if (TextBlock_Name)
	{
		TextBlock_Name->SetText(MaterialData.DisplayName);
	}
}

void UMaterialItemWidget::HandleDisplayButtonClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UInteRealUISubSystem* UISubsystem = GI->GetSubsystem<UInteRealUISubSystem>())
		{
			UISubsystem->NotifyWallMaterialDataChanged(MaterialData);
		}
	}
}