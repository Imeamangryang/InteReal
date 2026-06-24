#include "FurnitureCatalogWidget.h"
#include "InteReal/Master/UI/Components/FurnitureItemWidget.h"
#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/DataTable.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

void UFurnitureCatalogWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (EditText_Search)
	{
		EditText_Search->OnTextChanged.RemoveDynamic(this, &UFurnitureCatalogWidget::HandleSearchTextChanged);
		EditText_Search->OnTextChanged.AddDynamic(this, &UFurnitureCatalogWidget::HandleSearchTextChanged);
	}
	if (Button_All)
	{
		Button_All->OnClicked.RemoveDynamic(this, &UFurnitureCatalogWidget::HandleAllCategoryClicked);
		Button_All->OnClicked.AddDynamic(this, &UFurnitureCatalogWidget::HandleAllCategoryClicked);
	}

	if (Button_Bed)
	{
		Button_Bed->OnClicked.RemoveDynamic(this, &UFurnitureCatalogWidget::HandleBedCategoryClicked);
		Button_Bed->OnClicked.AddDynamic(this, &UFurnitureCatalogWidget::HandleBedCategoryClicked);
	}

	if (Button_Seating)
	{
		Button_Seating->OnClicked.RemoveDynamic(this, &UFurnitureCatalogWidget::HandleSeatingCategoryClicked);
		Button_Seating->OnClicked.AddDynamic(this, &UFurnitureCatalogWidget::HandleSeatingCategoryClicked);
	}

	if (Button_TableDesk)
	{
		Button_TableDesk->OnClicked.RemoveDynamic(this, &UFurnitureCatalogWidget::HandleTableDeskCategoryClicked);
		Button_TableDesk->OnClicked.AddDynamic(this, &UFurnitureCatalogWidget::HandleTableDeskCategoryClicked);
	}

	if (Button_Storage)
	{
		Button_Storage->OnClicked.RemoveDynamic(this, &UFurnitureCatalogWidget::HandleStorageCategoryClicked);
		Button_Storage->OnClicked.AddDynamic(this, &UFurnitureCatalogWidget::HandleStorageCategoryClicked);
	}

	if (Button_Lighting)
	{
		Button_Lighting->OnClicked.RemoveDynamic(this, &UFurnitureCatalogWidget::HandleLightingCategoryClicked);
		Button_Lighting->OnClicked.AddDynamic(this, &UFurnitureCatalogWidget::HandleLightingCategoryClicked);
	}

	if (Button_Electronics)
	{
		Button_Electronics->OnClicked.RemoveDynamic(this, &UFurnitureCatalogWidget::HandleElectronicsCategoryClicked);
		Button_Electronics->OnClicked.AddDynamic(this, &UFurnitureCatalogWidget::HandleElectronicsCategoryClicked);
	}

	if (Button_Kitchen)
	{
		Button_Kitchen->OnClicked.RemoveDynamic(this, &UFurnitureCatalogWidget::HandleKitchenCategoryClicked);
		Button_Kitchen->OnClicked.AddDynamic(this, &UFurnitureCatalogWidget::HandleKitchenCategoryClicked);
	}

	if (Button_Bathroom)
	{
		Button_Bathroom->OnClicked.RemoveDynamic(this, &UFurnitureCatalogWidget::HandleBathroomCategoryClicked);
		Button_Bathroom->OnClicked.AddDynamic(this, &UFurnitureCatalogWidget::HandleBathroomCategoryClicked);
	}

	if (Button_Decor)
	{
		Button_Decor->OnClicked.RemoveDynamic(this, &UFurnitureCatalogWidget::HandleDecorCategoryClicked);
		Button_Decor->OnClicked.AddDynamic(this, &UFurnitureCatalogWidget::HandleDecorCategoryClicked);
	}

	if (Button_Mirror)
	{
		Button_Mirror->OnClicked.RemoveDynamic(this, &UFurnitureCatalogWidget::HandleMirrorCategoryClicked);
		Button_Mirror->OnClicked.AddDynamic(this, &UFurnitureCatalogWidget::HandleMirrorCategoryClicked);
	}

	if (Button_Plant)
	{
		Button_Plant->OnClicked.RemoveDynamic(this, &UFurnitureCatalogWidget::HandlePlantCategoryClicked);
		Button_Plant->OnClicked.AddDynamic(this, &UFurnitureCatalogWidget::HandlePlantCategoryClicked);
	}

	if (Button_Rug)
	{
		Button_Rug->OnClicked.RemoveDynamic(this, &UFurnitureCatalogWidget::HandleRugCategoryClicked);
		Button_Rug->OnClicked.AddDynamic(this, &UFurnitureCatalogWidget::HandleRugCategoryClicked);
	}

	if (Button_Shelf)
	{
		Button_Shelf->OnClicked.RemoveDynamic(this, &UFurnitureCatalogWidget::HandleShelfCategoryClicked);
		Button_Shelf->OnClicked.AddDynamic(this, &UFurnitureCatalogWidget::HandleShelfCategoryClicked);
	}

	RebuildFurnitureList();
	RefreshCategoryButtonStyles();
}

