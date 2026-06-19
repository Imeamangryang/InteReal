// Fill out your copyright notice in the Description page of Project Settings.

#include "InteRealMinimap.h"
#include "InteReal/Harness/Public/HarnessMinimapCaptureComponent.h"

#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

void UInteRealMinimap::NativeConstruct()
{
	Super::NativeConstruct();
	// 포커스 강제 탈취 버그 방지를 위해 비워둠
}

void UInteRealMinimap::InjectMinimapData(UHarnessMinimapCaptureComponent* InCameraComp, UTextureRenderTarget2D* InRenderTarget)
{
	CaptureCameraComp = InCameraComp;
	if (MinimapImage && InRenderTarget)
	{
		MinimapImage->SetBrushResourceObject(InRenderTarget);
	}
}

FReply UInteRealMinimap::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!CaptureCameraComp || !MinimapImage)
	{
		return FReply::Unhandled();
	}

	// 미니맵 이미지 패널의 실제 화면 고유 영역 좌표 추출
	FGeometry MinimapGeo = MinimapImage->GetCachedGeometry();
	FVector2D LocalMousePos = MinimapGeo.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	FVector2D LocalSize = MinimapGeo.GetLocalSize();

	if (LocalSize.X <= 0.0f || LocalSize.Y <= 0.0f)
	{
		return FReply::Unhandled();
	}

	// 0.0 ~ 1.0 비율(UV) 산출
	float U = LocalMousePos.X / LocalSize.X;
	float V = LocalMousePos.Y / LocalSize.Y; 

	// 미니맵 영역 외곽 클릭 예외 처리
	if (U < 0.0f || U > 1.0f || V < 0.0f || V > 1.0f)
	{
		return FReply::Unhandled();
	}

	FVector CameraLoc = CaptureCameraComp->GetComponentLocation();
	float OrthoWidth = CaptureCameraComp->OrthoWidth;
	
	// OrthoWidth 기반 3D 월드 좌표 정밀 역산
	float TargetWorldX = CameraLoc.X + ((U - 0.5f) * OrthoWidth);
	float TargetWorldY = CameraLoc.Y + ((V - 0.5f) * OrthoWidth);

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PC)
	{
		APawn* PlayerPawn = PC->GetPawn();
		if (PlayerPawn)
		{
			// 현재 캐릭터의 Z축 높이를 유지하며 이동
			float CurrentZ = PlayerPawn->GetActorLocation().Z;
			PlayerPawn->SetActorLocation(FVector(TargetWorldX, TargetWorldY, CurrentZ));
		}
	}

	return FReply::Handled();
}

void UInteRealMinimap::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!CaptureCameraComp || !MinimapImage || !PlayerIcon)
	{
		return;
	}

	// 아이콘이 비활성화 상태라면 동기화 스킵
	if (PlayerIcon->GetVisibility() != ESlateVisibility::SelfHitTestInvisible && 
		PlayerIcon->GetVisibility() != ESlateVisibility::Visible)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return;

	AActor* ViewTarget = PC->GetViewTarget();
	if (!ViewTarget) return;

	FVector PlayerLoc = ViewTarget->GetActorLocation();
	FVector CameraLoc = CaptureCameraComp->GetComponentLocation();
	float OrthoWidth = CaptureCameraComp->OrthoWidth;

	UCanvasPanelSlot* MinimapSlot = Cast<UCanvasPanelSlot>(MinimapImage->Slot);
	UCanvasPanelSlot* PlayerSlot = Cast<UCanvasPanelSlot>(PlayerIcon->Slot);
	if (!MinimapSlot || !PlayerSlot) return;

	FVector2D MinimapSize = MinimapSlot->GetSize();
	if (MinimapSize.X <= 0.0f || MinimapSize.Y <= 0.0f) return;

	// 월드 좌표 좌표계를 2D 공간의 0 ~ 1 매핑 값으로 치환
	float U = 0.5f + ((PlayerLoc.X - CameraLoc.X) / OrthoWidth);
	float V = 0.5f + ((PlayerLoc.Y - CameraLoc.Y) / OrthoWidth);

	U = FMath::Clamp(U, 0.0f, 1.0f);
	V = FMath::Clamp(V, 0.0f, 1.0f);
	
	// 슬롯의 정렬(Alignment) 마진까지 완벽하게 계산하여 좌상단 앵커 기준 실제 좌표 유도
	FVector2D MinimapPos = MinimapSlot->GetPosition();
	FVector2D MinimapAlignment = MinimapSlot->GetAlignment();
	FVector2D RealMinimapTopLeft = MinimapPos - FVector2D(MinimapAlignment.X * MinimapSize.X, MinimapAlignment.Y * MinimapSize.Y);

	FVector2D NewIconPos = RealMinimapTopLeft + FVector2D(U * MinimapSize.X, V * MinimapSize.Y);
	PlayerSlot->SetPosition(NewIconPos);

	// 1인칭 및 소유 캐릭터 시선의 회전값(Yaw)을 실시간 렌더 트랜스폼 각도에 동기화
	PlayerIcon->SetRenderTransformAngle(ViewTarget->GetActorRotation().Yaw);
}
