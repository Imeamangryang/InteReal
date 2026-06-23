#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ViewModeData.h"
#include "ViewModeManager.generated.h"

class USpringArmComponent;
class UCameraComponent;

UCLASS()
class INTEREAL_API AViewModeManager : public AActor
{
	GENERATED_BODY()
	
public:	
	AViewModeManager();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

public:
	UFUNCTION(BlueprintCallable, Category = "ViewMode")
	void SetViewMode(EHarnessViewMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "ViewMode")
	void FocusOnBuilding();

	UFUNCTION(BlueprintCallable, Category = "ViewMode")
	void CalculateOptimalZoom();

	UFUNCTION(BlueprintCallable, Category = "ViewMode")
	void ToggleCanvasRotation();

	UFUNCTION(BlueprintPure, Category = "ViewMode")
	bool IsCanvasRotated() const { return bIsCanvasRotated; }

	// Interactive Inputs
	void AddRotationInput(float DeltaYaw, float DeltaPitch);
	void AddMovementInput(FVector Direction, float Scale);
	void AddPanInput(float DeltaX, float DeltaY);
	void AddZoomInput(float Delta);

	UFUNCTION(BlueprintPure, Category = "ViewMode")
	EHarnessViewMode GetCurrentViewMode() const { return CurrentMode; }

	UFUNCTION(BlueprintPure, Category = "ViewMode")
	FVector GetCameraTargetLocation() const { return TargetLocation; }

	// 타겟 위치로 즉시 카메라를 스냅시키는 함수
	UFUNCTION(BlueprintCallable, Category="Harness|Camera")
	void SnapToTarget();

	// 지정한 월드 위치로 카메라를 부드럽게 이동 (XY만 변경, Z 유지)
	UFUNCTION(BlueprintCallable, Category="Harness|Camera")
	void FocusOnLocation(FVector WorldLocation);

	UFUNCTION(BlueprintCallable, Category="Harness|Camera")
	void SetFloorPlanPanelOffset(bool bPanelOpen);

private:
	UPROPERTY(VisibleAnywhere, Category = "ViewMode|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "ViewMode|Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "ViewMode|Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditAnywhere, Category = "ViewMode|Settings")
	float TransitionSpeed = 5.0f;

	UPROPERTY(EditAnywhere, Category = "ViewMode|Presets")
	float TopDownArmLength = 1800.f;

	UPROPERTY(EditAnywhere, Category = "ViewMode|Presets")
	float TopDownFOV = 90.f;

	UPROPERTY(EditAnywhere, Category = "ViewMode|Presets")
	float IsometricArmLength = 1500.f;

	UPROPERTY(EditAnywhere, Category = "ViewMode|Presets")
	float IsometricFOV = 60.f;

	UPROPERTY(EditAnywhere, Category = "ViewMode|Presets")
	float FirstPersonHeight = 160.0f;

	UPROPERTY(EditAnywhere, Category = "ViewMode|Presets")
	float FirstPersonFOV = 100.f;

	UPROPERTY(EditAnywhere, Category = "ViewMode|Interaction")
	float RotationSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, Category = "ViewMode|Interaction")
	float MovementSpeed = 600.0f;

	UPROPERTY(EditAnywhere, Category = "ViewMode|Interaction")
	float PanSpeed = 10.0f;

	UPROPERTY(EditAnywhere, Category = "ViewMode|Interaction")
	float ZoomSpeed = 150.0f;

	EHarnessViewMode CurrentMode = EHarnessViewMode::Isometric;

	UPROPERTY(VisibleAnywhere, Category = "ViewMode")
	bool bIsCanvasRotated = false;

	// Target Parameters for Interpolation
	FVector TargetLocation;
	FRotator TargetRotation;
	float TargetArmLength;
	float TargetFOV;

	// Mode Presets
	void UpdateTargetParameters();
	
	FVector BuildingCenter = FVector::ZeroVector;
	float CurrentPanelWidthRatio = 0.f;
};
