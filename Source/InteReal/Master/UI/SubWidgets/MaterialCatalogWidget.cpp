#include "MaterialCatalogWidget.h"
#include "InteReal/Master/UI/Components/MaterialItemWidget.h"
#include "Components/EditableText.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/DataTable.h"
#include "Materials/FMaterialDataRow.h"

void UMaterialCatalogWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (EditText_Search)
	{
		EditText_Search->OnTextChanged.RemoveDynamic(this, &UMaterialCatalogWidget::HandleSearchTextChanged);
		EditText_Search->OnTextChanged.AddDynamic(this, &UMaterialCatalogWidget::HandleSearchTextChanged);
	}

	RebuildMaterialList();
}

void UMaterialCatalogWidget::SetMaterialDataTable(UDataTable* InMaterialDataTable)
{
	MaterialDataTable = InMaterialDataTable;
	RebuildMaterialList();
}

void UMaterialCatalogWidget::RebuildMaterialList()
{
	if (!WrapBox_Material)
	{
		return;
	}

	WrapBox_Material->ClearChildren();

	if (!MaterialDataTable || !MaterialItemWidgetClass)
	{
		return;
	}

	const TArray<FName> RowNames = MaterialDataTable->GetRowNames();

	for (const FName& RowName : RowNames)
	{
		const FMaterialDataRow* MaterialRow = MaterialDataTable->FindRow<FMaterialDataRow>(RowName, TEXT("MaterialCatalog"));
		if (!MaterialRow)
		{
			continue;
		}

		UMaterialItemWidget* ItemWidget = CreateWidget<UMaterialItemWidget>(GetOwningPlayer(), MaterialItemWidgetClass);
		if (!ItemWidget)
		{
			continue;
		}

		ItemWidget->SetupMaterialItem(RowName, *MaterialRow);

		if (UWrapBoxSlot* WrapBoxSlot = WrapBox_Material->AddChildToWrapBox(ItemWidget))
		{
			WrapBoxSlot->SetPadding(FMargin(4.0f));
		}
	}

	ApplyMaterialFilters();
}

void UMaterialCatalogWidget::HandleSearchTextChanged(const FText& InText)
{
	CurrentSearchText = InText.ToString();
	ApplyMaterialFilters();
}

void UMaterialCatalogWidget::ApplyMaterialFilters()
{
	if (!WrapBox_Material)
	{
		return;
	}

	for (UWidget* ChildWidget : WrapBox_Material->GetAllChildren())
	{
		UMaterialItemWidget* ItemWidget = Cast<UMaterialItemWidget>(ChildWidget);
		if (!ItemWidget)
		{
			continue;
		}

		const bool bShouldShow = DoesItemMatchFilter(ItemWidget);
		ItemWidget->SetVisibility(bShouldShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

bool UMaterialCatalogWidget::DoesItemMatchFilter(const UMaterialItemWidget* ItemWidget) const
{
	if (!ItemWidget)
	{
		return false;
	}

	const FString NormalizedSearchText = CurrentSearchText.TrimStartAndEnd();

	if (NormalizedSearchText.IsEmpty())
	{
		return true;
	}

	const FString RowNameString = ItemWidget->GetMaterialRowName().ToString();
	const FString DisplayNameString = ItemWidget->GetMaterialData().DisplayName.ToString();

	return RowNameString.Contains(NormalizedSearchText, ESearchCase::IgnoreCase)
		|| DisplayNameString.Contains(NormalizedSearchText, ESearchCase::IgnoreCase);
}