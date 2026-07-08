// Fill out your copyright notice in the Description page of Project Settings.

#include "LightsCatalogWidget.h"
#include "Components/EditableText.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/DataTable.h"
#include "InteReal/Master/UI/Components/BaseToggleSwitch.h"
#include "InteReal/Master/UI/Components/LightsItemWidget.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"

void ULightsCatalogWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (EditText_Search)
	{
		EditText_Search->OnTextChanged.RemoveDynamic(this, &ULightsCatalogWidget::HandleSearchTextChanged);
		EditText_Search->OnTextChanged.AddDynamic(this, &ULightsCatalogWidget::HandleSearchTextChanged);
	}

	if (Toggle_Enabled)
	{
		Toggle_Enabled->OnToggleChanged.RemoveDynamic(this, &ULightsCatalogWidget::HandleEnabledToggled);
		Toggle_Enabled->OnToggleChanged.AddDynamic(this, &ULightsCatalogWidget::HandleEnabledToggled);

		if (const UInteriorPlacementSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UInteriorPlacementSubsystem>() : nullptr)
		{
			Toggle_Enabled->SetIsOn(Subsystem->IsLightFixtureIconsVisible(), false);
		}
	}

	RebuildLightsList();
}

void ULightsCatalogWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!Toggle_Enabled)
	{
		return;
	}

	if (const UInteriorPlacementSubsystem* Subsystem = GetWorld()
		? GetWorld()->GetSubsystem<UInteriorPlacementSubsystem>()
		: nullptr)
	{
		const bool bActualState = Subsystem->IsLightFixtureIconsVisible();
		if (Toggle_Enabled->bIsOn != bActualState)
		{
			Toggle_Enabled->SetIsOn(bActualState, false);
		}
	}
}

void ULightsCatalogWidget::SetLightsDataTable(UDataTable* InLightsDataTable)
{
	LightsDataTable = InLightsDataTable;
	RebuildLightsList();
}

void ULightsCatalogWidget::RebuildLightsList()
{
	if (!WrapBox_Lights) return;
	WrapBox_Lights->ClearChildren();

	if (!LightsDataTable || !LightsItemWidgetClass) return;

	const TArray<FName> RowNames = LightsDataTable->GetRowNames();

	for (const FName& RowName : RowNames)
	{
		const FLightsDataRow* LightRow = LightsDataTable->FindRow<FLightsDataRow>(
			RowName, TEXT("LightsCatalog"));
		if (!LightRow) continue;

		ULightsItemWidget* ItemWidget = CreateWidget<ULightsItemWidget>(GetOwningPlayer(), LightsItemWidgetClass);
		if (!ItemWidget) continue;

		ItemWidget->SetupLightItem(RowName, *LightRow);

		if (UWrapBoxSlot* WrapBoxSlot = WrapBox_Lights->AddChildToWrapBox(ItemWidget))
		{
			WrapBoxSlot->SetPadding(FMargin(4.0f));
		}
	}

	ApplyLightsFilters();
}

void ULightsCatalogWidget::ApplyLightsFilters()
{
	if (!WrapBox_Lights) return;

	for (UWidget* ChildWidget : WrapBox_Lights->GetAllChildren())
	{
		ULightsItemWidget* ItemWidget = Cast<ULightsItemWidget>(ChildWidget);
		if (!ItemWidget) continue;

		const bool bShouldShow = DoesItemMatchFilter(ItemWidget);
		ItemWidget->SetVisibility(bShouldShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void ULightsCatalogWidget::HandleSearchTextChanged(const FText& InText)
{
	CurrentSearchText = InText.ToString();
	ApplyLightsFilters();
}

bool ULightsCatalogWidget::DoesItemMatchFilter(const ULightsItemWidget* ItemWidget) const
{
	if (!ItemWidget) return false;

	const FString NormalizedSearchText = CurrentSearchText.TrimStartAndEnd();
	if (NormalizedSearchText.IsEmpty())
	{
		return true;
	}

	const FString DisplayNameString = ItemWidget->GetLightData().DisplayName.ToString();
	return DisplayNameString.Contains(NormalizedSearchText, ESearchCase::IgnoreCase);
}

void ULightsCatalogWidget::HandleEnabledToggled(bool bIsOn)
{
	if (UInteriorPlacementSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UInteriorPlacementSubsystem>() : nullptr)
	{
		Subsystem->SetLightFixtureIconsVisible(bIsOn);
	}
}
