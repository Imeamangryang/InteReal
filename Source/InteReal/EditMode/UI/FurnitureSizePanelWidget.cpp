#include "FurnitureSizePanelWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Engine/Texture2D.h"
#include "InteReal/EditMode/Furniture/Furniture.h"
#include "InteReal/EditMode/Furniture/FFurnitureDataRow.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "InteReal/Master/UI/Components/BaseInput.h"
#include "InteReal/Master/UI/Components/BaseButton.h"
#include "Kismet/GameplayStatics.h"
#include "InteReal/Master/InteRealPlayerController.h"

void UFurnitureSizePanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_ApplySize)
	{
		Button_ApplySize->OnButtonClicked.AddDynamic(this, &UFurnitureSizePanelWidget::HandleApplyClicked);
	}

	if (Button_CancelSize)
	{
		Button_CancelSize->OnButtonClicked.AddDynamic(this, &UFurnitureSizePanelWidget::HandleCancelClicked);
	}
	
	if (Input_Value_X && Input_Value_X->Input_Main)
	{
		Input_Value_X->Input_Main->OnTextChanged.AddDynamic(this, &UFurnitureSizePanelWidget::HandleSizeInputChanged);
	}

	if (Input_Value_Y && Input_Value_Y->Input_Main)
	{
		Input_Value_Y->Input_Main->OnTextChanged.AddDynamic(this, &UFurnitureSizePanelWidget::HandleSizeInputChanged);
	}

	if (Input_Value_Z && Input_Value_Z->Input_Main)
	{
		Input_Value_Z->Input_Main->OnTextChanged.AddDynamic(this, &UFurnitureSizePanelWidget::HandleSizeInputChanged);
	}
}

void UFurnitureSizePanelWidget::NativeDestruct()
{
	RevertPreviewSize();

	if (Input_Value_X && Input_Value_X->Input_Main)
	{
		Input_Value_X->Input_Main->OnTextChanged.RemoveDynamic(this, &UFurnitureSizePanelWidget::HandleSizeInputChanged);
	}

	if (Input_Value_Y && Input_Value_Y->Input_Main)
	{
		Input_Value_Y->Input_Main->OnTextChanged.RemoveDynamic(this, &UFurnitureSizePanelWidget::HandleSizeInputChanged);
	}

	if (Input_Value_Z && Input_Value_Z->Input_Main)
	{
		Input_Value_Z->Input_Main->OnTextChanged.RemoveDynamic(this, &UFurnitureSizePanelWidget::HandleSizeInputChanged);
	}

	Super::NativeDestruct();
}

void UFurnitureSizePanelWidget::RefreshForFurniture(AFurniture* Furniture)
{
	if (TargetFurniture.IsValid() && TargetFurniture.Get() != Furniture)
	{
		RevertPreviewSize();
	}

	TargetFurniture = Furniture;
	bSizeChangeCommitted = false;
	bHasOriginalSize = false;

	if (!Furniture)
	{
		ClearFurnitureSummary();
		return;
	}

	OriginalSizeCm = Furniture->GetCurrentSizeCm();
	bHasOriginalSize = true;

	RefreshFurnitureSummary(Furniture);
	RefreshInputFieldsFromFurniture(Furniture);
}


void UFurnitureSizePanelWidget::RefreshInputFieldsFromFurniture(AFurniture* Furniture)
{
	if (!Furniture)
	{
		return;
	}

	const FVector SizeMm = Furniture->GetCurrentSizeCm() * 10.0f;

	FNumberFormattingOptions Opts;
	Opts.SetMinimumFractionalDigits(0);
	Opts.SetMaximumFractionalDigits(0);
	Opts.SetUseGrouping(true);

	bRefreshingInputs = true;

	if (Input_Value_X && Input_Value_X->Input_Main)
	{
		Input_Value_X->Input_Main->SetText(FText::AsNumber(SizeMm.X, &Opts));
	}
	if (Input_Value_Y && Input_Value_Y->Input_Main)
	{
		Input_Value_Y->Input_Main->SetText(FText::AsNumber(SizeMm.Y, &Opts));
	}
	if (Input_Value_Z && Input_Value_Z->Input_Main)
	{
		Input_Value_Z->Input_Main->SetText(FText::AsNumber(SizeMm.Z, &Opts));
	}

	bRefreshingInputs = false;
}

