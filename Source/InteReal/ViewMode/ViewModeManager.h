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

	// Interactive Inputs
	void AddRotationInput(float DeltaYaw, float DeltaPitch);
	void AddMovementInput(FVector Direction, float Scale);
	void AddPanInput(float DeltaX, float DeltaY);
	void AddZoomInput(float Delta);

	UFUNCTION(BlueprintPure, Category = "ViewMode")
	EHarnessViewMode GetCurrentViewMode() const { return CurrentMode; }

	UFUNCTION(BlueprintPure, Category = "ViewMode")
	FVector GetTargetLocation() const { return TargetLocation; }

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

	EHarnessViewMode CurrentMode = EHarnessViewMode::TopDown;

	// Target Parameters for Interpolation
	FVector TargetLocation;
	FRotator TargetRotation;
	float TargetArmLength;
	float TargetFOV;

	// Mode Presets
	void UpdateTargetParameters();
};
