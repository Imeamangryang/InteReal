#include "InteReal/Harness/Public/HarnessManager.h"
#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"
#include "InteReal/Harness/Public/HarnessMinimapCaptureComponent.h"
#include "InteReal/Harness/Public/HarnessSaveManagerComponent.h"
#include "InteReal/Harness/Public/HarnessPipelineManager.h"
#include "InteReal/EditMode/Managers/InteriorPlacementManager.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "InteReal/ViewMode/ViewModeManager.h"
#include "InteReal/Master/InteRealPlayerController.h"
#include "InteReal/Master/InteRealHUD.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

AHarnessManager::AHarnessManager()
{
    PrimaryActorTick.bCanEverTick = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;
    HarnessComponent = CreateDefaultSubobject<UHarnessGeneratorComponent>(TEXT("HarnessGenerator"));
    SaveManagerComponent = CreateDefaultSubobject<UHarnessSaveManagerComponent>(TEXT("SaveManagerComponent"));
    CaptureComponent = CreateDefaultSubobject<UHarnessMinimapCaptureComponent>(TEXT("MinimapCapture"));
    CaptureComponent->SetupAttachment(RootComponent);
}

void AHarnessManager::BeginPlay()
{
    Super::BeginPlay();

    if (auto* Pipeline = GetWorld()->GetSubsystem<UHarnessPipelineManager>())
    {
        Pipeline->InitializePipeline(SaveManagerComponent, HarnessComponent);
        Pipeline->OnPipelineLoadFinished.AddDynamic(this, &AHarnessManager::DelayedCapture);
        
        // 추가: HUD에 파이프라인 바인딩 (2D 도면 드로잉용)
        if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        {
            if (AInteRealHUD* HUD = Cast<AInteRealHUD>(PC->GetHUD()))
            {
                HUD->BindHarnessPipeline(Pipeline);
            }
        }
    }
}

void AHarnessManager::DelayedCapture()
{
    UE_LOG(LogTemp, Log, TEXT("[Harness] AHarnessManager: 파이프라인 완료. 카메라 포커싱 및 캡처 시작."));

    if (!HarnessComponent) return;

	AInteRealPlayerController* ViewPC = Cast<AInteRealPlayerController>(GetWorld()->GetFirstPlayerController());

	// 1. Legacy maps do not use the placement subsystem.
	const FHarnessFloorData& FloorData = HarnessComponent->GetCachedFloorData();
	if (!ViewPC)
	{
		for (TActorIterator<AInteriorPlacementManager> It(GetWorld()); It; ++It)
		{
			(*It)->InitializeFromFloorData(FloorData, (*It)->GridCellSize);
		}
	}
	else if (UInteriorPlacementSubsystem* PlacementSubsystem = GetWorld()->GetSubsystem<UInteriorPlacementSubsystem>())
	{
		if (!PlacementSubsystem->GetGrid())
		{
			PlacementSubsystem->InitializeFromFloorData(
				FloorData, PlacementSubsystem->GetGridCellSize());
		}
	}

    // 2. 뷰모드 초기화 (ISO 뷰로 강제 고정)
    EHarnessViewMode ModeToApply = EHarnessViewMode::Isometric;
    for (TActorIterator<AViewModeManager> It(GetWorld()); It; ++It)
    {
        (*It)->SetViewMode(ModeToApply);
        (*It)->FocusOnBuilding();
        (*It)->SnapToTarget();
    }

	if (ViewPC) ViewPC->SetViewMode(ModeToApply);

    // 3. 미니맵 캡처
    if (CaptureComponent)
    {
        FVector2D Min, Max;
        HarnessComponent->GetFloorBounds(Min, Max);
        CaptureComponent->AdjustToBoundingBox(Min, Max);
        
        UTextureRenderTarget2D* MinimapRT = CaptureComponent->GetOrCreateRenderTarget();
        if (ViewPC) ViewPC->SetupMinimapHUD(CaptureComponent, MinimapRT, MinimapWidgetClass);

        TWeakObjectPtr<AInteRealPlayerController> WeakPC = ViewPC;
        GetWorld()->GetTimerManager().SetTimer(CaptureTimerHandle, FTimerDelegate::CreateLambda([this, WeakPC]() {
            if (CaptureComponent) {
                CaptureComponent->UpdateMinimap();
                if (WeakPC.IsValid()) WeakPC->ShowMinimap();
            }
        }), 1.0f, false);
    }
}
