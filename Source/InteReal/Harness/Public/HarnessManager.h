#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HarnessManager.generated.h"

class UHarnessGeneratorComponent;
class UHarnessMinimapCaptureComponent;
class UHarnessCaptureMinimapWidget;
class UHarnessSaveManagerComponent;
class UHarnessPipelineManager;

UCLASS()
class INTEREAL_API AHarnessManager : public AActor
{
    GENERATED_BODY()

public:
    AHarnessManager();

protected:
    virtual void BeginPlay() override;

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Harness")
    TObjectPtr<USceneComponent> SceneRoot;

    // 제너레이터 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Harness")
    TObjectPtr<UHarnessGeneratorComponent> HarnessComponent;

    // 세이브 매니저 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Harness")
    TObjectPtr<UHarnessSaveManagerComponent> SaveManagerComponent;

    // 캡처 컴포넌트 추가
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Harness")
    TObjectPtr<UHarnessMinimapCaptureComponent> CaptureComponent;

    // 블루프린트 위젯(WBP_CaptureMinimap) 클래스를 할당할 변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness|UI")
    TSubclassOf<UHarnessCaptureMinimapWidget> MinimapWidgetClass;

    // 메인 HUD 위젯 클래스 (도면 목록 UI)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness|UI")
    TSubclassOf<class UHarnessMainHUD> MainHUDClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness")
    bool bBuildOnBeginPlay = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness")
    int32 DebugPlanId = 1;

private:
    // 생성된 위젯 인스턴스 보관용 포인터
    UPROPERTY(Transient)
    TObjectPtr<UHarnessCaptureMinimapWidget> MinimapWidget;

    // 머티리얼 적용 대기 후 캡처를 위한 타이머
    FTimerHandle CaptureTimerHandle;
    
    UFUNCTION()
    void DelayedCapture();
};
