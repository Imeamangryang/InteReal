#include "SearchListWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableText.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Input/Reply.h"
#include "InteReal/Master/UI/DesignTemplate/InteRealThemeData.h"
#include "Styling/SlateTypes.h"

void UInteRealSearchListItemClickHandler::HandleClicked()
{
    if (Owner)
    {
        Owner->NotifyItemClicked(ItemId);
    }
}

USearchListWidget::USearchListWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetIsFocusable(true);
    SearchHintText = FText::FromString(TEXT("Search"));
}

void USearchListWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyThemeStyle();

    if (EditText_Search)
    {
        EditText_Search->SetHintText(SearchHintText);
    }
}

void USearchListWidget::NativeConstruct()
{
    Super::NativeConstruct();
    ApplyThemeStyle();

    if (EditText_Search)
    {
        EditText_Search->OnTextChanged.AddUniqueDynamic(this, &USearchListWidget::HandleSearchTextChanged);
        EditText_Search->SetHintText(SearchHintText);
    }

    SetResultsVisible(false);
}

void USearchListWidget::NativeDestruct()
{
    if (EditText_Search)
    {
        EditText_Search->OnTextChanged.RemoveDynamic(this, &USearchListWidget::HandleSearchTextChanged);
    }
    Super::NativeDestruct();
}

void USearchListWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    const bool bIsInputFocused = EditText_Search && EditText_Search->HasKeyboardFocus();
    const bool bIsResultsHovered = (Panel_Results && Panel_Results->IsHovered()) || (ScrollBox_Items && ScrollBox_Items->IsHovered());

    if (bIsInputFocused && !bWasInputFocused)
    {
        HandleListFocused();
    }
    else if (bIsResultsVisible && !bIsInputFocused && !bIsResultsHovered)
    {
        SetResultsVisible(false);
        RestoreSelectedText();
    }

    bWasInputFocused = bIsInputFocused;
}

FReply USearchListWidget::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
    if (EditText_Search)
    {
        EditText_Search->SetKeyboardFocus();
    }

    HandleListFocused();
    return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
}

void USearchListWidget::SetHeaderText(const FText& InHeader)
{
    if (Text_Header)
    {
        Text_Header->SetText(InHeader);
    }
}

void USearchListWidget::SetSearchText(const FString& InText)
{
    if (EditText_Search)
    {
        bSuppressSearchTextChanged = true;
        EditText_Search->SetText(FText::FromString(InText));
        bSuppressSearchTextChanged = false;
    }
}

void USearchListWidget::ClearItems()
{
    Items.Reset();
    ClearRows();
}

void USearchListWidget::AddItem(int32 ItemId, const FString& Label, bool bIsSelected)
{
    FSearchListEntry Entry;
    Entry.ItemId = ItemId;
    Entry.Label = Label;
    Entry.bIsSelected = bIsSelected;
    Items.Add(Entry);
    RebuildFilteredItems();
}

void USearchListWidget::SetResultsVisible(bool bVisible)
{
    bIsResultsVisible = bVisible;
    const ESlateVisibility DesiredVisibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;

    if (Panel_Results)
    {
        Panel_Results->SetVisibility(DesiredVisibility);
    }
    else if (ScrollBox_Items)
    {
        ScrollBox_Items->SetVisibility(DesiredVisibility);
    }
}

void USearchListWidget::NotifyItemClicked(int32 ItemId)
{
    OnItemClicked.Broadcast(ItemId);
    SetResultsVisible(false);
}

void USearchListWidget::HandleSearchTextChanged(const FText& Text)
{
    if (bSuppressSearchTextChanged)
    {
        return;
    }

    SetResultsVisible(true);
    OnSearchStringChanged.Broadcast(Text.ToString());
    RebuildFilteredItems();
}

