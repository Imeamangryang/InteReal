#include "BaseComboBox.h"
#include "Components/ComboBoxString.h"
#include "InteReal/Master/UI/DesignTemplate/InteRealThemeData.h"
#include "Styling/SlateTypes.h"

void UBaseComboBox::NativePreConstruct()
{
    Super::NativePreConstruct();

    if (!ComboBox_Main) return;

    // 1. 옵션 초기화
    ComboBox_Main->ClearOptions();
    for (const FString& Option : DefaultOptions)
    {
        ComboBox_Main->AddOption(Option);
    }

    if (DefaultOptions.Num() > 0)
    {
        ComboBox_Main->SetSelectedIndex(0);
    }
    
    if (!ThemeData) return;

    // 2. 콤보박스 메인 버튼 및 팝업창 스타일 설정
    FComboBoxStyle ComboStyle = ComboBox_Main->GetWidgetStyle();
    
    auto ConfigureBrush = [&](FSlateBrush& OutBrush, const FLinearColor& StrokeColor)
    {
        OutBrush = FSlateBrush();
        OutBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        OutBrush.TintColor = FSlateColor(ThemeData->Card_BG_White);
        OutBrush.OutlineSettings.Color = FSlateColor(StrokeColor);
        OutBrush.OutlineSettings.Width = 1.0f;
        OutBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
        OutBrush.OutlineSettings.CornerRadii = FVector4(10.0f, 10.0f, 10.0f, 10.0f);
    };

    // 버튼 상태별 스타일 적용
    ConfigureBrush(ComboStyle.ComboButtonStyle.ButtonStyle.Normal, ThemeData->Card_Border);
    ConfigureBrush(ComboStyle.ComboButtonStyle.ButtonStyle.Hovered, ThemeData->Accent_Gold);
    ConfigureBrush(ComboStyle.ComboButtonStyle.ButtonStyle.Pressed, ThemeData->Accent_Gold);
    ConfigureBrush(ComboStyle.ComboButtonStyle.ButtonStyle.Disabled, ThemeData->Card_Border);

    ComboStyle.ComboButtonStyle.ButtonStyle.NormalForeground = FSlateColor(ThemeData->Sub_Text);
    ComboStyle.ComboButtonStyle.ButtonStyle.HoveredForeground = FSlateColor(ThemeData->Main_Navy);
    ComboStyle.ComboButtonStyle.ButtonStyle.PressedForeground = FSlateColor(ThemeData->Main_Navy);

    ComboStyle.ComboButtonStyle.MenuBorderBrush.DrawAs = ESlateBrushDrawType::NoDrawType; 
    ComboStyle.ComboButtonStyle.MenuBorderBrush.TintColor = FLinearColor::Transparent;
    ComboStyle.ComboButtonStyle.MenuBorderBrush.OutlineSettings.Width = 0.0f;
    ComboStyle.ComboButtonStyle.MenuBorderBrush.OutlineSettings.Color = FLinearColor::Transparent;
    
    ComboBox_Main->SetWidgetStyle(ComboStyle);

    // 3. 팝업창 내부 리스트 항목 스타일 설정
    FTableRowStyle ItemStyle = ComboBox_Main->GetItemStyle();

    auto ConfigureRowBrush = [&](FSlateBrush& OutBrush, const FLinearColor& BgColor)
    {
        OutBrush = FSlateBrush();
        OutBrush.DrawAs = ESlateBrushDrawType::Box;
        OutBrush.TintColor = FSlateColor(BgColor);
    };

    // 항목 배경색 설정
    ConfigureRowBrush(ItemStyle.EvenRowBackgroundBrush, ThemeData->Card_BG_White);
    ConfigureRowBrush(ItemStyle.OddRowBackgroundBrush, ThemeData->Card_BG_White);

    // 마우스 호버링 및 선택 효과
    ConfigureRowBrush(ItemStyle.InactiveHoveredBrush, ThemeData->Card_Border);
    ConfigureRowBrush(ItemStyle.ActiveHoveredBrush, ThemeData->Card_Border);
    ConfigureRowBrush(ItemStyle.EvenRowBackgroundHoveredBrush, ThemeData->Card_Border);
    ConfigureRowBrush(ItemStyle.OddRowBackgroundHoveredBrush, ThemeData->Card_Border);

    ConfigureRowBrush(ItemStyle.ActiveBrush, ThemeData->Card_BG_White);
    ConfigureRowBrush(ItemStyle.InactiveBrush, ThemeData->Card_BG_White);

    // 글자 색상
    ItemStyle.TextColor = FSlateColor(ThemeData->Sub_Text);
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