void UFurnitureCatalogWidget::SetFurnitureDataTable(UDataTable* InFurnitureDataTable)
{
	FurnitureDataTable = InFurnitureDataTable;
	RebuildFurnitureList();
}

void UFurnitureCatalogWidget::RebuildFurnitureList()
{
	if (!WrapBox_Furniture)
	{
		return;
	}

	WrapBox_Furniture->ClearChildren();

	if (!FurnitureDataTable || !FurnitureItemWidgetClass)
	{
		return;
	}

	const TArray<FName> RowNames = FurnitureDataTable->GetRowNames();

	for (const FName& RowName : RowNames)
	{
		const FFurnitureDataRow* FurnitureRow = FurnitureDataTable->FindRow<FFurnitureDataRow>(RowName, TEXT("FurnitureCatalog"));
		if (!FurnitureRow)
		{
			continue;
		}

		UFurnitureItemWidget* ItemWidget = CreateWidget<UFurnitureItemWidget>(GetOwningPlayer(), FurnitureItemWidgetClass);
		if (!ItemWidget)
		{
			continue;
		}

		ItemWidget->SetupFurnitureItem(RowName, *FurnitureRow);

		if (UWrapBoxSlot* WrapBoxSlot = WrapBox_Furniture->AddChildToWrapBox(ItemWidget))
		{
			WrapBoxSlot->SetPadding(FMargin(4.0f));
		}
	}

	ApplyFurnitureFilters();
}

void UFurnitureCatalogWidget::HandleSearchTextChanged(const FText& InText)
{
	CurrentSearchText = InText.ToString();
	ApplyFurnitureFilters();
}

void UFurnitureCatalogWidget::SetCategoryFilter(EFurnitureAssetCategory InCategory)
{
	CurrentCategoryFilter = InCategory;
	bUseCategoryFilter = true;
	ApplyFurnitureFilters();
	RefreshCategoryButtonStyles();
}

void UFurnitureCatalogWidget::ClearCategoryFilter()
{
	CurrentCategoryFilter = EFurnitureAssetCategory::None;
	bUseCategoryFilter = false;
	ApplyFurnitureFilters();
	RefreshCategoryButtonStyles();
}

void UFurnitureCatalogWidget::HandleAllCategoryClicked()
{
	ClearCategoryFilter();
}

void UFurnitureCatalogWidget::HandleBedCategoryClicked()
{
	SetCategoryFilter(EFurnitureAssetCategory::Bed);
}

void UFurnitureCatalogWidget::HandleSeatingCategoryClicked()
{
	SetCategoryFilter(EFurnitureAssetCategory::Seating);
}

void UFurnitureCatalogWidget::HandleTableDeskCategoryClicked()
{
	SetCategoryFilter(EFurnitureAssetCategory::TableDesk);
}

