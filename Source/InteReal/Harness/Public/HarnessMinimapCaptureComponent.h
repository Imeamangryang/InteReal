// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/ActorComponent.h"
#include "HarnessMinimapCaptureComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class INTEREAL_API UHarnessMinimapCaptureComponent : public USceneCaptureComponent2D
{
	GENERATED_BODY()

public:
	UHarnessMinimapCaptureComponent();

	UFUNCTION(BlueprintCallable, Category="Harness|Minimap")
	void ApplyStableCaptureSettings();

	// 렌더 타겟을 싱글턴 패턴처럼 관리하여 반환
	UFUNCTION(BlueprintCallable, Category="Harness|Minimap")
	UTextureRenderTarget2D* GetOrCreateRenderTarget(int32 Resolution = 1024);

	// 전달받은 바운딩 박스(AABB) 영역에 맞춰 카메라 위치와 직교 투영 범위(OrthoWidth) 자동 조절
	UFUNCTION(BlueprintCallable, Category="Harness|Minimap")
	void AdjustToBoundingBox(FVector2D MinBounds, FVector2D MaxBounds, float Padding = 500.0f);

	// 미니맵 수동 업데이트 트리거 (가구 배치, 재질 변경 시 호출)
	UFUNCTION(BlueprintCallable, Category="Harness|Minimap")
	void UpdateMinimap();

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> MinimapRenderTarget;

	FVector2D CachedMin;
	FVector2D CachedMax;
};
