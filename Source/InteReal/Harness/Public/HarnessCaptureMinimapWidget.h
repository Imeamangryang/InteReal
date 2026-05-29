// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HarnessCaptureMinimapWidget.generated.h"

class UOverlay;
class UHarnessMinimapCaptureComponent;
class ASceneCapture2D;
class UImage;

UCLASS()
class INTEREAL_API UHarnessCaptureMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// UMG 디자이너에서 생성할 이미지와 이름이 정확히 일치해야 자동 바인딩됩니다.
	UPROPERTY(meta = (BindWidget))
	UImage* MinimapImage;

	UPROPERTY(meta = (BindWidget))
	UOverlay* PlayerIcon;
	

	UPROPERTY(BlueprintReadWrite, Category = "Minimap")
	UHarnessMinimapCaptureComponent* CaptureCameraComp;

	UFUNCTION(BlueprintCallable, Category = "Minimap")
	void InjectMinimapData(UHarnessMinimapCaptureComponent* InCameraComp, UTextureRenderTarget2D* InRenderTarget);
	
protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	
protected:
	virtual void NativeConstruct() override;
	// 매 프레임 플레이어 위치를 동기화할 Tick 함수
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
};
