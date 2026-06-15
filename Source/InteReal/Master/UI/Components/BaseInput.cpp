#include "BaseInput.h"
#include "InteReal/Master/UI/DesignTemplate/InteRealThemeData.h"
#include "Styling/SlateTypes.h"

void UBaseInput::NativePreConstruct()
{
    Super::NativePreConstruct();

    if (!Input_Main || !Border_Bg || !ThemeData) return;

    // 1. 힌트 텍스트 세팅
    Input_Main->SetHintText(HintText);

    // 2. 글자 색상 세팅
    FEditableTextStyle TextStyle = Input_Main->WidgetStyle;
    TextStyle.ColorAndOpacity = FSlateColor(ThemeData->Main_Navy);
    Input_Main->SetWidgetStyle(TextStyle);

    // 3. Border 배경 및 둥근 테두리 세팅
    FSlateBrush BgBrush;
    BgBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
    BgBrush.TintColor = FSlateColor(ThemeData->Card_BG_White);
    
    // 테두리(Outline) 상세 설정
    BgBrush.OutlineSettings.Color = FSlateColor(ThemeData->Card_Border);
    BgBrush.OutlineSettings.Width = 1.0f;
    BgBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
    BgBrush.OutlineSettings.CornerRadii = FVector4(10.0, 10.0, 10.0, 10.0);

    Border_Bg->SetBrush(BgBrush);
}

void UBaseInput::NativeConstruct()
{
    Super::NativeConstruct();

    if (Input_Main)
    {
        Input_Main->OnTextChanged.AddUniqueDynamic(this, &UBaseInput::HandleTextChanged);
        Input_Main->OnTextCommitted.AddUniqueDynamic(this, &UBaseInput::HandleTextCommitted);
    }
}

void UBaseInput::HandleTextChanged(const FText& Text)
{
    OnBaseTextChanged.Broadcast(Text);
}

void UBaseInput::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
    OnBaseTextCommitted.Broadcast(Text, CommitMethod);
}