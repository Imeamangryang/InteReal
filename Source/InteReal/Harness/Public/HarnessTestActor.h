#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HarnessTestActor.generated.h"

class UHarnessGeneratorComponent;
class UHarnessMinimapCaptureComponent;
class UHarnessCaptureMinimapWidget;

UCLASS()
class INTEREAL_API AHarnessTestActor : public AActor
{
    GENERATED_BODY()

public:
    AHarnessTestActor();

protected:
    virtual void BeginPlay() override;

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Harness")
    TObjectPtr<USceneComponent> SceneRoot;

    // 제너레이터 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Harness")
    TObjectPtr<UHarnessGeneratorComponent> HarnessComponent;

    // 캡처 컴포넌트 추가
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Harness")
    TObjectPtr<UHarnessMinimapCaptureComponent> CaptureComponent;

    // 블루프린트 위젯(WBP_CaptureMinimap) 클래스를 할당할 변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness|UI")
    TSubclassOf<UHarnessCaptureMinimapWidget> MinimapWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness")
    bool bBuildOnBeginPlay = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness")
    FString JsonFilePath;

private:
    // 생성된 위젯 인스턴스 보관용 포인터
    UPROPERTY(Transient)
    TObjectPtr<UHarnessCaptureMinimapWidget> MinimapWidget;
};