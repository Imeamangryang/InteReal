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

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC)
    {
        PC->SetShowMouseCursor(true);

        FInputModeGameAndUI InputMode;
        InputMode.SetWidgetToFocus(TakeWidget());
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(InputMode);
    }
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

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC) return;

    APawn* PlayerPawn = PC->GetPawn();
    if (!PlayerPawn) return;

    FVector CameraLoc = CaptureCameraComp->GetComponentLocation();
    float OrthoWidth = CaptureCameraComp->OrthoWidth;
    FVector PlayerLoc = PlayerPawn->GetActorLocation();

    UCanvasPanelSlot* MinimapSlot = Cast<UCanvasPanelSlot>(MinimapImage->Slot);
    UCanvasPanelSlot* PlayerSlot = Cast<UCanvasPanelSlot>(PlayerIcon->Slot);
    if (!MinimapSlot || !PlayerSlot) return;

    FVector2D MinimapSize = MinimapSlot->GetSize();
    if (MinimapSize.X <= 0 || MinimapSize.Y <= 0) return;

    // 세로축 비율 계산 시 OrthoHeight 제거 후 정방형 뷰포트 스펙 변환 일치
    float U = 0.5f + ((PlayerLoc.Y - CameraLoc.Y) / OrthoWidth);
    float V = 0.5f - ((PlayerLoc.X - CameraLoc.X) / OrthoWidth);

    // 플레이어가 맵 밖으로 나가도 아이콘은 미니맵 끝에 걸리도록 가두기 (Clamp)
    U = FMath::Clamp(U, 0.0f, 1.0f);
    V = FMath::Clamp(V, 0.0f, 1.0f);
    
    // MinimapImage의 Alignment(0.0, 1.0) 설정을 역산하여 좌상단 오프셋 보정
    FVector2D MinimapPos = MinimapSlot->GetPosition();
    FVector2D MinimapAlignment = MinimapSlot->GetAlignment();
    FVector2D RealMinimapTopLeft = MinimapPos - FVector2D(MinimapAlignment.X * MinimapSize.X, MinimapAlignment.Y * MinimapSize.Y);

    FVector2D NewIconPos = RealMinimapTopLeft + FVector2D(U * MinimapSize.X, V * MinimapSize.Y);
    PlayerSlot->SetPosition(NewIconPos);

    PlayerIcon->SetRenderTransformAngle(PlayerPawn->GetActorRotation().Yaw);
}

void UHarnessCaptureMinimapWidget::InjectMinimapData(UHarnessMinimapCaptureComponent* InCameraComp, UTextureRenderTarget2D* InRenderTarget)
{
    CaptureCameraComp = InCameraComp;
    if (MinimapImage && InRenderTarget)
    {
       MinimapImage->SetBrushResourceObject(InRenderTarget);
    }
}