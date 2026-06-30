#include "EditModeToolbarWidget.h"
#include "ToggleButtonWidget.h"
#include "InteReal/Master/InteRealPlayerController.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "Components/Widget.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"

namespace
{
	UInteriorPlacementSubsystem* GetPlacementSubsystemFromWorld(const UWorld* World)
	{
		return World ? World->GetSubsystem<UInteriorPlacementSubsystem>() : nullptr;
	}
}

void UEditModeToolbarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Grid)
	{
		Btn_Grid->OnToggled.AddDynamic(this, &UEditModeToolbarWidget::HandleGridClicked);
	}
	if (Btn_LightIcons)
	{
		Btn_LightIcons->OnToggled.AddDynamic(this, &UEditModeToolbarWidget::HandleLightIconsClicked);
	}
	if (Btn_PlacementMode)
	{
		Btn_PlacementMode->OnToggled.AddDynamic(this, &UEditModeToolbarWidget::HandlePlacementModeClicked);
	}
	if (Btn_MoveGizmo)
	{
		Btn_MoveGizmo->OnToggled.AddDynamic(this, &UEditModeToolbarWidget::HandleMoveClicked);
	}
	if (Btn_RotateGizmo)
	{
		Btn_RotateGizmo->OnToggled.AddDynamic(this, &UEditModeToolbarWidget::HandleRotateClicked);
	}
	if (Btn_Collapse)
	{
		Btn_Collapse->OnToggled.AddDynamic(this, &UEditModeToolbarWidget::HandleCollapseClicked);
		if (Icon_CollapseRight)
		{
			Btn_Collapse->SetIconAndLabel(Icon_CollapseRight, FText::GetEmpty());
		}
	}

	if (B_ToolbarBox)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(B_ToolbarBox->Slot))
		{
			CanvasSlot->SetSize(ExpandedBoxSize);
			FVector2D NewPos = CanvasSlot->GetPosition();
			NewPos.X = ExpandedOffsetX;
			CanvasSlot->SetPosition(NewPos);
		}
	}

	bCachedInitialized = false;
	RefreshAll();
}

void UEditModeToolbarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAll();
}

void UEditModeToolbarWidget::InitializeForPlayer(AInteRealPlayerController* InPlayerController)
{
	PlayerController = InPlayerController;
	bCachedInitialized = false;
	RefreshAll();
}

void UEditModeToolbarWidget::HandleGridClicked()
{
	if (AInteRealPlayerController* PC = PlayerController.Get())
	{
		PC->SetGridVisible(!PC->IsGridVisible());
	}
	RefreshAll();
}

void UEditModeToolbarWidget::HandleLightIconsClicked()
{
	if (UInteriorPlacementSubsystem* Subsystem = GetPlacementSubsystemFromWorld(GetWorld()))
	{
		Subsystem->ToggleLightFixtureIconsVisible();
	}
	RefreshAll();
}

void UEditModeToolbarWidget::HandlePlacementModeClicked()
{
	if (UInteriorPlacementSubsystem* Subsystem = GetPlacementSubsystemFromWorld(GetWorld()))
	{
		Subsystem->ToggleFreePlacementMode();
	}
	RefreshAll();
}

void UEditModeToolbarWidget::HandleMoveClicked()
{
	if (AInteRealPlayerController* PC = PlayerController.Get())
	{
		PC->SetGizmoShowMove(!PC->IsGizmoShowingMove());
	}
	RefreshAll();
}

void UEditModeToolbarWidget::HandleRotateClicked()
{
	if (AInteRealPlayerController* PC = PlayerController.Get())
	{
		PC->SetGizmoShowRotate(!PC->IsGizmoShowingRotate());
	}
	RefreshAll();
}

void UEditModeToolbarWidget::HandleCollapseClicked()
{
	bIsCollapsed = !bIsCollapsed;

	if (Box_Buttons)
	{
		Box_Buttons->SetVisibility(bIsCollapsed ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (Btn_Collapse)
	{
		// 접힌 상태면 "눌러서 펼치기"(왼쪽), 펼쳐진 상태면 "눌러서 접기"(오른쪽) 아이콘으로 교체
		UTexture2D* ArrowTexture = bIsCollapsed ? Icon_CollapseLeft : Icon_CollapseRight;
		if (ArrowTexture)
		{
			Btn_Collapse->SetIconAndLabel(ArrowTexture, FText::GetEmpty());
		}
	}

	if (B_ToolbarBox)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(B_ToolbarBox->Slot))
		{
			CanvasSlot->SetSize(bIsCollapsed ? CollapsedBoxSize : ExpandedBoxSize);
			FVector2D NewPos = CanvasSlot->GetPosition();
			NewPos.X = bIsCollapsed ? CollapsedOffsetX : ExpandedOffsetX;
			CanvasSlot->SetPosition(NewPos);
		}
	}
}

void UEditModeToolbarWidget::RefreshAll()
{
	UInteriorPlacementSubsystem* Subsystem = GetPlacementSubsystemFromWorld(GetWorld());
	AInteRealPlayerController* PC = PlayerController.Get();

	const bool bShowGrid = PC ? PC->IsGridVisible() : bCachedShowGrid;
	const bool bShowLightIcons = Subsystem ? Subsystem->IsLightFixtureIconsVisible() : bCachedShowLightIcons;
	const bool bFreePlacement = Subsystem ? Subsystem->IsFreePlacementMode() : bCachedFreePlacement;
	const bool bShowMove = PC ? PC->IsGizmoShowingMove() : bCachedShowMove;
	const bool bShowRotate = PC ? PC->IsGizmoShowingRotate() : bCachedShowRotate;

	if (bCachedInitialized &&
		bShowGrid == bCachedShowGrid &&
		bShowLightIcons == bCachedShowLightIcons &&
		bFreePlacement == bCachedFreePlacement &&
		bShowMove == bCachedShowMove &&
		bShowRotate == bCachedShowRotate)
	{
		return;
	}

	if (Btn_Grid)
	{
		Btn_Grid->SetActiveVisual(bShowGrid);
	}

	if (Btn_LightIcons)
	{
		Btn_LightIcons->SetActiveVisual(bShowLightIcons);
	}

	if (Btn_PlacementMode)
	{
		Btn_PlacementMode->SetActiveVisual(bFreePlacement);
	}

	if (Btn_MoveGizmo)
	{
		Btn_MoveGizmo->SetActiveVisual(bShowMove);
	}

	if (Btn_RotateGizmo)
	{
		Btn_RotateGizmo->SetActiveVisual(bShowRotate);
	}

	bCachedInitialized = true;
	bCachedShowGrid = bShowGrid;
	bCachedShowLightIcons = bShowLightIcons;
	bCachedFreePlacement = bFreePlacement;
	bCachedShowMove = bShowMove;
	bCachedShowRotate = bShowRotate;
}
