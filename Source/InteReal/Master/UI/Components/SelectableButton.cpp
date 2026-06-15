#include "SelectableButton.h"
#include "Components/TextBlock.h"
#include "InteReal/Master/UI/DesignTemplate/InteRealThemeData.h"
#include "Styling/SlateBrush.h"

void USelectableButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (Txt_Label)
	{
		Txt_Label->SetText(ButtonText);
	}

	UpdateSelectionUI();
}

void USelectableButton::SetIsSelected(bool bNewSelected)
{
	if (bIsSelected != bNewSelected)
	{
		bIsSelected = bNewSelected;
		UpdateSelectionUI();
	}
}

void USelectableButton::UpdateSelectionUI()
{
	if (!Btn_Main || !ThemeData)
	{
		return;
	}

	FButtonStyle NewStyle = Btn_Main->GetStyle();

	// 패딩 설정
	NewStyle.NormalPadding = ContentPadding;
	NewStyle.PressedPadding = ContentPadding;

	FLinearColor BgColor = ThemeData->Card_BG_White;
	FLinearColor OutlineColor = bIsSelected ? ThemeData->Accent_Gold : ThemeData->Card_Border;
	FLinearColor TextColor = bIsSelected ? ThemeData->Main_Navy : ThemeData->Sub_Text;
	float OutlineWidth = 2.0f;

	// 람다를 사용하여 각 상태(Normal, Hovered, Pressed)의 브러시 설정
	auto ConfigureBrush = [&](FSlateBrush& Brush)
	{
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(BgColor);
		
		// 아웃라인 설정 (SlateCore)
		Brush.OutlineSettings.Color = FSlateColor(OutlineColor);
		Brush.OutlineSettings.Width = OutlineWidth;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(10.0f, 10.0f, 10.0f, 10.0f);
	};

	ConfigureBrush(NewStyle.Normal);
	ConfigureBrush(NewStyle.Hovered);
	ConfigureBrush(NewStyle.Pressed);

	Btn_Main->SetStyle(NewStyle);

	if (Txt_Label)
	{
		Txt_Label->SetColorAndOpacity(FSlateColor(TextColor));
	}
}