void USearchListWidget::HandleListFocused()
{
    if (EditText_Search && !EditText_Search->GetText().IsEmpty())
    {
        bSuppressSearchTextChanged = true;
        EditText_Search->SetText(FText::GetEmpty());
        bSuppressSearchTextChanged = false;
        
        OnSearchStringChanged.Broadcast(TEXT(""));
    }

    const bool bWasVisible = bIsResultsVisible;
    SetResultsVisible(true);
    RebuildFilteredItems();

    if (!bWasVisible)
    {
        OnListFocused.Broadcast();
    }
}

void USearchListWidget::RestoreSelectedText()
{
    if (!EditText_Search) return;

    for (const FSearchListEntry& Entry : Items)
    {
        if (Entry.bIsSelected)
        {
            bSuppressSearchTextChanged = true;
            EditText_Search->SetText(FText::FromString(Entry.Label));
            bSuppressSearchTextChanged = false;
            return;
        }
    }

    bSuppressSearchTextChanged = true;
    EditText_Search->SetText(FText::GetEmpty());
    bSuppressSearchTextChanged = false;
}

void USearchListWidget::ClearRows()
{
    if (ScrollBox_Items)
    {
        ScrollBox_Items->ClearChildren();
    }

    ItemClickHandlers.Reset();
}

void USearchListWidget::RebuildFilteredItems()
{
    ClearRows();

    if (!ScrollBox_Items)
    {
        return;
    }

    for (const FSearchListEntry& Entry : Items)
    {
        if (!MatchesFilter(Entry.Label))
        {
            continue;
        }

        if (UButton* Button = BuildRowButton(FText::FromString(Entry.Label), Entry.bIsSelected, Entry.ItemId))
        {
            if (UScrollBoxSlot* ItemSlot = Cast<UScrollBoxSlot>(ScrollBox_Items->AddChild(Button)))
            {
                ItemSlot->SetHorizontalAlignment(HAlign_Fill);
                ItemSlot->SetVerticalAlignment(VAlign_Center);
                ItemSlot->SetPadding(FMargin(0.0f));
            }
        }
    }
}

bool USearchListWidget::MatchesFilter(const FString& Label) const
{
    if (!EditText_Search)
    {
        return true;
    }

    const FString Filter = EditText_Search->GetText().ToString();
    return Filter.IsEmpty() || Label.Contains(Filter, ESearchCase::IgnoreCase);
}