void UFurnitureCatalogWidget::HandleStorageCategoryClicked()
{
	SetCategoryFilter(EFurnitureAssetCategory::Storage);
}

void UFurnitureCatalogWidget::HandleLightingCategoryClicked()
{
	SetCategoryFilter(EFurnitureAssetCategory::Lighting);
}

void UFurnitureCatalogWidget::HandleElectronicsCategoryClicked()
{
	SetCategoryFilter(EFurnitureAssetCategory::Electronics);
}

void UFurnitureCatalogWidget::HandleKitchenCategoryClicked()
{
	SetCategoryFilter(EFurnitureAssetCategory::Kitchen);
}

void UFurnitureCatalogWidget::HandleBathroomCategoryClicked()
{
	SetCategoryFilter(EFurnitureAssetCategory::Bathroom);
}

void UFurnitureCatalogWidget::HandleDecorCategoryClicked()
{
	SetCategoryFilter(EFurnitureAssetCategory::Decor);
}

void UFurnitureCatalogWidget::HandleMirrorCategoryClicked()
{
	SetCategoryFilter(EFurnitureAssetCategory::Mirror);
}

void UFurnitureCatalogWidget::HandlePlantCategoryClicked()
{
	SetCategoryFilter(EFurnitureAssetCategory::Plant);
}

void UFurnitureCatalogWidget::HandleRugCategoryClicked()
{
	SetCategoryFilter(EFurnitureAssetCategory::Rug);
}

void UFurnitureCatalogWidget::HandleShelfCategoryClicked()
{
	SetCategoryFilter(EFurnitureAssetCategory::Shelf);
}

