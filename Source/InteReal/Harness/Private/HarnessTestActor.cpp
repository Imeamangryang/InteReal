#include "InteReal/Harness/Public/HarnessTestActor.h"

#include "Misc/Paths.h"
#include "Blueprint/UserWidget.h"
#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"
#include "InteReal/Harness/Public/HarnessMinimapCaptureComponent.h"
#include "InteReal/Harness/Public/HarnessCaptureMinimapWidget.h"
#include "InteReal/Harness/Public/HarnessJsonParser.h"
#include "Kismet/GameplayStatics.h"

AHarnessTestActor::AHarnessTestActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    // 도면 생성 컴포넌트
    HarnessComponent = CreateDefaultSubobject<UHarnessGeneratorComponent>(TEXT("HarnessGenerator"));

    // 캡처 컴포넌트 생성 및 루트에 부착
    CaptureComponent = CreateDefaultSubobject<UHarnessMinimapCaptureComponent>(TEXT("MinimapCapture"));
    CaptureComponent->SetupAttachment(RootComponent);

    JsonFilePath = FPaths::ProjectDir() / TEXT("floor.json");
}

void AHarnessTestActor::BeginPlay()
{
    Super::BeginPlay();

    if (!bBuildOnBeginPlay || !HarnessComponent || !CaptureComponent)
    {
        return;
    }

    FHarnessFloorData FloorData;
    FString ErrorMessage;
    
    if (FHarnessJsonParser::LoadFloorDataFromJsonFile(JsonFilePath, FloorData, ErrorMessage))
    {
        // 1. 도면 생성 (Generator)
        HarnessComponent->BuildHarness(FloorData);
        
        // 2. 바운딩 박스 계산 및 카메라 세팅 (Generator -> Capture)
        FVector2D MinBounds, MaxBounds;
        HarnessComponent->GetFloorBounds(MinBounds, MaxBounds);
        CaptureComponent->AdjustToBoundingBox(MinBounds, MaxBounds);
        
        // 3. 렌더 타겟 생성 (Capture)
        UTextureRenderTarget2D* MinimapRT = CaptureComponent->GetOrCreateRenderTarget();

        // 4. 위젯 생성 및 데이터 주입, 뷰포트 출력 (UI)
        if (MinimapWidgetClass)
        {
            MinimapWidget = CreateWidget<UHarnessCaptureMinimapWidget>(GetWorld(), MinimapWidgetClass);
            if (MinimapWidget)
            {
                MinimapWidget->InjectMinimapData(CaptureComponent, MinimapRT);
                MinimapWidget->AddToViewport();

                APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
                if (PC)
                {
                    PC->SetShowMouseCursor(true);
                    FInputModeGameAndUI InputMode;
                    InputMode.SetWidgetToFocus(MinimapWidget->TakeWidget());
                    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
                    PC->SetInputMode(InputMode);
                }

                // 매 프레임 캡처를 비활성화하여 렌더링 성능 확보
                CaptureComponent->bCaptureEveryFrame = false;
                CaptureComponent->bCaptureOnMovement = false;

                // 동적 메쉬에 머티리얼이 완전히 씌워질 시간을 확보한 뒤 수동으로 단일 캡처 실행
                GetWorld()->GetTimerManager().SetTimer(CaptureTimerHandle, this, &AHarnessTestActor::DelayedCapture, 1.0f, false);
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[HarnessTestActor] MinimapWidgetClass is NULL. Please assign the Widget Blueprint in the details panel."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[HarnessTestActor] JSON Load Failed! Reason: %s"), *ErrorMessage);
    }
}

void AHarnessTestActor::DelayedCapture()
{
    if (CaptureComponent)
    {
        // 렌더 타겟에 현재 화면을 한 번만 업데이트(사진 찍기)
        CaptureComponent->CaptureScene();
        UE_LOG(LogTemp, Log, TEXT("[Harness] 미니맵 머티리얼 지연 캡처 완료!"));
    }
}