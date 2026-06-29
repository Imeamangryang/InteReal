#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "BaseToggleSwitch.generated.h"

class UInteRealThemeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnToggleSwitchChanged, bool, bIsOn);

/**
 * UBaseToggleSwitch
 * 알약 모양 on/off 스위치 베이스 클래스 (하이브리드 패턴)
 * WBP에 Btn_Main(전체 클릭 영역), Border_Track(트랙), Border_Knob(원형 손잡이, Overlay 안에서 Btn_Main과 같은 칸)를 BindWidget으로 구성
 */
UCLASS(Abstract)
class INTEREAL_API UBaseToggleSwitch : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> Btn_Main;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI")
	TObjectPtr<UBorder> Border_Track;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI")
	TObjectPtr<UBorder> Border_Knob;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bIsOn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TObjectPtr<UInteRealThemeData> ThemeData;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnToggleSwitchChanged OnToggleChanged;

	// 외부에서 상태를 동기화할 때 사용. bBroadcastEvent가 false면 이벤트 없이 비주얼만 갱신
	UFUNCTION(BlueprintCallable, Category = "Logic")
	void SetIsOn(bool bNewOn, bool bBroadcastEvent = false);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void HandleClicked();

	void UpdateToggleUI();
};
