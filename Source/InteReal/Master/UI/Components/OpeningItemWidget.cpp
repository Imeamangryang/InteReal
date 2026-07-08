#include "OpeningItemWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "InteReal/SubSystems/InteRealUISubSystem.h"

void UOpeningItemWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Display)
	{
		Button_Display->OnClicked.RemoveDynamic(this, &UOpeningItemWidget::HandleDisplayButtonClicked);
		Button_Display->OnClicked.AddDynamic(this, &UOpeningItemWidget::HandleDisplayButtonClicked);
	}

	if (!OpeningRowName.IsNone())
	{
		SetupOpeningItem(OpeningRowName, OpeningData);
	}
}

void UOpeningItemWidget::SetupOpeningItem(const FName& InRowName, const FOpeningAssetDataRow& InOpeningData)
{
	OpeningRowName = InRowName;
	OpeningData = InOpeningData;

	if (TextBlock_Name)
	{
		TextBlock_Name->SetText(FText::FromName(OpeningRowName));
	}

	if (TextBlock_Kind)
	{
		TextBlock_Kind->SetText(MakeOpeningKindText(OpeningData.OpeningKind));
	}

	if (TextBlock_YawOffset)
	{
		TextBlock_YawOffset->SetText(FText::FromString(FString::Printf(TEXT("Yaw %.0f°"), OpeningData.OpeningMeshYawOffset)));
	}
}

void UOpeningItemWidget::HandleDisplayButtonClicked()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UInteRealUISubSystem* UISubsystem = GameInstance->GetSubsystem<UInteRealUISubSystem>())
		{
			UISubsystem->NotifyOpeningAssetSelected(OpeningData);
		}
	}
}

FText UOpeningItemWidget::MakeOpeningKindText(EOpeningAssetKind InKind) const
{
	switch (InKind)
	{
	case EOpeningAssetKind::Window:
		return FText::FromString(TEXT("창문"));

	case EOpeningAssetKind::EntranceDoor:
		return FText::FromString(TEXT("현관문"));

	case EOpeningAssetKind::SlidingDoor:
		return FText::FromString(TEXT("미닫이문"));

	case EOpeningAssetKind::Door:
	default:
		return FText::FromString(TEXT("문"));
	}
}