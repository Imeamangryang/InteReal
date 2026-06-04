#include "InteReal/Harness/Public/HarnessTestActor.h"

#include "Misc/Paths.h"
#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"
#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"
#include "InteReal/Harness/Public/HarnessMinimapCaptureComponent.h"
#include "InteReal/Harness/Public/HarnessMainHUD.h"
#include "InteReal/Harness/Public/HarnessJsonParser.h"
#include "InteReal/Harness/Public/HarnessNetworkComponent.h"
#include "InteReal/Harness/Public/HarnessSaveManagerComponent.h"
#include "InteReal/Harness/Public/HarnessPipelineManager.h"
#include "InteReal/EditMode/Managers/InteriorPlacementManager.h"
#include "InteReal/ViewMode/ViewModeManager.h"
#include "InteReal/Master/InteRealPlayerController.h"
#include "Kismet/GameplayStatics.h"

AHarnessTestActor::AHarnessTestActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    // 도면 생성 컴포넌트
    HarnessComponent = CreateDefaultSubobject<UHarnessGeneratorComponent>(TEXT("HarnessGenerator"));

    // 네트워크, 세이브, 파이프라인 컴포넌트 추가
    NetworkComponent = CreateDefaultSubobject<UHarnessNetworkComponent>(TEXT("NetworkComponent"));
    SaveManagerComponent = CreateDefaultSubobject<UHarnessSaveManagerComponent>(TEXT("SaveManagerComponent"));
    PipelineManager = CreateDefaultSubobject<UHarnessPipelineManager>(TEXT("PipelineManager"));

    // 캡처 컴포넌트 생성 및 루트에 부착
    CaptureComponent = CreateDefaultSubobject<UHarnessMinimapCaptureComponent>(TEXT("MinimapCapture"));
    CaptureComponent->SetupAttachment(RootComponent);

    // 기본적으로 자동 로드는 꺼둡니다 (UI에서 선택)
    JsonFilePath = TEXT("test1"); 
}

void AHarnessTestActor::BeginPlay()
{
    Super::BeginPlay();

    if (PipelineManager && NetworkComponent && SaveManagerComponent && HarnessComponent)
    {
        // 파이프라인 초기화
        PipelineManager->InitializePipeline(NetworkComponent, SaveManagerComponent, HarnessComponent);
        
        // 파이프라인 완료 시 후속 작업(그리드, 미니맵) 수행을 위해 바인딩
        PipelineManager->OnPipelineLoadFinished.AddDynamic(this, &AHarnessTestActor::DelayedCapture); 
    }

    // 메인 HUD(도면 목록 리스트) 생성 및 표시
    if (MainHUDClass)
    {
        UHarnessMainHUD* MainHUD = CreateWidget<UHarnessMainHUD>(GetWorld(), MainHUDClass);
        if (MainHUD)
        {
            MainHUD->SetupHUD(NetworkComponent, PipelineManager);
            MainHUD->AddToViewport();

            APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
            if (PC)
            {
                PC->SetShowMouseCursor(true);
                FInputModeGameAndUI InputMode;
                InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
                PC->SetInputMode(InputMode);
            }
        }
    }

    // 만약 디버그용으로 즉시 빌드가 필요하다면 실행 (기본 false)
    if (bBuildOnBeginPlay && PipelineManager)
    {
        PipelineManager->LoadProject(JsonFilePath);
    }
}

void AHarnessTestActor::DelayedCapture()
{
    UE_LOG(LogTemp, Log, TEXT("[Harness] AHarnessTestActor::DelayedCapture() 시작"));

    if (!HarnessComponent) return;

    // 1. 그리드 매니저 초기화
    const FHarnessFloorData& FloorData = HarnessComponent->GetCachedFloorData();
    for (TActorIterator<AInteriorPlacementManager> It(GetWorld()); It; ++It)
    {
        (*It)->InitializeFromFloorData(FloorData, (*It)->GridCellSize);
        (*It)->SetGridVisible(true);
    }

    // 💡 [수정] 2. 뷰모드를 무조건 TopDown(평면 뷰)으로 강제 초기화
    EHarnessViewMode ModeToApply = EHarnessViewMode::TopDown;

    for (TActorIterator<AViewModeManager> It(GetWorld()); It; ++It)
    {
        (*It)->SetViewMode(ModeToApply);   // 목표 상태를 평면 뷰로 설정
        (*It)->FocusOnBuilding();          // 도면 크기에 맞춰 줌 거리 및 타겟 계산
        (*It)->SnapToTarget();             // 💡 카메라가 서서히 날아가지 않고 즉시 찰칵! 이동
    }

    // 플레이어 컨트롤러 역시 평면 뷰 상태로 동기화
    AInteRealPlayerController* ViewPC = Cast<AInteRealPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
    if (ViewPC)
    {
        ViewPC->SetViewMode(ModeToApply);
    }

    // 3. 미니맵 캡처 설정
    if (CaptureComponent)
    {
        FVector2D MinBounds, MaxBounds;
        HarnessComponent->GetFloorBounds(MinBounds, MaxBounds);
        
        CaptureComponent->AdjustToBoundingBox(MinBounds, MaxBounds);
        UTextureRenderTarget2D* MinimapRT = CaptureComponent->GetOrCreateRenderTarget();

        if (ViewPC)
        {
            // 위젯을 생성하지만 처음에는 숨겨진(Hidden) 상태로 뷰포트에 올라감
            ViewPC->SetupMinimapHUD(CaptureComponent, MinimapRT, MinimapWidgetClass);
        }

        CaptureComponent->bCaptureEveryFrame = false;
        CaptureComponent->bCaptureOnMovement = false;

        // 💡 [수정] 람다 안에서 안전하게 PC를 참조하기 위해 WeakPtr 사용
        TWeakObjectPtr<AInteRealPlayerController> WeakPC = ViewPC;

        // 실제 렌더링은 머티리얼 적용 대기 후 수행 (1초 대기)
        GetWorld()->GetTimerManager().SetTimer(CaptureTimerHandle, FTimerDelegate::CreateLambda([this, WeakPC]() {
            if (CaptureComponent) 
            {
                CaptureComponent->CaptureScene();
                
                // 💡 [수정] 캡처가 성공적으로 끝난 직후, 숨겨뒀던 미니맵을 짠! 하고 나타나게 함
                if (WeakPC.IsValid())
                {
                    WeakPC->ShowMinimap();
                }
            }
        }), 1.0f, false);
    }
}
