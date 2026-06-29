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
#include "InteReal/Network/InteRealNetworkSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"

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

	if (ShouldUseApiAssets())
	{
		if (bHasFetchedApiAssets)
		{
			RebuildFurnitureListFromApiAssets(CachedApiAssetList);
		}
		else
		{
			RequestApiAssets();
		}
		return;
	}

	RebuildFurnitureListFromDataTable();
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
	ButtonStyle.NormalPadding = FMargin(12.0f, 1.5f, 12.0f, 1.5f);
	ButtonStyle.PressedPadding = FMargin(12.0f, 1.5f, 12.0f, 1.5f);

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
	ApplyCategoryButtonStyle(Button_Shelf, bUseCategoryFilter && CurrentCategoryFilter == EFurnitureAssetCategory::Shelf);
}

bool UFurnitureCatalogWidget::ShouldUseApiAssets() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UInteRealNetworkSubsystem* Network = GameInstance ? GameInstance->GetSubsystem<UInteRealNetworkSubsystem>() : nullptr;
	return Network && !Network->bUseMockData;
}

void UFurnitureCatalogWidget::RequestApiAssets()
{
	if (bIsFetchingApiAssets)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UInteRealNetworkSubsystem* Network = GameInstance ? GameInstance->GetSubsystem<UInteRealNetworkSubsystem>() : nullptr;
	if (!Network)
	{
		return;
	}

	bIsFetchingApiAssets = true;

	FOnAssetsReceived Delegate;
	Delegate.BindDynamic(this, &UFurnitureCatalogWidget::HandleApiAssetsReceived);
	Network->FetchAllAssets(0, ApiAssetFetchLimit, Delegate);
}

void UFurnitureCatalogWidget::HandleApiAssetsReceived(bool bSuccess, const FUnrealAssetListResponse& Response)
{
	bIsFetchingApiAssets = false;

	if (!bSuccess)
	{
		bHasFetchedApiAssets = false;
		UE_LOG(LogTemp, Warning, TEXT("[FurnitureCatalog] FetchAllAssets failed. Falling back to DataTable."));

		if (WrapBox_Furniture)
		{
			WrapBox_Furniture->ClearChildren();
			RebuildFurnitureListFromDataTable();
		}
		return;
	}

	CachedApiAssetList = Response;
	bHasFetchedApiAssets = true;

	UE_LOG(LogTemp, Log, TEXT("[FurnitureCatalog] FetchAllAssets success. Total=%d Items=%d"), Response.total, Response.items.Num());

	RebuildFurnitureList();
}

void UFurnitureCatalogWidget::RebuildFurnitureListFromDataTable()
{
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

void UFurnitureCatalogWidget::RebuildFurnitureListFromApiAssets(const FUnrealAssetListResponse& Response)
{
	if (!FurnitureItemWidgetClass)
	{
		return;
	}

	for (const FInteRealAssetData& Asset : Response.items)
	{
		if (!IsFurnitureAsset(Asset))
		{
			continue;
		}

		const FName RowName(*FString::Printf(TEXT("API_Furniture_%d"), Asset.id));
		const FFurnitureDataRow FurnitureRow = ConvertAssetToFurnitureRow(Asset);

		UFurnitureItemWidget* ItemWidget = CreateWidget<UFurnitureItemWidget>(GetOwningPlayer(), FurnitureItemWidgetClass);
		if (!ItemWidget)
		{
			continue;
		}

		ItemWidget->SetupFurnitureItem(RowName, FurnitureRow);

		if (UWrapBoxSlot* WrapBoxSlot = WrapBox_Furniture->AddChildToWrapBox(ItemWidget))
		{
			WrapBoxSlot->SetPadding(FMargin(4.0f));
		}
	}

	ApplyFurnitureFilters();
}

bool UFurnitureCatalogWidget::IsFurnitureAsset(const FInteRealAssetData& Asset) const
{
	const FString Text = FString::Printf(TEXT("%s %s"), *Asset.name, *Asset.unreal_path).ToLower();

	if (Text.Contains(TEXT("material")) || Text.Contains(TEXT("/materials/")) || Text.Contains(TEXT("mi_")) || Text.Contains(TEXT("/mi_")))
	{
		return false;
	}

	if (!Asset.unreal_path.IsEmpty())
	{
		if (LoadObject<UStaticMesh>(nullptr, *Asset.unreal_path))
		{
			return true;
		}

		if (LoadObject<UMaterialInterface>(nullptr, *Asset.unreal_path))
		{
			return false;
		}
	}

	return true;
}

EFurnitureAssetCategory UFurnitureCatalogWidget::ResolveFurnitureCategoryFromAsset(const FInteRealAssetData& Asset) const
{
	const FString Text = FString::Printf(TEXT("%s %s"), *Asset.name, *Asset.unreal_path).ToLower();

	if (Text.Contains(TEXT("bed"))) return EFurnitureAssetCategory::Bed;
	if (Text.Contains(TEXT("chair")) || Text.Contains(TEXT("sofa") ) || Text.Contains(TEXT("seat"))) return EFurnitureAssetCategory::Seating;
	if (Text.Contains(TEXT("table")) || Text.Contains(TEXT("desk"))) return EFurnitureAssetCategory::TableDesk;
	if (Text.Contains(TEXT("storage")) || Text.Contains(TEXT("cabinet")) || Text.Contains(TEXT("closet"))) return EFurnitureAssetCategory::Storage;
	if (Text.Contains(TEXT("light")) || Text.Contains(TEXT("lamp"))) return EFurnitureAssetCategory::Lighting;
	if (Text.Contains(TEXT("tv")) || Text.Contains(TEXT("monitor")) || Text.Contains(TEXT("electronics"))) return EFurnitureAssetCategory::Electronics;
	if (Text.Contains(TEXT("kitchen"))) return EFurnitureAssetCategory::Kitchen;
	if (Text.Contains(TEXT("bath"))) return EFurnitureAssetCategory::Bathroom;
	if (Text.Contains(TEXT("decor"))) return EFurnitureAssetCategory::Decor;
	if (Text.Contains(TEXT("mirror"))) return EFurnitureAssetCategory::Mirror;
	if (Text.Contains(TEXT("plant"))) return EFurnitureAssetCategory::Plant;
	if (Text.Contains(TEXT("shelf"))) return EFurnitureAssetCategory::Shelf;

	return EFurnitureAssetCategory::None;
}

FFurnitureDataRow UFurnitureCatalogWidget::ConvertAssetToFurnitureRow(const FInteRealAssetData& Asset) const
{
	FFurnitureDataRow Row;
	Row.ID = Asset.id;
	Row.DisplayName = FText::FromString(Asset.name.IsEmpty() ? FString::Printf(TEXT("Asset %d"), Asset.id) : Asset.name);
	Row.Width = 100.0f;
	Row.Depth = 100.0f;
	Row.Dimensions = FIntPoint(FMath::RoundToInt(Row.Width), FMath::RoundToInt(Row.Depth));
	Row.Category = ResolveFurnitureCategoryFromAsset(Asset);
	Row.FurnitureMesh = Asset.unreal_path.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *Asset.unreal_path);
	return Row;
}