void UFurnitureSizePanelWidget::HandleApplyClicked()
{
	ApplyPreviewSizeFromInputs();
	CommitPreviewSize();
}

void UFurnitureSizePanelWidget::CommitPreviewSize()
{
	AFurniture* Furniture = TargetFurniture.Get();
	if (!Furniture)
	{
		return;
	}

	OriginalSizeCm = Furniture->GetCurrentSizeCm();
	bHasOriginalSize = true;
	bSizeChangeCommitted = true;

	RefreshInputFieldsFromFurniture(Furniture);
}

void UFurnitureSizePanelWidget::HandleCancelClicked()
{
	RevertPreviewSize();
	SetVisibility(ESlateVisibility::Hidden);
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


float UFurnitureSizePanelWidget::ParseSizeInputMm(UBaseInput* Input) const
{
	if (!Input || !Input->Input_Main)
	{
		return 0.0f;
	}

	FString Text = Input->Input_Main->GetText().ToString();
	Text.ReplaceInline(TEXT(","), TEXT(""));
	Text.ReplaceInline(TEXT(" "), TEXT(""));
	Text.ReplaceInline(TEXT("mm"), TEXT(""), ESearchCase::IgnoreCase);

	return FCString::Atof(*Text);
}

void UFurnitureSizePanelWidget::HandleSizeInputChanged(const FText& Text)
{
	if (bRefreshingInputs)
	{
		return;
	}

	ApplyPreviewSizeFromInputs();
}

void UFurnitureSizePanelWidget::ApplyPreviewSizeFromInputs()
{
	AFurniture* Furniture = TargetFurniture.Get();
	if (!Furniture)
	{
		return;
	}

	if (!bHasOriginalSize)
	{
		OriginalSizeCm = Furniture->GetCurrentSizeCm();
		bHasOriginalSize = true;
	}

	const float XMm = ParseSizeInputMm(Input_Value_X);
	const float YMm = ParseSizeInputMm(Input_Value_Y);
	const float ZMm = ParseSizeInputMm(Input_Value_Z);

	if (XMm <= 0.0f || YMm <= 0.0f || ZMm <= 0.0f)
	{
		return;
	}

	Furniture->SetTargetSizeCm(FVector(XMm, YMm, ZMm) * 0.1f);

	if (UWorld* World = Furniture->GetWorld())
	{
		if (UInteriorPlacementSubsystem* PlacementSubsystem = World->GetSubsystem<UInteriorPlacementSubsystem>())
		{
			PlacementSubsystem->RevalidatePlacedFurnitureWarnings();
		}
	}

	if (AInteRealPlayerController* PC = Cast<AInteRealPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		PC->SyncFurnitureSizeChangeToFloorPlan2D(Furniture);
	}
}

void UFurnitureSizePanelWidget::RevertPreviewSize()
{
	AFurniture* Furniture = TargetFurniture.Get();
	if (!Furniture || !bHasOriginalSize || bSizeChangeCommitted)
	{
		return;
	}

	Furniture->SetTargetSizeCm(OriginalSizeCm);

	if (UWorld* World = Furniture->GetWorld())
	{
		if (UInteriorPlacementSubsystem* PlacementSubsystem = World->GetSubsystem<UInteriorPlacementSubsystem>())
		{
			PlacementSubsystem->RevalidatePlacedFurnitureWarnings();
		}
	}

	if (AInteRealPlayerController* PC = Cast<AInteRealPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		PC->SyncFurnitureSizeChangeToFloorPlan2D(Furniture);
	}

	RefreshInputFieldsFromFurniture(Furniture);
}