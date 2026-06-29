#include "BaseToggleSwitch.h"
#include "InteReal/Master/UI/DesignTemplate/InteRealThemeData.h"
#include "Components/OverlaySlot.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

void UBaseToggleSwitch::NativePreConstruct()
{
	Super::NativePreConstruct();
	UpdateToggleUI();
}

void UBaseToggleSwitch::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Main)
	{
		Btn_Main->OnClicked.AddUniqueDynamic(this, &UBaseToggleSwitch::HandleClicked);
	}
}

void UBaseToggleSwitch::HandleClicked()
{
	SetIsOn(!bIsOn, true);
}

void UBaseToggleSwitch::SetIsOn(bool bNewOn, bool bBroadcastEvent)
{
	if (bIsOn == bNewOn)
	{
		return;
	}

	bIsOn = bNewOn;
	UpdateToggleUI();

	if (bBroadcastEvent)
	{
		OnToggleChanged.Broadcast(bIsOn);
	}
}

void UBaseToggleSwitch::UpdateToggleUI()
{
	if (!Border_Track || !Border_Knob || !ThemeData)
	{
		return;
	}
	
	if (Btn_Main)
	{
		FButtonStyle ButtonStyle = Btn_Main->GetStyle();
		FSlateBrush NoBrush;
		NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		ButtonStyle.Normal = NoBrush;
		ButtonStyle.Hovered = NoBrush;
		ButtonStyle.Pressed = NoBrush;
		ButtonStyle.Disabled = NoBrush;
		Btn_Main->SetStyle(ButtonStyle);
	}

	// RoundingType은 기본값(HalfHeightRadius)을 그대로 둬서 트랙은 알약 모양, 손잡이는 항상 완전한 원이 되도록 한다
	FSlateBrush TrackBrush;
	TrackBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	TrackBrush.TintColor = FSlateColor(bIsOn ? ThemeData->Accent_Gold : ThemeData->Card_Border);
	Border_Track->SetBrush(TrackBrush);

	FSlateBrush KnobBrush;
	KnobBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	KnobBrush.TintColor = FSlateColor(ThemeData->Card_BG_White);
	Border_Knob->SetBrush(KnobBrush);

	// Border_Knob이 크기 고정용 SizeBox 등으로 감싸져 있으면 그 슬롯은 Overlay 슬롯이 아니므로,
	// Overlay 슬롯을 가진 조상을 찾을 때까지 위로 올라가서 정렬을 바꾼다
	for (UWidget* Current = Border_Knob; Current; Current = Current->GetParent())
	{
		if (UOverlaySlot* KnobSlot = Cast<UOverlaySlot>(Current->Slot))
		{
			KnobSlot->SetHorizontalAlignment(bIsOn ? HAlign_Right : HAlign_Left);
			break;
		}
	}
}
