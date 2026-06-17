#include "InteRealHUD.h"

#include "Blueprint/UserWidget.h"
#include "Public/HarnessCaptureMinimapWidget.h"
#include "Components/Overlay.h"
#include "InteReal/EditMode/2D/InteReal2DFloorPlanViewportWidget.h"
#include "InteReal/Harness/Public/HarnessPipelineManager.h"
#include "InteReal/Harness/Public/HarnessData.h"
#include "UI/ViewModeWidgets/EnvironmentPanel.h"
#include "UI/ViewModeWidgets/InteRealMinimap.h"

void AInteRealHUD::BeginPlay()
{
	Super::BeginPlay();
	InitializeHUDWidgets();

	if (AInteRealPlayerController* PC = Cast<AInteRealPlayerController>(GetOwningPlayerController()))
	{
		UpdateModeUIVisibility(PC->GetControlMode());
	}
}

void AInteRealHUD::InitializeHUDWidgets()
{
	if (PlacementTabWidget)
	{
		PlacementTabInstance = CreateWidget<UUserWidget>(GetOwningPlayerController(), PlacementTabWidget);
		if (PlacementTabInstance)
		{
			PlacementTabInstance->AddToViewport();
		}
	}

	if (TooltipWidgetClass)
	{
		TooltipInstance = CreateWidget<UPlacementTooltipWidget>(GetOwningPlayerController(), TooltipWidgetClass);
		if (TooltipInstance)
		{
			TooltipInstance->AddToViewport(10);
			TooltipInstance->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (RotationGuideWidgetClass)
	{
		RotationGuideInstance = CreateWidget<URotationGuideWidget>(GetOwningPlayerController(), RotationGuideWidgetClass);
		if (RotationGuideInstance)
		{
			RotationGuideInstance->AddToViewport();
			RotationGuideInstance->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (UserGuideWidgetClass)
	{
		UserGuideInstance = CreateWidget<UUserGuideWidget>(GetOwningPlayerController(), UserGuideWidgetClass);
		if (UserGuideInstance)
		{
			UserGuideInstance->AddToViewport(10);
			UserGuideInstance->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (FloorPlan2DWidgetClass)
	{
		FloorPlan2DWidgetInstance = CreateWidget<UInteReal2DFloorPlanViewportWidget>(
			GetOwningPlayerController(),
			FloorPlan2DWidgetClass
		);

		if (FloorPlan2DWidgetInstance)
		{
			FloorPlan2DWidgetInstance->AddToViewport(5);
			FloorPlan2DWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (EnvironmentPanelClass)
	{
		EnvironmentPanelInstance = CreateWidget<UEnvironmentPanel>(GetOwningPlayerController(), EnvironmentPanelClass);
		if (EnvironmentPanelInstance)
		{
			EnvironmentPanelInstance->AddToViewport(); 
		}
	}
}

void AInteRealHUD::UpdateModeUIVisibility(EInteRealControlMode CurrentMode)
{
	const bool bIsEdit = (CurrentMode == EInteRealControlMode::Edit);
	const ESlateVisibility EditVisibility = bIsEdit ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden;
	const ESlateVisibility ViewVisibility = bIsEdit ? ESlateVisibility::Hidden : ESlateVisibility::SelfHitTestInvisible;

	if (PlacementTabInstance)
	{
		PlacementTabInstance->SetVisibility(EditVisibility);
	}

	if (TooltipInstance)
	{
		TooltipInstance->SetVisibility(ESlateVisibility::Hidden);
	}

	if (RotationGuideInstance)
	{
		RotationGuideInstance->SetVisibility(ESlateVisibility::Hidden);
	}

	if (UserGuideInstance)
	{
		UserGuideInstance->SetVisibility(ESlateVisibility::Hidden);
	}

	if (FloorPlan2DWidgetInstance)
	{
		FloorPlan2DWidgetInstance->SetVisibility(
			bIsEdit ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden
		);
	}

	if (EnvironmentPanelInstance)
	{
		EnvironmentPanelInstance->SetVisibility(ViewVisibility);
	}

	if (MinimapWidgetInstance)
	{
		MinimapWidgetInstance->SetVisibility(
			bIsEdit ? ESlateVisibility::Hidden : ESlateVisibility::SelfHitTestInvisible
		);
	}
}

void AInteRealHUD::UpdatePlacementTooltip(
	bool bIsEditMode,
	bool bHasActivePreview,
	EPlacementInvalidReason InvalidReason,
	const FVector2D& MousePosition
)
{
	if (!bIsEditMode)
	{
		if (TooltipInstance)
		{
			TooltipInstance->SetVisibility(ESlateVisibility::Hidden);
		}
		return;
	}

	if (!TooltipInstance) return;

	if (!bHasActivePreview || InvalidReason == EPlacementInvalidReason::None)
	{
		TooltipInstance->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	TooltipInstance->ShowReason(InvalidReason);
	TooltipInstance->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void AInteRealHUD::SetupMinimapHUD(
	UHarnessMinimapCaptureComponent* InCaptureComp,
	UTextureRenderTarget2D* InRT,
	TSubclassOf<UInteRealMinimap> InWidgetClass,
	EHarnessViewMode CurrentViewMode,
	EInteRealControlMode CurrentControlMode)
{
	if (!InWidgetClass) return;

	if (MinimapWidgetInstance)
	{
		MinimapWidgetInstance->RemoveFromParent();
		MinimapWidgetInstance = nullptr;
	}

	// 💡 [수정] CreateWidget의 타입 파라미터 변경
	MinimapWidgetInstance = CreateWidget<UInteRealMinimap>(GetOwningPlayerController(), InWidgetClass);
	if (MinimapWidgetInstance)
	{
		MinimapWidgetInstance->InjectMinimapData(InCaptureComp, InRT);
		MinimapWidgetInstance->AddToViewport();
		MinimapWidgetInstance->SetVisibility(ESlateVisibility::Hidden);

		UpdateMinimapIconVisibility(CurrentViewMode);
		UpdateModeUIVisibility(CurrentControlMode);
	}
}

void AInteRealHUD::UpdateUserGuide(bool bVisible, EPlacementInvalidReason Reason, const FVector2D& MousePosition)
{
	if (UserGuideInstance)
	{
		if (!bVisible)
		{
			UserGuideInstance->SetVisibility(ESlateVisibility::Hidden);
			return;
		}

		FVector2D ViewportSize = FVector2D::ZeroVector;
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->GetViewportSize(ViewportSize);
		}

		FVector2D Pos = MousePosition + FVector2D(8.f, -8.f);
		if (ViewportSize.X > 0.f) Pos.X = FMath::Clamp(Pos.X, 0.f, ViewportSize.X - 300.f);
		if (ViewportSize.Y > 0.f) Pos.Y = FMath::Clamp(Pos.Y, 0.f, ViewportSize.Y - 150.f);

		UserGuideInstance->SetPositionInViewport(Pos, true);
		UserGuideInstance->UpdateTooltip(Reason);
		UserGuideInstance->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void AInteRealHUD::ShowMinimap(EInteRealControlMode CurrentMode)
{
	if (MinimapWidgetInstance && CurrentMode == EInteRealControlMode::View)
	{
		MinimapWidgetInstance->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void AInteRealHUD::UpdateMinimapIconVisibility(EHarnessViewMode NewMode)
{
	if (MinimapWidgetInstance && MinimapWidgetInstance->PlayerIcon)
	{
		const ESlateVisibility NewVisibility =
			(NewMode == EHarnessViewMode::FirstPerson)
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Hidden;

		MinimapWidgetInstance->PlayerIcon->SetVisibility(NewVisibility);
	}
}

void AInteRealHUD::ShowFloorPlan2D(bool bVisible)
{
	if (!FloorPlan2DWidgetInstance)
	{
		return;
	}

	FloorPlan2DWidgetInstance->SetVisibility(
		bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden
	);
}

void AInteRealHUD::LoadFloorPlan2DFromHarnessData(const FHarnessFloorData& InFloorData)
{
	if (!FloorPlan2DWidgetInstance)
	{
		return;
	}

	FloorPlan2DWidgetInstance->LoadFromHarnessFloorData(InFloorData);
}

void AInteRealHUD::BindHarnessPipeline(UHarnessPipelineManager* InPipelineManager)
{
	if (!InPipelineManager)
	{
		return;
	}

	InPipelineManager->OnFloorPlanDataReady.AddDynamic(this, &AInteRealHUD::OnFloorPlanDataReady);
}

void AInteRealHUD::OnFloorPlanDataReady(const FHarnessFloorData& InFloorData)
{
	LoadFloorPlan2DFromHarnessData(InFloorData);
}