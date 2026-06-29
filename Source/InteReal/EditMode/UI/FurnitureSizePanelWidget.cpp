#include "FurnitureSizePanelWidget.h"
#include "Components/EditableText.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "InteReal/EditMode/Furniture/Furniture.h"
#include "InteReal/EditMode/Furniture/FFurnitureDataRow.h"
#include "Kismet/GameplayStatics.h"
#include "InteReal/Master/InteRealPlayerController.h"

void UFurnitureSizePanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_ApplySize)
	{
		Button_ApplySize->OnClicked.AddDynamic(this, &UFurnitureSizePanelWidget::HandleApplyClicked);
	}
}

void UFurnitureSizePanelWidget::RefreshForFurniture(AFurniture* Furniture)
{
	TargetFurniture = Furniture;

	if (!Furniture)
	{
		ClearFurnitureSummary();
		return;
	}

	RefreshFurnitureSummary(Furniture);

	const FVector SizeCm = Furniture->GetCurrentSizeCm();

	FNumberFormattingOptions Opts;
	Opts.SetMinimumFractionalDigits(1);
	Opts.SetMaximumFractionalDigits(1);

	if (ET_Value_X)
	{
		ET_Value_X->SetText(FText::AsNumber(SizeCm.X, &Opts));
	}
	if (ET_Value_Y)
	{
		ET_Value_Y->SetText(FText::AsNumber(SizeCm.Y, &Opts));
	}
	if (ET_Value_Z)
	{
		ET_Value_Z->SetText(FText::AsNumber(SizeCm.Z, &Opts));
	}
}

void UFurnitureSizePanelWidget::HandleApplyClicked()
{
	AFurniture* Furniture = TargetFurniture.Get();
	if (!Furniture)
	{
		return;
	}

	const float X = ET_Value_X ? FCString::Atof(*ET_Value_X->GetText().ToString()) : 0.0f;
	const float Y = ET_Value_Y ? FCString::Atof(*ET_Value_Y->GetText().ToString()) : 0.0f;
	const float Z = ET_Value_Z ? FCString::Atof(*ET_Value_Z->GetText().ToString()) : 0.0f;

	Furniture->SetTargetSizeCm(FVector(X, Y, Z));

	if (AInteRealPlayerController* PC = Cast<AInteRealPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		PC->SyncFurnitureSizeChangeToFloorPlan2D(Furniture);
	}
}

void UFurnitureSizePanelWidget::RefreshFurnitureSummary(AFurniture* Furniture)
{
	if (!Furniture || !Furniture->HasFurnitureDataRow())
	{
		ClearFurnitureSummary();
		return;
	}

	const FFurnitureDataRow& FurnitureData = Furniture->GetFurnitureDataRow();

	if (Image_SelectedFurniture)
	{
		Image_SelectedFurniture->SetBrushFromTexture(FurnitureData.DisplayImage);
		Image_SelectedFurniture->SetVisibility(FurnitureData.DisplayImage ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (Text_SelectedFurnitureName)
	{
		Text_SelectedFurnitureName->SetText(FurnitureData.DisplayName);
	}

	if (Text_SelectedFurnitureInfo)
	{
		Text_SelectedFurnitureInfo->SetText(FText::FromString(FurnitureData.SKU));
	}
}

void UFurnitureSizePanelWidget::ClearFurnitureSummary()
{
	if (Image_SelectedFurniture)
	{
		Image_SelectedFurniture->SetBrushFromTexture(nullptr);
		Image_SelectedFurniture->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (Text_SelectedFurnitureName)
	{
		Text_SelectedFurnitureName->SetText(FText::FromString(TEXT("-")));
	}

	if (Text_SelectedFurnitureInfo)
	{
		Text_SelectedFurnitureInfo->SetText(FText::GetEmpty());
	}
}
