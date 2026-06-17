// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteRealMinimap.generated.h"

class UImage;
class UHarnessMinimapCaptureComponent;
class UTextureRenderTarget2D;

UCLASS()
class INTEREAL_API UInteRealMinimap : public UUserWidget
{
	GENERATED_BODY()

public:
	// UMG 디자이너에서 생성할 이미지 위젯 (렌더 타겟 표시용)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> MinimapImage;

	// UMG 디자이너에서 생성할 플레이어 아이콘 위젯 (Canvas 내 조절용)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> PlayerIcon;

	// 캡처를 담당하는 컴포넌트 참조
	UPROPERTY(BlueprintReadWrite, Category = "InteReal|Minimap")
	TObjectPtr<UHarnessMinimapCaptureComponent> CaptureCameraComp;

	// 외부(매니저 등)에서 캡처 컴포넌트와 렌더 타겟을 주입받는 함수
	UFUNCTION(BlueprintCallable, Category = "InteReal|Minimap")
	void InjectMinimapData(UHarnessMinimapCaptureComponent* InCameraComp, UTextureRenderTarget2D* InRenderTarget);
	
protected:
	virtual void NativeConstruct() override;
	
	// 매 프레임 플레이어 위치 및 회전값을 동기화할 틱 함수
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 미니맵 클릭 시 3D 월드 좌표로 역산하여 캐릭터를 텔레포트시키는 이벤트
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
};