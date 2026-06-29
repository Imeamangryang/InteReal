#include "EditModeLayoutWidget.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "InteReal/EditMode/2D/InteReal2DFloorPlanViewportWidget.h"

void UEditModeLayoutWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CurrentFloorPlanPanelWidth = GetTargetPanelWidth();

	if (ToggleButton)
	{
		ToggleButton->OnClicked.RemoveDynamic(this, &UEditModeLayoutWidget::HandleToggleButtonClicked);
		ToggleButton->OnClicked.AddDynamic(this, &UEditModeLayoutWidget::HandleToggleButtonClicked);
	}
	
	if (Button_SelectTool2D)
	{
		Button_SelectTool2D->OnClicked.Clear();
		Button_SelectTool2D->OnClicked.AddDynamic(this, &UEditModeLayoutWidget::HandleSelectToolButtonClicked);
	}

	if (Button_ObjectSnap2D)
	{
		Button_ObjectSnap2D->OnClicked.Clear();
		Button_ObjectSnap2D->OnClicked.AddDynamic(this, &UEditModeLayoutWidget::HandleObjectSnapButtonClicked);
	}

	ApplyLayout();
	ApplyFloorPlanToolBarState();
	UpdateFloorPlanToolButtonState();
	BP_OnFloorPlanPanelOpenChanged(bIsFloorPlanPanelOpen);
}

void UEditModeLayoutWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const float TargetPanelWidth = GetTargetPanelWidth();
	const float PreviousWidth = CurrentFloorPlanPanelWidth;

	if (bAnimatePanelWidth)
	{
		CurrentFloorPlanPanelWidth = FMath::FInterpTo(
			CurrentFloorPlanPanelWidth,
			TargetPanelWidth,
			InDeltaTime,
			PanelWidthInterpSpeed
		);
	}
	else
	{
		CurrentFloorPlanPanelWidth = TargetPanelWidth;
	}

	if (FMath::IsNearlyEqual(CurrentFloorPlanPanelWidth, TargetPanelWidth, 0.5f))
	{
		CurrentFloorPlanPanelWidth = TargetPanelWidth;
	}

	if (!FMath::IsNearlyEqual(PreviousWidth, CurrentFloorPlanPanelWidth, 0.1f))
	{
		ApplyLayout();
	}

	if (bPendingInitialDrawArea && FloorPlan2DWidget)
	{
		const FVector2D Size = FloorPlan2DWidget->GetCachedGeometry().GetLocalSize();

		if (Size.X > 1.f && Size.Y > 1.f)
		{
			ApplyFloorPlanDrawArea();
			bPendingInitialDrawArea = false;
		}
	}
	
	UpdateFloorPlanToolButtonState();
}

void UEditModeLayoutWidget::SetFloorPlanPanelOpen(bool bOpen)
{
	if (bIsFloorPlanPanelOpen == bOpen)
	{
		return;
	}

	bIsFloorPlanPanelOpen = bOpen;

	if (FloorPlan2DWidget)
	{
		FloorPlan2DWidget->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}

	ApplyFloorPlanToolBarState();

	BP_OnFloorPlanPanelOpenChanged(bIsFloorPlanPanelOpen);
	ApplyLayout();
	
	OnFloorPlanPanelOpenChanged.Broadcast(bIsFloorPlanPanelOpen);
}

void UEditModeLayoutWidget::ToggleFloorPlanPanel()
{
	SetFloorPlanPanelOpen(!bIsFloorPlanPanelOpen);
}

void UEditModeLayoutWidget::SetFloorPlanPanelWidth(float NewWidth)
{
	OpenPanelWidth = FMath::Clamp(NewWidth, MinOpenPanelWidth, MaxOpenPanelWidth);

	if (bIsFloorPlanPanelOpen)
	{
		ApplyLayout();
	}
}

