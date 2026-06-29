#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EditModeToolbarWidget.generated.h"

class UToggleButtonWidget;
class AInteRealPlayerController;
class UTexture2D;
class UBorder;
class UWidget;

UCLASS()
class INTEREAL_API UEditModeToolbarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

public:
	void InitializeForPlayer(AInteRealPlayerController* InPlayerController);

	// 펼쳐진(활성) 상태일 때 보이는 화살표 — 누르면 접힌다는 뜻으로 오른쪽을 가리킴
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toolbar|Icons")
	UTexture2D* Icon_CollapseRight = nullptr;

	// 접힌 상태일 때 보이는 화살표 — 누르면 펼쳐진다는 뜻으로 왼쪽을 가리킴
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toolbar|Icons")
	UTexture2D* Icon_CollapseLeft = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toolbar|Collapse")
	FVector2D ExpandedBoxSize = FVector2D(220.0f, 64.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toolbar|Collapse")
	FVector2D CollapsedBoxSize = FVector2D(48.0f, 48.0f);

	// 펼쳐졌을 땐 화면 가장자리에서 살짝 띄우고(-20), 접히면 가장자리에 딱 붙게(0)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toolbar|Collapse")
	float ExpandedOffsetX = -20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toolbar|Collapse")
	float CollapsedOffsetX = 0.0f;

private:
	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* B_ToolbarBox;

	UPROPERTY(meta = (BindWidgetOptional))
	UToggleButtonWidget* Btn_Grid;

	UPROPERTY(meta = (BindWidget))
	UToggleButtonWidget* Btn_PlacementMode;

	UPROPERTY(meta = (BindWidget))
	UToggleButtonWidget* Btn_MoveGizmo;

	UPROPERTY(meta = (BindWidget))
	UToggleButtonWidget* Btn_RotateGizmo;
	
	UPROPERTY(meta = (BindWidgetOptional))
	UToggleButtonWidget* Btn_Collapse;
	
	UPROPERTY(meta = (BindWidgetOptional))
	UWidget* Box_Buttons;

	UFUNCTION()
	void HandleGridClicked();

	UFUNCTION()
	void HandlePlacementModeClicked();

	UFUNCTION()
	void HandleMoveClicked();

	UFUNCTION()
	void HandleRotateClicked();

	UFUNCTION()
	void HandleCollapseClicked();

	bool bIsCollapsed = false;

	void RefreshAll();

	TWeakObjectPtr<AInteRealPlayerController> PlayerController;

	bool bCachedInitialized = false;
	bool bCachedShowGrid = true;
	bool bCachedFreePlacement = false;
	bool bCachedShowMove = true;
	bool bCachedShowRotate = true;
};