void UFurnitureCatalogWidget::ApplyFurnitureFilters()
{
	if (!WrapBox_Furniture)
	{
		return;
	}

	for (UWidget* ChildWidget : WrapBox_Furniture->GetAllChildren())
	{
		UFurnitureItemWidget* ItemWidget = Cast<UFurnitureItemWidget>(ChildWidget);
		if (!ItemWidget)
		{
			continue;
		}

		const bool bShouldShow = DoesItemMatchFilter(ItemWidget);
		ItemWidget->SetVisibility(bShouldShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

bool UFurnitureCatalogWidget::DoesItemMatchFilter(const UFurnitureItemWidget* ItemWidget) const
{
	if (!ItemWidget)
	{
		return false;
	}

	if (bUseCategoryFilter && ItemWidget->GetFurnitureCategory() != CurrentCategoryFilter)
	{
		return false;
	}

	const FString NormalizedSearchText = CurrentSearchText.TrimStartAndEnd();

	if (NormalizedSearchText.IsEmpty())
	{
		return true;
	}

	const FString RowNameString = ItemWidget->GetFurnitureRowName().ToString();

	return RowNameString.Contains(NormalizedSearchText, ESearchCase::IgnoreCase);
}

FButtonStyle UFurnitureCatalogWidget::MakeCategoryButtonStyle(bool bSelected) const
{
	const FLinearColor SelectedColor(0.003347f, 0.023153f, 0.054480f, 1.0f);
	const FLinearColor NormalColor(0.791298f, 0.723055f, 0.651406f, 1.0f);
	const FLinearColor ButtonColor = bSelected ? SelectedColor : NormalColor;

	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.Tiling = ESlateBrushTileType::NoTile;
	Brush.Mirroring = ESlateBrushMirrorType::NoMirror;
	Brush.ImageType = ESlateBrushImageType::NoImage;
	Brush.ImageSize = FVector2D(32.0f, 16.0f);
	Brush.Margin = FMargin(0.0f);
	Brush.TintColor = FSlateColor(ButtonColor);
	Brush.OutlineSettings.CornerRadii = FVector4(10.0f, 10.0f, 10.0f, 10.0f);
	Brush.OutlineSettings.Color = FSlateColor(ButtonColor);
	Brush.OutlineSettings.Width = 1.0f;
	Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Brush.OutlineSettings.bUseBrushTransparency = true;

	FButtonStyle ButtonStyle;
	ButtonStyle.SetNormal(Brush);
	ButtonStyle.SetHovered(Brush);
	ButtonStyle.SetPressed(Brush);
	ButtonStyle.SetDisabled(Brush);
	ButtonStyle.NormalPadding = FMargin(5.0f, 0.0f, 5.0f, 0.0f);
	ButtonStyle.PressedPadding = FMargin(5.0f, 0.0f, 5.0f, 0.0f);

	return ButtonStyle;
}

void UFurnitureCatalogWidget::ApplyCategoryButtonTextColor(UButton* Button, bool bSelected) const
{
	if (!Button)
	{
		return;
	}

	UTextBlock* TextBlock = Cast<UTextBlock>(Button->GetChildAt(0));
	if (!TextBlock)
	{
		return;
	}

	const FLinearColor SelectedColor(0.003347f, 0.023153f, 0.054480f, 1.0f);
	const FLinearColor SelectedTextColor = FLinearColor::White;
	const FLinearColor NormalTextColor = SelectedColor;

	TextBlock->SetColorAndOpacity(FSlateColor(bSelected ? SelectedTextColor : NormalTextColor));
}

void UFurnitureCatalogWidget::ApplyCategoryButtonStyle(UButton* Button, bool bSelected) const
{
	if (!Button)
	{
		return;
	}

	Button->SetStyle(MakeCategoryButtonStyle(bSelected));
	ApplyCategoryButtonTextColor(Button, bSelected);
}

void UFurnitureCatalogWidget::RefreshCategoryButtonStyles()
{
	ApplyCategoryButtonStyle(Button_All, !bUseCategoryFilter);
	ApplyCategoryButtonStyle(Button_Bed, bUseCategoryFilter && CurrentCategoryFilter == EFurnitureAssetCategory::Bed);
	ApplyCategoryButtonStyle(Button_Seating, bUseCategoryFilter && CurrentCategoryFilter == EFurnitureAssetCategory::Seating);
	ApplyCategoryButtonStyle(Button_TableDesk, bUseCategoryFilter && CurrentCategoryFilter == EFurnitureAssetCategory::TableDesk);
	ApplyCategoryButtonStyle(Button_Storage, bUseCategoryFilter && CurrentCategoryFilter == EFurnitureAssetCategory::Storage);
	ApplyCategoryButtonStyle(Button_Lighting, bUseCategoryFilter && CurrentCategoryFilter == EFurnitureAssetCategory::Lighting);
	ApplyCategoryButtonStyle(Button_Electronics, bUseCategoryFilter && CurrentCategoryFilter == EFurnitureAssetCategory::Electronics);
	ApplyCategoryButtonStyle(Button_Kitchen, bUseCategoryFilter && CurrentCategoryFilter == EFurnitureAssetCategory::Kitchen);
	ApplyCategoryButtonStyle(Button_Bathroom, bUseCategoryFilter && CurrentCategoryFilter == EFurnitureAssetCategory::Bathroom);
	ApplyCategoryButtonStyle(Button_Decor, bUseCategoryFilter && CurrentCategoryFilter == EFurnitureAssetCategory::Decor);
	ApplyCategoryButtonStyle(Button_Mirror, bUseCategoryFilter && CurrentCategoryFilter == EFurnitureAssetCategory::Mirror);
	ApplyCategoryButtonStyle(Button_Plant, bUseCategoryFilter && CurrentCategoryFilter == EFurnitureAssetCategory::Plant);
	ApplyCategoryButtonStyle(Button_Rug, bUseCategoryFilter && CurrentCategoryFilter == EFurnitureAssetCategory::Rug);
	ApplyCategoryButtonStyle(Button_Shelf, bUseCategoryFilter && CurrentCategoryFilter == EFurnitureAssetCategory::Shelf);
}