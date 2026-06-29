#include "BaseInput.h"
#include "Components/TextBlock.h"
#include "InteReal/Master/UI/DesignTemplate/InteRealThemeData.h"
#include "Styling/SlateTypes.h"

void UBaseInput::NativePreConstruct()
{
    Super::NativePreConstruct();

    // 고정 접두/접미 글자 표시 여부는 ThemeData 유무와 상관없이 항상 적용되어야 함
    if (Text_Prefix)
    {
        Text_Prefix->SetText(PrefixText);
        Text_Prefix->SetVisibility(PrefixText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    }
    if (Text_Suffix)
    {
        Text_Suffix->SetText(SuffixText);
        Text_Suffix->SetVisibility(SuffixText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    }

    if (!Input_Main || !Border_Bg || !ThemeData) return;

    // 1. 힌트 텍스트 세팅
    Input_Main->SetHintText(HintText);

    // 2. 글자 색상 세팅
    FEditableTextStyle TextStyle = Input_Main->WidgetStyle;
    TextStyle.ColorAndOpacity = FSlateColor(ThemeData->Main_Navy);
    Input_Main->SetWidgetStyle(TextStyle);

    // 2.5. 접두/접미 글자 색상 (ThemeData가 있을 때만 입힘)
    if (Text_Prefix)
    {
        Text_Prefix->SetColorAndOpacity(FSlateColor(ThemeData->Main_Navy));
    }
    if (Text_Suffix)
    {
        Text_Suffix->SetColorAndOpacity(FSlateColor(ThemeData->Sub_Text));
    }

    if (bShowBackground)
    {
        // 3. Border 배경 및 둥근 테두리 세팅
        FSlateBrush BgBrush;
        BgBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        BgBrush.TintColor = FSlateColor(ThemeData->Card_BG_White);

        // 테두리(Outline) 상세 설정
        BgBrush.OutlineSettings.Color = FSlateColor(ThemeData->Card_Border);
        BgBrush.OutlineSettings.Width = 1.0f;
        BgBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
        BgBrush.OutlineSettings.CornerRadii = FVector4(CornerRadius, CornerRadius, CornerRadius, CornerRadius);

        Border_Bg->SetBrush(BgBrush);
    }
    else
    {
        FSlateBrush NoBrush;
        NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
        Border_Bg->SetBrush(NoBrush);
    }
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