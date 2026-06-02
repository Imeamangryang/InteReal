// Fill out your copyright notice in the Description page of Project Settings.

#include "InteReal/Harness/Public/HarnessCaptureMinimapWidget.h"
#include "InteReal/Harness/Public/HarnessMinimapCaptureComponent.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

void UHarnessCaptureMinimapWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 💡 [입력 차단 수정] 미니맵 생성 시 강제로 포커스를 가져오는 로직을 제거했습니다.
    // 이제 다른 위젯(버튼 등)과 입력을 공유할 수 있습니다.
}

FReply UHarnessCaptureMinimapWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (!CaptureCameraComp || !MinimapImage)
    {
       return FReply::Unhandled();
    }

    // 💡 [버그 수정 1] 전체 화면 기준(InGeometry)이 아닌, 미니맵 이미지 패널의 고유 영역 기준 좌표 추출
    FGeometry MinimapGeo = MinimapImage->GetCachedGeometry();
    FVector2D LocalMousePos = MinimapGeo.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    FVector2D LocalSize = MinimapGeo.GetLocalSize();

    // 미니맵 이미지 내부에서의 0.0 ~ 1.0 비율(UV) 산출
    float U = LocalMousePos.X / LocalSize.X;
    float V = LocalMousePos.Y / LocalSize.Y; 

    // 미니맵 영역 밖을 클릭한 경우 예외 처리
    if (U < 0.0f || U > 1.0f || V < 0.0f || V > 1.0f)
    {
        return FReply::Unhandled();
    }

    FVector CameraLoc = CaptureCameraComp->GetComponentLocation();
    float OrthoWidth = CaptureCameraComp->OrthoWidth;
    
    // 💡 [버그 수정 2] 1:1 정방형 캡처이므로 X, Y 축 모두 OrthoWidth를 기준으로 3D 좌표 역산
    float TargetWorldX = CameraLoc.X - ((V - 0.5f) * OrthoWidth);
    float TargetWorldY = CameraLoc.Y + ((U - 0.5f) * OrthoWidth);

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC)
    {
        APawn* PlayerPawn = PC->GetPawn();
        if (PlayerPawn)
        {
            PlayerPawn->SetActorLocation(FVector(TargetWorldX, TargetWorldY, 150.0f));
        }
    }

    return FReply::Handled();
}

void UHarnessCaptureMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!CaptureCameraComp || !MinimapImage || !PlayerIcon)
    {
        return;
    }

    // 1인칭 모드에서 아이콘이 보이지 않는다면 위치 업데이트를 건너뜜
    if (PlayerIcon->GetVisibility() != ESlateVisibility::SelfHitTestInvisible)
    {
        return;
    }

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC) return;

    // 현재 카메라가 쳐다보고 있는 타겟(Manager 또는 Character)을 추적
    AActor* ViewTarget = PC->GetViewTarget();
    if (!ViewTarget) return;

    FVector PlayerLoc = ViewTarget->GetActorLocation();
    FVector CameraLoc = CaptureCameraComp->GetComponentLocation();
    float OrthoWidth = CaptureCameraComp->OrthoWidth;

    UCanvasPanelSlot* MinimapSlot = Cast<UCanvasPanelSlot>(MinimapImage->Slot);
    UCanvasPanelSlot* PlayerSlot = Cast<UCanvasPanelSlot>(PlayerIcon->Slot);
    if (!MinimapSlot || !PlayerSlot) return;

    FVector2D MinimapSize = MinimapSlot->GetSize();
    if (MinimapSize.X <= 0 || MinimapSize.Y <= 0) return;

    float U = 0.5f + ((PlayerLoc.Y - CameraLoc.Y) / OrthoWidth);
    float V = 0.5f - ((PlayerLoc.X - CameraLoc.X) / OrthoWidth);

    U = FMath::Clamp(U, 0.0f, 1.0f);
    V = FMath::Clamp(V, 0.0f, 1.0f);
    
    FVector2D MinimapPos = MinimapSlot->GetPosition();
    FVector2D MinimapAlignment = MinimapSlot->GetAlignment();
    FVector2D RealMinimapTopLeft = MinimapPos - FVector2D(MinimapAlignment.X * MinimapSize.X, MinimapAlignment.Y * MinimapSize.Y);

    FVector2D NewIconPos = RealMinimapTopLeft + FVector2D(U * MinimapSize.X, V * MinimapSize.Y);
    PlayerSlot->SetPosition(NewIconPos);

    // 회전값 동기화 (1인칭 시 캐릭터의 Yaw와 일치)
    PlayerIcon->SetRenderTransformAngle(ViewTarget->GetActorRotation().Yaw);
}

void UHarnessCaptureMinimapWidget::InjectMinimapData(UHarnessMinimapCaptureComponent* InCameraComp, UTextureRenderTarget2D* InRenderTarget)
{
    CaptureCameraComp = InCameraComp;
    if (MinimapImage && InRenderTarget)
    {
       MinimapImage->SetBrushResourceObject(InRenderTarget);
    }
}