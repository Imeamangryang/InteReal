#include "InteRealHUD.h"

#include "Blueprint/UserWidget.h"
#include "Components/Overlay.h"
#include "InteReal/EditMode/2D/InteReal2DFloorPlanViewportWidget.h"
#include "InteReal/Master/UI/SubWidgets/EditModeLayoutWidget.h"
#include "InteReal/Master/UI/SubWidgets/TopBarWidget.h"
#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"
#include "InteReal/Harness/Public/HarnessPipelineManager.h"
#include "InteReal/Harness/Public/HarnessData.h"
#include "UI/ViewModeWidgets/EnvironmentPanel.h"
#include "UI/ViewModeWidgets/InteRealMinimap.h"
#include "InteReal/EditMode/Furniture/LightFixture.h"

namespace
{
	constexpr int32 ViewLayerZOrder = 0;
	constexpr int32 EditLayerZOrder = 5;
	constexpr int32 ToolOverlayZOrder = 10;
	constexpr int32 TopBarZOrder = 20;
	const TCHAR* DefaultTopBarWidgetPath = TEXT("/Game/UI/Widgets/WBP_TopBar.WBP_TopBar_C");
}

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
	if (!TopBarWidgetClass)
	{
		TopBarWidgetClass = LoadClass<UTopBarWidget>(nullptr, DefaultTopBarWidgetPath);
	}

	if (TopBarWidgetClass)
	{
		TopBarWidgetInstance = CreateWidget<UTopBarWidget>(GetOwningPlayerController(), TopBarWidgetClass);
		if (TopBarWidgetInstance)
		{
			TopBarWidgetInstance->AddToViewport(TopBarZOrder);
		}
	}

	if (PlacementTabWidget)
	{
		PlacementTabInstance = CreateWidget<UUserWidget>(GetOwningPlayerController(), PlacementTabWidget);
		if (PlacementTabInstance)
		{
			PlacementTabInstance->AddToViewport(EditLayerZOrder);
		}
	}

	if (TooltipWidgetClass)
	{
		TooltipInstance = CreateWidget<UPlacementTooltipWidget>(GetOwningPlayerController(), TooltipWidgetClass);
		if (TooltipInstance)
		{
			TooltipInstance->AddToViewport(ToolOverlayZOrder);
			TooltipInstance->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (RotationGuideWidgetClass)
	{
		RotationGuideInstance = CreateWidget<URotationGuideWidget>(GetOwningPlayerController(), RotationGuideWidgetClass);
		if (RotationGuideInstance)
		{
			RotationGuideInstance->AddToViewport(ToolOverlayZOrder);
			RotationGuideInstance->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (UserGuideWidgetClass)
	{
		UserGuideInstance = CreateWidget<UUserGuideWidget>(GetOwningPlayerController(), UserGuideWidgetClass);
		if (UserGuideInstance)
		{
			UserGuideInstance->AddToViewport(ToolOverlayZOrder);
			UserGuideInstance->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (EditModeToolbarWidgetClass)
	{
		EditModeToolbarInstance = CreateWidget<UEditModeToolbarWidget>(GetOwningPlayerController(), EditModeToolbarWidgetClass);
		if (EditModeToolbarInstance)
		{
			EditModeToolbarInstance->AddToViewport(ToolOverlayZOrder);
			EditModeToolbarInstance->SetVisibility(ESlateVisibility::Hidden);

			if (AInteRealPlayerController* PC = Cast<AInteRealPlayerController>(GetOwningPlayerController()))
			{
				EditModeToolbarInstance->InitializeForPlayer(PC);
			}
		}
	}

	if (EditModeLayoutWidgetClass)
	{
		EditModeLayoutWidgetInstance = CreateWidget<UEditModeLayoutWidget>(
			GetOwningPlayerController(),
			EditModeLayoutWidgetClass
		);

		if (EditModeLayoutWidgetInstance)
		{
			EditModeLayoutWidgetInstance->SetFloorPlan2DWidgetClass(FloorPlan2DWidgetClass);
			EditModeLayoutWidgetInstance->AddToViewport(EditLayerZOrder);
			EditModeLayoutWidgetInstance->SetVisibility(ESlateVisibility::Hidden);

			if (UWorld* World = GetWorld())
			{
				if (UHarnessPipelineManager* Pipeline = World->GetSubsystem<UHarnessPipelineManager>())
				{
					if (UHarnessGeneratorComponent* GeneratorComp = Pipeline->GetGeneratorComp())
					{
						const FHarnessFloorData& CachedFloorData = GeneratorComp->GetCachedFloorData();
						if (CachedFloorData.vertices.Num() > 0 || CachedFloorData.faces.Num() > 0)
						{
							LoadFloorPlan2DFromHarnessData(CachedFloorData);
						}
					}
				}
			}
		}
	}


	if (EnvironmentPanelClass)
	{
		EnvironmentPanelInstance = CreateWidget<UEnvironmentPanel>(GetOwningPlayerController(), EnvironmentPanelClass);
		if (EnvironmentPanelInstance)
		{
			EnvironmentPanelInstance->AddToViewport(ViewLayerZOrder);
		}
	}

	if (FurnitureSizePanelWidgetClass)
	{
		FurnitureSizePanelInstance = CreateWidget<UFurnitureSizePanelWidget>(GetOwningPlayerController(), FurnitureSizePanelWidgetClass);
		if (FurnitureSizePanelInstance)
		{
			FurnitureSizePanelInstance->AddToViewport(ToolOverlayZOrder);
			FurnitureSizePanelInstance->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (LightAttributesPanelWidgetClass)
	{
		LightAttributesPanelInstance = CreateWidget<ULightAttributesPanelWidget>(GetOwningPlayerController(), LightAttributesPanelWidgetClass);
		if (LightAttributesPanelInstance)
		{
			LightAttributesPanelInstance->AddToViewport(ToolOverlayZOrder);
			LightAttributesPanelInstance->SetVisibility(ESlateVisibility::Hidden);
		}
	}
	
	if (MaterialAttributesPanelWidgetClass)
	{
		MaterialAttributesPanelInstance = CreateWidget<UMaterialAttributesPanelWidget>(GetOwningPlayerController(), MaterialAttributesPanelWidgetClass);
		if (MaterialAttributesPanelInstance)
		{
			MaterialAttributesPanelInstance->AddToViewport(ToolOverlayZOrder);
			MaterialAttributesPanelInstance->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void AInteRealHUD::ShowFurnitureSizePanel(AFurniture* Furniture)
{
	if (!FurnitureSizePanelInstance)
	{
		return;
	}

	if (Furniture)
	{
		FurnitureSizePanelInstance->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		FurnitureSizePanelInstance->RefreshForFurniture(Furniture);
	}
	else
	{
		FurnitureSizePanelInstance->SetVisibility(ESlateVisibility::Hidden);
	}
}

void AInteRealHUD::ShowLightAttributesPanel(AFurniture* Furniture)
{
	if (!LightAttributesPanelInstance)
	{
		return;
	}

	// RefreshForFurniture는 ALightFixture가 아니면 조용히 아무것도 안 하므로,
	// 여기서 미리 걸러내지 않으면 일반 가구를 선택했을 때 패널이 비어있는 채로 보이게 된다.
	if (ALightFixture* LightFixture = Cast<ALightFixture>(Furniture))
	{
		LightAttributesPanelInstance->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		LightAttributesPanelInstance->RefreshForFurniture(LightFixture);
	}
	else
	{
		LightAttributesPanelInstance->SetVisibility(ESlateVisibility::Hidden);
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

	if (EditModeLayoutWidgetInstance)
	{
		EditModeLayoutWidgetInstance->SetVisibility(EditVisibility);
	}

	if (EditModeToolbarInstance)
	{
		EditModeToolbarInstance->SetVisibility(EditVisibility);
	}

	if (EnvironmentPanelInstance)
	{
		EnvironmentPanelInstance->SetVisibility(ViewVisibility);
	}

	if (MaterialAttributesPanelInstance && !bIsEdit)
	{
		MaterialAttributesPanelInstance->SetVisibility(EditVisibility);
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
		MinimapWidgetInstance->AddToViewport(ViewLayerZOrder);
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

void AInteRealHUD::UpdateRotationGuide(bool bVisible, float DeltaAngle, const FVector2D& AnchorScreenPosition)
{
	if (!RotationGuideInstance) return;

	if (!bVisible)
	{
		RotationGuideInstance->HideGuide();
		return;
	}

	FVector2D ViewportSize = FVector2D::ZeroVector;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	const FVector2D GuideSize = RotationGuideInstance->GuideScreenSize;
	FVector2D Pos = AnchorScreenPosition + RotationGuideInstance->GizmoScreenOffset;
	if (ViewportSize.X > 0.f) Pos.X = FMath::Clamp(Pos.X, 0.f, FMath::Max(0.f, ViewportSize.X - GuideSize.X));
	if (ViewportSize.Y > 0.f) Pos.Y = FMath::Clamp(Pos.Y, 0.f, FMath::Max(0.f, ViewportSize.Y - GuideSize.Y));

	RotationGuideInstance->UpdateRotation(DeltaAngle);
	RotationGuideInstance->SetAlignmentInViewport(FVector2D::ZeroVector);
	RotationGuideInstance->SetDesiredSizeInViewport(GuideSize);
	RotationGuideInstance->SetPositionInViewport(Pos, true);
	RotationGuideInstance->ShowGuide();
}

void AInteRealHUD::ShowRotationGuideForInput(float InitialYawDegrees, const FVector2D& GizmoCenterScreenPos)
{
	if (!RotationGuideInstance) return;

	// 기존 UpdateRotationGuide와 동일하게 bRemoveDPIScale=true 사용
	// WidgetRadius는 WBP에서 실제 위젯 반경(px)에 맞게 설정
	const float Radius = RotationGuideInstance->WidgetRadius;
	const FVector2D TopLeft = GizmoCenterScreenPos - FVector2D(Radius, Radius);
	RotationGuideInstance->SetAlignmentInViewport(FVector2D::ZeroVector);
	RotationGuideInstance->SetDesiredSizeInViewport(FVector2D(Radius * 2.0f));
	RotationGuideInstance->SetPositionInViewport(TopLeft, true);
	RotationGuideInstance->ShowForRotation(InitialYawDegrees);
}

void AInteRealHUD::UpdateRotationGuideForInput(float DeltaAngle)
{
	if (RotationGuideInstance)
	{
		RotationGuideInstance->UpdateRotation(DeltaAngle);
	}
}

void AInteRealHUD::ShowEditModeToolbar(bool bVisible)
{
	if (EditModeToolbarInstance)
	{
		EditModeToolbarInstance->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
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
	if (!EditModeLayoutWidgetInstance)
	{
		return;
	}

	EditModeLayoutWidgetInstance->SetVisibility(
		bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden
	);

	EditModeLayoutWidgetInstance->SetFloorPlanPanelOpen(bVisible);
}

void AInteRealHUD::LoadFloorPlan2DFromHarnessData(const FHarnessFloorData& InFloorData)
{
	if (UInteReal2DFloorPlanViewportWidget* FloorPlan2DWidget = GetFloorPlan2DWidget())
	{
		FloorPlan2DWidget->LoadFromHarnessFloorData(InFloorData);
	}
}

void AInteRealHUD::BindHarnessPipeline(UHarnessPipelineManager* InPipelineManager)
{
	if (!InPipelineManager)
	{
		return;
	}

	InPipelineManager->OnFloorPlanDataReady.AddDynamic(this, &AInteRealHUD::OnFloorPlanDataReady);
	
	if (UHarnessGeneratorComponent* GeneratorComp = InPipelineManager->GetGeneratorComp())
	{
		const FHarnessFloorData& CachedFloorData = GeneratorComp->GetCachedFloorData();
		if (CachedFloorData.vertices.Num() > 0 || CachedFloorData.faces.Num() > 0)
		{
			LoadFloorPlan2DFromHarnessData(CachedFloorData);
		}
	}
}

void AInteRealHUD::OnFloorPlanDataReady(const FHarnessFloorData& InFloorData)
{
	LoadFloorPlan2DFromHarnessData(InFloorData);
}


UInteReal2DFloorPlanViewportWidget* AInteRealHUD::GetFloorPlan2DWidget() const
{
	return EditModeLayoutWidgetInstance
		? EditModeLayoutWidgetInstance->GetFloorPlan2DWidget()
		: nullptr;
}

void AInteRealHUD::ShowMaterialAttributesPanel(bool bVisible)
{
	if (!MaterialAttributesPanelInstance)
	{
		return;
	}

	MaterialAttributesPanelInstance->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
}

void AInteRealHUD::RefreshMaterialAttributesPanel(const FMaterialDataRow& MaterialData)
{
	if (!MaterialAttributesPanelInstance)
	{
		return;
	}

	MaterialAttributesPanelInstance->RefreshForMaterial(MaterialData);
	MaterialAttributesPanelInstance->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void AInteRealHUD::ResetMaterialAttributesPanel()
{
	if (!MaterialAttributesPanelInstance)
	{
		return;
	}

	MaterialAttributesPanelInstance->ResetForSurfaceWithoutMaterial(); 
	MaterialAttributesPanelInstance->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}