void UEditModeLayoutWidget::SetFloorPlan2DWidgetClass(TSubclassOf<UInteReal2DFloorPlanViewportWidget> InWidgetClass)
{
	FloorPlan2DWidgetClass = InWidgetClass;

	if (!FloorPlan2DWidget)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("FloorPlan2DWidget is not bound. Place a widget named 'FloorPlan2DWidget' in WBP_EditModeLayoutWidget.")
		);
	}
}

void UEditModeLayoutWidget::HandleToggleButtonClicked()
{
	ToggleFloorPlanPanel();
}

float UEditModeLayoutWidget::GetTargetPanelWidth() const
{
	return bIsFloorPlanPanelOpen
		? FMath::Clamp(OpenPanelWidth, MinOpenPanelWidth, MaxOpenPanelWidth)
		: CollapsedPanelWidth;
}

void UEditModeLayoutWidget::ApplyLayout()
{
	if (FloorPlanPanelSizeBox)
	{
		FloorPlanPanelSizeBox->SetWidthOverride(CurrentFloorPlanPanelWidth);
	}

	ApplyFloorPlanToolBarState();

	BP_OnFloorPlanPanelWidthChanged(CurrentFloorPlanPanelWidth);
	ApplyFloorPlanDrawArea();
}

void UEditModeLayoutWidget::ApplyFloorPlanDrawArea()
{
	if (!bAutoFitFloorPlanDrawArea || !FloorPlan2DWidget)
	{
		return;
	}

	const FVector2D FloorPlanLocalSize = FloorPlan2DWidget->GetCachedGeometry().GetLocalSize();

	if (FloorPlanLocalSize.X <= 1.0f || FloorPlanLocalSize.Y <= 1.0f)
	{
		return;
	}

	FloorPlan2DWidget->SetDrawArea(
		FVector2D::ZeroVector,
		FloorPlanLocalSize
	);
}

void UEditModeLayoutWidget::HandleSelectToolButtonClicked()
{
	if (FloorPlan2DWidget)
	{
		FloorPlan2DWidget->SetSelectToolActive2D(true);
		UpdateFloorPlanToolButtonState();
	}
}

void UEditModeLayoutWidget::HandleObjectSnapButtonClicked()
{
	if (FloorPlan2DWidget)
	{
		FloorPlan2DWidget->ToggleObjectSnap2D();
		UpdateFloorPlanToolButtonState();
	}
}

void UEditModeLayoutWidget::ApplyFloorPlanToolBarState()
{
	if (Border_FloorPlanToolBar)
	{
		Border_FloorPlanToolBar->SetVisibility(bIsFloorPlanPanelOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UEditModeLayoutWidget::ApplyToolButtonVisualState(UButton* Button, UImage* Image, bool bActive)
{
	if (Button)
	{
		FButtonStyle ButtonStyle = Button->GetStyle();
		const FLinearColor Tint = bActive ? ToolButtonActiveTint : ToolButtonInactiveTint;

		ButtonStyle.Normal.TintColor = FSlateColor(Tint);
		ButtonStyle.Hovered.TintColor = FSlateColor(Tint);
		ButtonStyle.Pressed.TintColor = FSlateColor(Tint);
		Button->SetStyle(ButtonStyle);
	}

	if (Image)
	{
		Image->SetColorAndOpacity(bActive ? ToolIconActiveTint : ToolIconInactiveTint);
	}
}

void UEditModeLayoutWidget::UpdateFloorPlanToolButtonState()
{
	if (!FloorPlan2DWidget)
	{
		ApplyToolButtonVisualState(Button_SelectTool2D, Image_SelectTool2D, false);
		ApplyToolButtonVisualState(Button_ObjectSnap2D, Image_ObjectSnap2D, false);
		return;
	}

	const bool bFurnitureSelected = FloorPlan2DWidget->HasSelectedFurniture2D();
	const bool bObjectSnapEnabled = FloorPlan2DWidget->IsObjectSnapEnabled2D();

	ApplyToolButtonVisualState(Button_SelectTool2D, Image_SelectTool2D, bFurnitureSelected);
	ApplyToolButtonVisualState(Button_ObjectSnap2D, Image_ObjectSnap2D, bObjectSnapEnabled);
}