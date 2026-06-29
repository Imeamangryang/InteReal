#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ViewModeData.h"
#include "ViewModePlayerController.generated.h"

class UHarnessMinimapCaptureComponent;
class AViewModeManager;
class UInputMappingContext;
class UInputAction;

/**
 * PlayerController specialized for switching between different view modes.
 */
UCLASS()
class INTEREAL_API AViewModePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AViewModePlayerController();

	// Mode switching logic
	UFUNCTION(BlueprintCallable, Category = "ViewMode")
	void SetViewMode(EHarnessViewMode NewMode);
	
	UFUNCTION(BlueprintCallable, Category="Harness|Minimap")
	void ShowMinimap();

protected:
	UFUNCTION()
	void HandlePipelineLoadFinished();

	bool bShouldResetFirstPersonPosition = true;

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	
	// Key handlers
	void OnTopDownKey();
	void OnIsometricKey();
	void OnFirstPersonKey();

	// Enhanced Input Handlers
	void EnhancedMove(const struct FInputActionValue& Value);
	void EnhancedLook(const struct FInputActionValue& Value);
	void EnhancedZoom(const struct FInputActionValue& Value);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputMappingContext> ViewModeMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputAction> IA_SwitchToTopDown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputAction> IA_SwitchToIsometric;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputAction> IA_SwitchToFirstPerson;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputAction> IA_Move;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputAction> IA_Look;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|Input")
	TObjectPtr<UInputAction> IA_Zoom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|UI")
	TSubclassOf<UUserWidget> ViewModeWidgetClass;

	// 미니맵 위젯 클래스
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode|UI")
	TSubclassOf<class UHarnessCaptureMinimapWidget> MinimapWidgetClass;

private:
	UPROPERTY()
	TObjectPtr<AViewModeManager> CachedViewModeManager;

	UPROPERTY()
	TObjectPtr<UUserWidget> ViewModeWidgetInstance;

	UPROPERTY()
	TObjectPtr<class UHarnessCaptureMinimapWidget> MinimapWidgetInstance;

	void FindViewModeManager();
	void UpdateMinimapIconVisibility(EHarnessViewMode NewMode);
};