UButton* USearchListWidget::BuildRowButton(const FText& Label, bool bSelected, int32 ItemId)
{
    if (!WidgetTree) return nullptr;

    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());

    UInteRealThemeData* Theme = ResolveThemeData();
    const FLinearColor BodyTextColor = Theme ? Theme->Body_Text : FLinearColor::Black;
    const FLinearColor SelectedColor = Theme ? Theme->Card_BG_Tint : FLinearColor(0.89f, 0.86f, 0.80f, 1.0f);
    const FLinearColor HoverColor = Theme ? Theme->Card_Border : FLinearColor(0.95f, 0.93f, 0.90f, 1.0f);
    const FLinearColor PressedColor = Theme ? Theme->Sub_Divider : FLinearColor(0.82f, 0.78f, 0.72f, 1.0f);
    const FLinearColor TransparentColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.0f);

    auto MakeBrush = [](const FLinearColor& Color)
    {
        FSlateBrush Brush;
        Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
        Brush.TintColor = FSlateColor(Color);
        Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
        Brush.OutlineSettings.CornerRadii = FVector4(8.0f, 8.0f, 8.0f, 8.0f);
        Brush.OutlineSettings.Width = 0.0f;
        return Brush;
    };

    FButtonStyle ButtonStyle;
    ButtonStyle.Normal = MakeBrush(bSelected ? SelectedColor : TransparentColor);
    ButtonStyle.Hovered = MakeBrush(bSelected ? HoverColor : HoverColor.CopyWithNewOpacity(0.72f));
    ButtonStyle.Pressed = MakeBrush(PressedColor);
    ButtonStyle.NormalPadding = FMargin(0.0f);
    ButtonStyle.PressedPadding = FMargin(0.0f, 1.0f, 0.0f, 0.0f);
    Button->SetStyle(ButtonStyle);

    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    LabelText->SetText(Label);
    LabelText->SetColorAndOpacity(FSlateColor(BodyTextColor));
    LabelText->SetJustification(ETextJustify::Left);

    FSlateFontInfo LabelFont = LabelText->GetFont();
    LabelFont.Size = ItemFontSize;
    LabelText->SetFont(LabelFont);

    UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelText);
    LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    LabelSlot->SetHorizontalAlignment(HAlign_Left);
    LabelSlot->SetVerticalAlignment(VAlign_Center);
    LabelSlot->SetPadding(FMargin(10.0f, 3.0f, 8.0f, 3.0f));

    if (bSelected)
    {
        UTextBlock* CheckText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        CheckText->SetText(FText::FromString(TEXT("\u2713")));
        CheckText->SetColorAndOpacity(FSlateColor(Theme ? Theme->Main_Navy : FLinearColor::Black));
        FSlateFontInfo CheckFont = CheckText->GetFont();
        CheckFont.Size = SelectedMarkFontSize;
        CheckText->SetFont(CheckFont);
        UHorizontalBoxSlot* CheckSlot = Row->AddChildToHorizontalBox(CheckText);
        CheckSlot->SetVerticalAlignment(VAlign_Center);
        CheckSlot->SetPadding(FMargin(6.0f, 3.0f, 10.0f, 3.0f));
    }

    if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Button->AddChild(Row)))
    {
        ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
        ButtonSlot->SetVerticalAlignment(VAlign_Center);
        ButtonSlot->SetPadding(FMargin(0.0f));
    }

    UInteRealSearchListItemClickHandler* Handler = NewObject<UInteRealSearchListItemClickHandler>(this);
    Handler->Owner = this;
    Handler->ItemId = ItemId;
    ItemClickHandlers.Add(Handler);
    Button->OnClicked.AddDynamic(Handler, &UInteRealSearchListItemClickHandler::HandleClicked);

    return Button;
}

UInteRealThemeData* USearchListWidget::ResolveThemeData()
{
    if (!ThemeData)
    {
        ThemeData = LoadObject<UInteRealThemeData>(nullptr, TEXT("/Game/Master/Widgets/Template/DA_InteRealTheme.DA_InteRealTheme"));
    }
    return ThemeData;
}

void USearchListWidget::ApplyThemeStyle()
{
    UInteRealThemeData* Theme = ResolveThemeData();
    if (!Theme) return;

    auto ConfigureRoundedBrush = [](FSlateBrush& Brush, const FLinearColor& FillColor, const FLinearColor& StrokeColor, float Radius, float StrokeWidth)
    {
        Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
        Brush.TintColor = FSlateColor(FillColor);
        Brush.OutlineSettings.Color = FSlateColor(StrokeColor);
        Brush.OutlineSettings.Width = StrokeWidth;
        Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
        Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
    };

    if (Border_Search)
    {
        FSlateBrush SearchBrush;
        ConfigureRoundedBrush(SearchBrush, Theme->Card_BG_White, Theme->Accent_Gold, 12.0f, 1.0f);
        Border_Search->SetBrush(SearchBrush);
    }

    if (Panel_Results)
    {
        FSlateBrush PanelBrush;
        ConfigureRoundedBrush(PanelBrush, Theme->Card_BG_White, Theme->Card_Border, 12.0f, 1.0f);
        Panel_Results->SetBrush(PanelBrush);
    }

    if (EditText_Search)
    {
        FEditableTextStyle TextStyle = EditText_Search->WidgetStyle;
        TextStyle.ColorAndOpacity = FSlateColor(Theme->Main_Navy);
        EditText_Search->SetWidgetStyle(TextStyle);
    }

    if (Text_Header)
    {
        Text_Header->SetColorAndOpacity(FSlateColor(Theme->Body_Text));
    }
}
