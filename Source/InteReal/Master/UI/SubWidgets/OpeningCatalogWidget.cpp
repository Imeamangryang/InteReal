#include "OpeningCatalogWidget.h"
#include <Openings/FOpeningAssetDataRow.h>
#include "InteReal/Master/UI/Components/OpeningItemWidget.h"
#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "InteReal/Network/InteRealNetworkSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

void UOpeningCatalogWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (EditText_Search)
	{
		EditText_Search->OnTextChanged.RemoveDynamic(this, &UOpeningCatalogWidget::HandleSearchTextChanged);
		EditText_Search->OnTextChanged.AddDynamic(this, &UOpeningCatalogWidget::HandleSearchTextChanged);
	}

	if (Button_All)
	{
		Button_All->OnClicked.RemoveDynamic(this, &UOpeningCatalogWidget::HandleAllCategoryClicked);
		Button_All->OnClicked.AddDynamic(this, &UOpeningCatalogWidget::HandleAllCategoryClicked);
	}
	
	if (Button_Door)
	{
		Button_Door->OnClicked.RemoveDynamic(this, &UOpeningCatalogWidget::HandleDoorCategoryClicked);
		Button_Door->OnClicked.AddDynamic(this, &UOpeningCatalogWidget::HandleDoorCategoryClicked);
	}

	if (Button_Window)
	{
		Button_Window->OnClicked.RemoveDynamic(this, &UOpeningCatalogWidget::HandleWindowCategoryClicked);
		Button_Window->OnClicked.AddDynamic(this, &UOpeningCatalogWidget::HandleWindowCategoryClicked);
	}

	RebuildOpeningList();
	RefreshCategoryButtonStyles();
}

void UOpeningCatalogWidget::SetOpeningDataTable(UDataTable* InOpeningDataTable)
{
	OpeningDataTable = InOpeningDataTable;
	RebuildOpeningList();
}

void UOpeningCatalogWidget::RebuildOpeningList()
{
	if (!WrapBox_Opening)
	{
		return;
	}

	WrapBox_Opening->ClearChildren();
	CategoryByItem.Reset();
	SearchTextByItem.Reset();

	if (ShouldUseApiAssets())
	{
		if (bHasFetchedApiAssets)
		{
			RebuildOpeningListFromApiAssets(CachedApiAssetList);
		}
		else
		{
			RequestApiAssets();
		}
		return;
	}

	RebuildOpeningListFromDataTable();
}

void UOpeningCatalogWidget::HandleSearchTextChanged(const FText& InText)
{
	CurrentSearchText = InText.ToString();
	ApplyOpeningFilters();
}

void UOpeningCatalogWidget::SetCategoryFilter(EOpeningCatalogCategory InCategory)
{
	CurrentCategoryFilter = InCategory;
	ApplyOpeningFilters();
	RefreshCategoryButtonStyles();
}

void UOpeningCatalogWidget::HandleAllCategoryClicked()
{
	SetCategoryFilter(EOpeningCatalogCategory::All);
}

void UOpeningCatalogWidget::HandleDoorCategoryClicked()
{
	SetCategoryFilter(EOpeningCatalogCategory::Door);
}

void UOpeningCatalogWidget::HandleWindowCategoryClicked()
{
	SetCategoryFilter(EOpeningCatalogCategory::Window);
}

