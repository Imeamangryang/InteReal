#include "MaterialCatalogWidget.h"
#include "InteReal/Master/UI/Components/MaterialItemWidget.h"
#include "Components/EditableText.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/DataTable.h"
#include "Materials/FMaterialDataRow.h"
#include "InteReal/Network/InteRealNetworkSubsystem.h"
#include "Engine/GameInstance.h"
#include "Materials/MaterialInterface.h"

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

	if (ShouldUseApiAssets())
	{
		if (bHasFetchedApiAssets)
		{
			WrapBox_Material->ClearChildren();
			RebuildMaterialListFromApiAssets(CachedApiAssetList);

			if (WrapBox_Material->GetChildrenCount() == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("[MaterialCatalog] API assets received but no material item was created. Total=%d"), CachedApiAssetList.items.Num());
			}
		}
		else
		{
			RequestApiAssets();
		}
		return;
	}

	WrapBox_Material->ClearChildren();
	RebuildMaterialListFromDataTable();
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

bool UMaterialCatalogWidget::ShouldUseApiAssets() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UInteRealNetworkSubsystem* Network = GameInstance ? GameInstance->GetSubsystem<UInteRealNetworkSubsystem>() : nullptr;
	return Network && !Network->bUseMockData;
}

void UMaterialCatalogWidget::RequestApiAssets()
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
	Delegate.BindDynamic(this, &UMaterialCatalogWidget::HandleApiAssetsReceived);
	Network->FetchAllAssets(0, ApiAssetFetchLimit, Delegate);
}

void UMaterialCatalogWidget::HandleApiAssetsReceived(bool bSuccess, const FUnrealAssetListResponse& Response)
{
	bIsFetchingApiAssets = false;

	if (!bSuccess)
	{
		bHasFetchedApiAssets = false;
		UE_LOG(LogTemp, Warning, TEXT("[MaterialCatalog] FetchAllAssets failed. Falling back to DataTable."));

		if (WrapBox_Material)
		{
			WrapBox_Material->ClearChildren();
			RebuildMaterialListFromDataTable();
		}
		return;
	}

	CachedApiAssetList = Response;
	bHasFetchedApiAssets = true;

	UE_LOG(LogTemp, Log, TEXT("[MaterialCatalog] FetchAllAssets success. Total=%d Items=%d"), Response.total, Response.items.Num());

	RebuildMaterialList();
}

void UMaterialCatalogWidget::RebuildMaterialListFromDataTable()
{
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

void UMaterialCatalogWidget::RebuildMaterialListFromApiAssets(const FUnrealAssetListResponse& Response)
{
	if (!MaterialItemWidgetClass)
	{
		return;
	}

	for (const FInteRealAssetData& Asset : Response.items)
	{
		if (!IsMaterialAsset(Asset))
		{
			continue;
		}

		const FName RowName(*FString::Printf(TEXT("API_Material_%d"), Asset.id));
		const FMaterialDataRow MaterialRow = ConvertAssetToMaterialRow(Asset);

		UMaterialItemWidget* ItemWidget = CreateWidget<UMaterialItemWidget>(GetOwningPlayer(), MaterialItemWidgetClass);
		if (!ItemWidget)
		{
			continue;
		}

		ItemWidget->SetupMaterialItem(RowName, MaterialRow);

		if (UWrapBoxSlot* WrapBoxSlot = WrapBox_Material->AddChildToWrapBox(ItemWidget))
		{
			WrapBoxSlot->SetPadding(FMargin(4.0f));
		}
	}

	ApplyMaterialFilters();
}

bool UMaterialCatalogWidget::IsMaterialAsset(const FInteRealAssetData& Asset) const
{
	const FString Text = FString::Printf(TEXT("%s %s"), *Asset.name, *Asset.unreal_path).ToLower();

	if (!Asset.unreal_path.IsEmpty())
	{
		if (LoadObject<UMaterialInterface>(nullptr, *Asset.unreal_path))
		{
			return true;
		}

		if (LoadObject<UStaticMesh>(nullptr, *Asset.unreal_path))
		{
			return false;
		}
	}

	return Text.Contains(TEXT("material")) ||
		Text.Contains(TEXT("/materials/")) ||
		Text.Contains(TEXT("/m_")) ||
		Text.Contains(TEXT("mi_"));
}

FMaterialDataRow UMaterialCatalogWidget::ConvertAssetToMaterialRow(const FInteRealAssetData& Asset) const
{
	FMaterialDataRow Row;
	Row.DisplayName = FText::FromString(Asset.name.IsEmpty() ? FString::Printf(TEXT("Asset %d"), Asset.id) : Asset.name);
	Row.Material = Asset.unreal_path.IsEmpty() ? nullptr : LoadObject<UMaterialInterface>(nullptr, *Asset.unreal_path);
	return Row;
}