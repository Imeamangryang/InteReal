#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ViewModeData.h"
#include "ViewModePlayerController.generated.h"

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
protected:
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

private:
	UPROPERTY()
	TObjectPtr<AViewModeManager> CachedViewModeManager;

	UPROPERTY()
	TObjectPtr<UUserWidget> ViewModeWidgetInstance;

	void FindViewModeManager();
	void UpdateMinimapIconVisibility(EHarnessViewMode NewMode);
};