void UOpeningCatalogWidget::ApplyOpeningFilters()
{
	if (!WrapBox_Opening)
	{
		return;
	}

	for (UWidget* ChildWidget : WrapBox_Opening->GetAllChildren())
	{
		UOpeningItemWidget* ItemWidget = Cast<UOpeningItemWidget>(ChildWidget);
		if (!ItemWidget)
		{
			continue;
		}

		const bool bShouldShow = DoesItemMatchFilter(ItemWidget);
		ItemWidget->SetVisibility(bShouldShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

bool UOpeningCatalogWidget::DoesItemMatchFilter(const UOpeningItemWidget* ItemWidget) const
{
	if (!ItemWidget)
	{
		return false;
	}

	const TObjectKey<UOpeningItemWidget> ItemKey(const_cast<UOpeningItemWidget*>(ItemWidget));
	const EOpeningCatalogCategory* ItemCategory = CategoryByItem.Find(ItemKey);
	if (!ItemCategory)
	{
		return false;
	}

	if (CurrentCategoryFilter != EOpeningCatalogCategory::All && *ItemCategory != CurrentCategoryFilter)
	{
		return false;
	}

	const FString NormalizedSearchText = CurrentSearchText.TrimStartAndEnd();
	if (NormalizedSearchText.IsEmpty())
	{
		return true;
	}

	const FString* SearchText = SearchTextByItem.Find(ItemKey);
	return SearchText && SearchText->Contains(NormalizedSearchText, ESearchCase::IgnoreCase);
}

bool UOpeningCatalogWidget::TryResolveOpeningCategory(const FOpeningAssetDataRow& Row, EOpeningCatalogCategory& OutCategory) const
{
	if (IsWindowRow(Row))
	{
		OutCategory = EOpeningCatalogCategory::Window;
		return true;
	}

	if (IsDoorRow(Row))
	{
		OutCategory = EOpeningCatalogCategory::Door;
		return true;
	}

	return false;
}

bool UOpeningCatalogWidget::IsDoorRow(const FOpeningAssetDataRow& Row) const
{
	return Row.OpeningKind == EOpeningAssetKind::Door || Row.OpeningKind == EOpeningAssetKind::EntranceDoor || Row.OpeningKind == EOpeningAssetKind::SlidingDoor;
}

bool UOpeningCatalogWidget::IsWindowRow(const FOpeningAssetDataRow& Row) const
{
	return Row.OpeningKind == EOpeningAssetKind::Window;
}

bool UOpeningCatalogWidget::AddOpeningItem(const FName& RowName, const FOpeningAssetDataRow& Row)
{
	if (!WrapBox_Opening || !OpeningItemWidgetClass)
	{
		return false;
	}

	EOpeningCatalogCategory OpeningCategory;
	if (!TryResolveOpeningCategory(Row, OpeningCategory))
	{
		return false;
	}

	UOpeningItemWidget* ItemWidget = CreateWidget<UOpeningItemWidget>(GetOwningPlayer(), OpeningItemWidgetClass);
	if (!ItemWidget)
	{
		return false;
	}

	ItemWidget->SetupOpeningItem(RowName, Row);

	const TObjectKey<UOpeningItemWidget> ItemKey(ItemWidget);
	CategoryByItem.Add(ItemKey, OpeningCategory);
	SearchTextByItem.Add(ItemKey, RowName.ToString());

	if (UWrapBoxSlot* WrapBoxSlot = WrapBox_Opening->AddChildToWrapBox(ItemWidget))
	{
		WrapBoxSlot->SetPadding(FMargin(4.0f));
	}

	return true;
}

FButtonStyle UOpeningCatalogWidget::MakeCategoryButtonStyle(bool bSelected) const
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

void UOpeningCatalogWidget::ApplyCategoryButtonTextColor(UButton* Button, bool bSelected) const
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

void UOpeningCatalogWidget::ApplyCategoryButtonStyle(UButton* Button, bool bSelected) const
{
	if (!Button)
	{
		return;
	}

	Button->SetStyle(MakeCategoryButtonStyle(bSelected));
	ApplyCategoryButtonTextColor(Button, bSelected);
}

void UOpeningCatalogWidget::RefreshCategoryButtonStyles()
{
	ApplyCategoryButtonStyle(Button_All, CurrentCategoryFilter == EOpeningCatalogCategory::All);
	ApplyCategoryButtonStyle(Button_Door, CurrentCategoryFilter == EOpeningCatalogCategory::Door);
	ApplyCategoryButtonStyle(Button_Window, CurrentCategoryFilter == EOpeningCatalogCategory::Window);
}

bool UOpeningCatalogWidget::ShouldUseApiAssets() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UInteRealNetworkSubsystem* Network = GameInstance ? GameInstance->GetSubsystem<UInteRealNetworkSubsystem>() : nullptr;
	return Network && !Network->bUseMockData;
}

void UOpeningCatalogWidget::RequestApiAssets()
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
	Delegate.BindDynamic(this, &UOpeningCatalogWidget::HandleApiAssetsReceived);
	Network->FetchAllAssets(0, ApiAssetFetchLimit, Delegate);
}

void UOpeningCatalogWidget::HandleApiAssetsReceived(bool bSuccess, const FUnrealAssetListResponse& Response)
{
	bIsFetchingApiAssets = false;

	if (!bSuccess)
	{
		bHasFetchedApiAssets = false;
		UE_LOG(LogTemp, Warning, TEXT("[OpeningCatalog] FetchAllAssets failed. Falling back to DataTable."));

		if (WrapBox_Opening)
		{
			WrapBox_Opening->ClearChildren();
			CategoryByItem.Reset();
			SearchTextByItem.Reset();
			RebuildOpeningListFromDataTable();
		}
		return;
	}

	CachedApiAssetList = Response;
	bHasFetchedApiAssets = true;

	UE_LOG(LogTemp, Log, TEXT("[OpeningCatalog] FetchAllAssets success. Total=%d Items=%d"), Response.total, Response.items.Num());

	RebuildOpeningList();
}

void UOpeningCatalogWidget::RebuildOpeningListFromDataTable()
{
	if (!OpeningDataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OpeningCatalog] OpeningDataTable is null."));
		return;
	}

	if (!OpeningItemWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OpeningCatalog] OpeningItemWidgetClass is null."));
		return;
	}

	const TArray<FName> RowNames = OpeningDataTable->GetRowNames();
	int32 AddedCount = 0;

	for (const FName& RowName : RowNames)
	{
		const FOpeningAssetDataRow* OpeningRow = OpeningDataTable->FindRow<FOpeningAssetDataRow>(RowName, TEXT("OpeningCatalog"));
		if (!OpeningRow)
		{
			UE_LOG(LogTemp, Warning, TEXT("[OpeningCatalog] Row type mismatch or invalid row. RowName=%s"), *RowName.ToString());
			continue;
		}

		if (AddOpeningItem(RowName, *OpeningRow))
		{
			++AddedCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[OpeningCatalog] DataTable rebuild finished. Table=%s Rows=%d Added=%d"), *GetNameSafe(OpeningDataTable), RowNames.Num(), AddedCount);

	ApplyOpeningFilters();
}

void UOpeningCatalogWidget::RebuildOpeningListFromApiAssets(const FUnrealAssetListResponse& Response)
{
	if (!OpeningItemWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OpeningCatalog] OpeningItemWidgetClass is null."));
		return;
	}

	int32 AddedCount = 0;

	for (const FInteRealAssetData& Asset : Response.items)
	{
		if (!IsOpeningAsset(Asset))
		{
			continue;
		}

		const FName RowName(*FString::Printf(TEXT("API_Opening_%d"), Asset.id));
		const FOpeningAssetDataRow OpeningRow = ConvertAssetToOpeningRow(Asset);
		if (AddOpeningItem(RowName, OpeningRow))
		{
			++AddedCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[OpeningCatalog] API rebuild finished. Items=%d Added=%d"), Response.items.Num(), AddedCount);

	ApplyOpeningFilters();
}

bool UOpeningCatalogWidget::IsOpeningAsset(const FInteRealAssetData& Asset) const
{
	const FString Text = FString::Printf(TEXT("%s %s"), *Asset.name, *Asset.unreal_path).ToLower();

	if (Text.Contains(TEXT("material")) || Text.Contains(TEXT("/materials/")) || Text.Contains(TEXT("mi_")) || Text.Contains(TEXT("/mi_")))
	{
		return false;
	}

	if (!Asset.unreal_path.IsEmpty())
	{
		if (!LoadObject<UStaticMesh>(nullptr, *Asset.unreal_path))
		{
			return false;
		}

		if (LoadObject<UMaterialInterface>(nullptr, *Asset.unreal_path))
		{
			return false;
		}
	}

	return Text.Contains(TEXT("door")) || Text.Contains(TEXT("window")) || Text.Contains(TEXT("/doors/")) || Text.Contains(TEXT("/windows/")) || Text.Contains(TEXT("문")) || Text.Contains(TEXT("창")) || Text.Contains(TEXT("현관")) || Text.Contains(TEXT("미닫이")) || Text.Contains(TEXT("슬라이딩"));
}

FOpeningAssetDataRow UOpeningCatalogWidget::ConvertAssetToOpeningRow(const FInteRealAssetData& Asset) const
{
	const FString Text = FString::Printf(TEXT("%s %s"), *Asset.name, *Asset.unreal_path).ToLower();

	FOpeningAssetDataRow Row;
	Row.OpeningMesh = Asset.unreal_path.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *Asset.unreal_path);
	Row.OpeningMeshYawOffset = 0.0f;

	if (Text.Contains(TEXT("window")) || Text.Contains(TEXT("창")))
	{
		Row.OpeningKind = EOpeningAssetKind::Window;
	}
	else if (Text.Contains(TEXT("sliding")) || Text.Contains(TEXT("미닫이")) || Text.Contains(TEXT("슬라이딩")))
	{
		Row.OpeningKind = EOpeningAssetKind::SlidingDoor;
	}
	else if (Text.Contains(TEXT("entrance")) || Text.Contains(TEXT("front")) || Text.Contains(TEXT("현관")))
	{
		Row.OpeningKind = EOpeningAssetKind::EntranceDoor;
	}
	else
	{
		Row.OpeningKind = EOpeningAssetKind::Door;
	}

	return Row;
}