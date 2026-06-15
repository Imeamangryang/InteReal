#include "BaseComboBox.h"

#include "InteReal/Master/UI/DesignTemplate/InteRealThemeData.h"
#include "Styling/SlateTypes.h"

void UBaseComboBox::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (!ComboBox_Main) return;

	ComboBox_Main->ClearOptions();
	for (const FString& Option : DefaultOptions)
	{
		ComboBox_Main->AddOption(Option);
	}

	if (!ThemeData) return;

	FComboBoxStyle ComboStyle = ComboBox_Main->GetWidgetStyle();
	
	auto ConfigureBrush = [&](FSlateBrush& OutBrush, const FLinearColor& StrokeColor)
	{
		OutBrush = FSlateBrush(); // 안전한 초기화
		OutBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		OutBrush.TintColor = FSlateColor(ThemeData->Card_BG_White);
		OutBrush.OutlineSettings.Color = FSlateColor(StrokeColor);
		OutBrush.OutlineSettings.Width = 1.0f;
		OutBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		OutBrush.OutlineSettings.CornerRadii = FVector4(10.0, 10.0, 10.0, 10.0);
	};

	ConfigureBrush(ComboStyle.ComboButtonStyle.ButtonStyle.Normal, ThemeData->Card_Border);
	ConfigureBrush(ComboStyle.ComboButtonStyle.ButtonStyle.Hovered, ThemeData->Accent_Gold);

	ComboStyle.ComboButtonStyle.ButtonStyle.NormalForeground = FSlateColor(ThemeData->Sub_Text);
	ComboStyle.ComboButtonStyle.ButtonStyle.HoveredForeground = FSlateColor(ThemeData->Main_Navy);
	
	ComboBox_Main->SetWidgetStyle(ComboStyle);

	FTableRowStyle ItemStyle = ComboBox_Main->GetItemStyle();
	ItemStyle.SelectedTextColor = FSlateColor(ThemeData->Main_Navy);
	
	ComboBox_Main->SetItemStyle(ItemStyle);
}

void UBaseComboBox::NativeConstruct()
{
	Super::NativeConstruct();

	if (ComboBox_Main)
	{
		ComboBox_Main->OnSelectionChanged.AddUniqueDynamic(this, &UBaseComboBox::HandleSelectionChanged);
	}
}

void UBaseComboBox::HandleSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	OnBaseSelectionChanged.Broadcast(SelectedItem, SelectionType);
}
