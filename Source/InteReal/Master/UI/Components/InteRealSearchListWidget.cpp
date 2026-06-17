#include "InteRealSearchListWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Input/Reply.h"
#include "InteReal/Master/UI/DesignTemplate/InteRealThemeData.h"
#include "InteReal/Network/ViewModel/InteRealPlanViewModel.h"
#include "Styling/SlateTypes.h"
#include "UObject/UObjectIterator.h"

UInteRealSearchListWidget::UInteRealSearchListWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetIsFocusable(true);
    PlanHeaderText = FText::FromString(TEXT("\uCD5C\uADFC \uB3C4\uBA74"));
    DeltaHeaderText = FText::FromString(TEXT("Delta Versions"));
    SearchHintText = FText::FromString(TEXT("Search"));
}

void UInteRealSearchListWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    EnsureDefaultWidgetTree();
    ApplyThemeStyle();
    UpdateHeaderText();

    if (EditText_Search)
    {
        EditText_Search->SetHintText(SearchHintText);
    }
}

void UInteRealSearchListWidget::NativeConstruct()
{
    Super::NativeConstruct();

    EnsureDefaultWidgetTree();
    ApplyThemeStyle();

    if (EditText_Search)
    {
        EditText_Search->OnTextChanged.AddUniqueDynamic(this, &UInteRealSearchListWidget::HandleSearchTextChanged);
        EditText_Search->SetHintText(SearchHintText);
    }

    SetResultsVisible(false);
    BindViewModelEvents();
}

void UInteRealSearchListWidget::NativeDestruct()
{
    UnbindViewModelEvents();

    if (EditText_Search)
    {
        EditText_Search->OnTextChanged.RemoveDynamic(this, &UInteRealSearchListWidget::HandleSearchTextChanged);
    }

    Super::NativeDestruct();
}

void UInteRealSearchListWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    const bool bIsInputFocused = EditText_Search && EditText_Search->HasKeyboardFocus();
    if (bIsInputFocused && !bWasInputFocused)
    {
        HandleSearchFocused();
    }

    bWasInputFocused = bIsInputFocused;
}

FReply UInteRealSearchListWidget::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
    HandleSearchFocused();
    return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
}

void UInteRealSearchListWidget::SetViewModel(UInteRealPlanViewModel* InViewModel)
{
    if (PlanViewModel == InViewModel)
    {
        return;
    }

    UnbindViewModelEvents();
    PlanViewModel = InViewModel;
    BindViewModelEvents();
}

void UInteRealSearchListWidget::SetupForPlanSearch(UInteRealPlanViewModel* InViewModel)
{
    SetViewModel(InViewModel);
    SetMode(EInteRealSearchListMode::Plan);
}

void UInteRealSearchListWidget::SetupForDeltaVersionSearch(int32 InPlanId, UInteRealPlanViewModel* InViewModel)
{
    SetViewModel(InViewModel);
    SetDeltaPlanId(InPlanId);
    SetMode(EInteRealSearchListMode::DeltaVersion);
}

void UInteRealSearchListWidget::SetMode(EInteRealSearchListMode InMode)
{
    if (Mode == InMode)
    {
        return;
    }

    Mode = InMode;
    ClearSelection();
    UpdateHeaderText();
    RebuildRows();
}

void UInteRealSearchListWidget::SetDeltaPlanId(int32 InPlanId)
{
    if (DeltaPlanId == InPlanId)
    {
        return;
    }

    DeltaPlanId = InPlanId;
    bHasLoadedDeltaVersions = false;
    CachedDeltaVersions.Reset();

    if (Mode == EInteRealSearchListMode::DeltaVersion)
    {
        RebuildRows();
    }
}

void UInteRealSearchListWidget::RefreshList()
{
    UInteRealPlanViewModel* ViewModel = ResolveViewModel();
    if (!ViewModel)
    {
        return;
    }

    if (Mode == EInteRealSearchListMode::Plan)
    {
        FUnrealPlanSearchParams Params;
        Params.limit = SearchLimit;
        Params.executable_only = true;
        ViewModel->FetchExecutablePlanList(Params);
    }
    else
    {
        int32 PlanId = DeltaPlanId;
        if (PlanId == 0)
        {
            PlanId = ViewModel->GetCurrentPlan().id;
        }

        if (PlanId != 0)
        {
            DeltaPlanId = PlanId;
            ViewModel->FetchDeltaVersionList(PlanId);
        }
    }
}

void UInteRealSearchListWidget::OpenRecentList()
{
    SetResultsVisible(true);

    const bool bNeedsLoad = Mode == EInteRealSearchListMode::Plan ? !bHasLoadedPlans : !bHasLoadedDeltaVersions;
    if (bNeedsLoad)
    {
        RefreshList();
    }
    else
    {
        RebuildRows();
    }
}

void UInteRealSearchListWidget::ClearSelection()
{
    SelectedPlanId = 0;
    SelectedDeltaVersion = INDEX_NONE;
}

void UInteRealSearchListWidget::HandleItemClicked(int32 ItemIndex)
{
    UInteRealPlanViewModel* ViewModel = ResolveViewModel();

    if (Mode == EInteRealSearchListMode::Plan)
    {
        if (!CachedPlans.IsValidIndex(ItemIndex))
        {
            return;
        }

        const FUnrealPlanItem PlanItem = CachedPlans[ItemIndex];
        SelectedPlanId = PlanItem.id;
        OnPlanSelected.Broadcast(PlanItem);

        if (EditText_Search)
        {
            EditText_Search->SetText(FText::FromString(PlanItem.GetDisplayTitle()));
        }

        if (bAutoLoadOnSelection && ViewModel)
        {
            ViewModel->LoadPlanTopology(PlanItem);
        }
    }
    else
    {
        if (!CachedDeltaVersions.IsValidIndex(ItemIndex))
        {
            return;
        }

        const FUnrealDeltaVersionItem VersionItem = CachedDeltaVersions[ItemIndex];
        SelectedDeltaVersion = VersionItem.version;
        OnDeltaVersionSelected.Broadcast(VersionItem);

        if (EditText_Search)
        {
            EditText_Search->SetText(FText::FromString(VersionItem.GetDisplayTitle()));
        }

        if (bAutoLoadOnSelection && ViewModel)
        {
            ViewModel->LoadDeltaVersion(VersionItem);
        }
    }

    RebuildRows();
}

void UInteRealSearchListWidget::HandleSearchTextChanged(const FText& Text)
{
    SetResultsVisible(true);

    const bool bNeedsLoad = Mode == EInteRealSearchListMode::Plan ? !bHasLoadedPlans : !bHasLoadedDeltaVersions;
    if (bNeedsLoad)
    {
        RefreshList();
    }

    RebuildRows();
}

void UInteRealSearchListWidget::HandlePlanListUpdated(bool bSuccess, const FUnrealPlanListResponse& Response)
{
    if (!bSuccess)
    {
        return;
    }

    CachedPlans = Response.items;
    bHasLoadedPlans = true;

    if (Mode == EInteRealSearchListMode::Plan)
    {
        SetResultsVisible(true);
        RebuildRows();
    }
}

void UInteRealSearchListWidget::HandleDeltaVersionListUpdated(bool bSuccess, const FUnrealDeltaVersionListResponse& Response)
{
    if (!bSuccess)
    {
        return;
    }

    CachedDeltaVersions = Response.items;
    bHasLoadedDeltaVersions = true;

    if (Mode == EInteRealSearchListMode::DeltaVersion)
    {
        SetResultsVisible(true);
        RebuildRows();
    }
}

void UInteRealSearchListWidget::EnsureDefaultWidgetTree()
{
    if ((EditText_Search && ScrollBox_Items) || !WidgetTree || WidgetTree->RootWidget)
    {
        return;
    }

    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    Border_Search = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_Search"));
    Border_Search->SetPadding(FMargin(20.0f, 14.0f));

    FSlateBrush InputBrush;
    InputBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
    InputBrush.TintColor = FSlateColor(FLinearColor::White);
    InputBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.64f, 0.59f, 0.52f, 1.0f));
    InputBrush.OutlineSettings.Width = 1.0f;
    InputBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
    InputBrush.OutlineSettings.CornerRadii = FVector4(12.0f, 12.0f, 12.0f, 12.0f);
    Border_Search->SetBrush(InputBrush);

    EditText_Search = WidgetTree->ConstructWidget<UEditableText>(UEditableText::StaticClass(), TEXT("EditText_Search"));
    Border_Search->SetContent(EditText_Search);

    UVerticalBoxSlot* InputSlot = Root->AddChildToVerticalBox(Border_Search);
    InputSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

    Panel_Results = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel_Results"));
    Panel_Results->SetPadding(FMargin(20.0f, 18.0f));

    FSlateBrush PanelBrush;
    PanelBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
    PanelBrush.TintColor = FSlateColor(FLinearColor::White);
    PanelBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.9f, 0.88f, 0.84f, 1.0f));
    PanelBrush.OutlineSettings.Width = 1.0f;
    PanelBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
    PanelBrush.OutlineSettings.CornerRadii = FVector4(12.0f, 12.0f, 12.0f, 12.0f);
    Panel_Results->SetBrush(PanelBrush);

    UVerticalBox* PanelContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PanelContent"));
    Text_Header = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Header"));
    ScrollBox_Items = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ScrollBox_Items"));

    PanelContent->AddChildToVerticalBox(Text_Header);
    PanelContent->AddChildToVerticalBox(ScrollBox_Items);
    Panel_Results->SetContent(PanelContent);
    Root->AddChildToVerticalBox(Panel_Results);
}

UInteRealThemeData* UInteRealSearchListWidget::ResolveThemeData()
{
    if (!ThemeData)
    {
        ThemeData = LoadObject<UInteRealThemeData>(nullptr, TEXT("/Game/Master/Widgets/Template/DA_InteRealTheme.DA_InteRealTheme"));
    }

    return ThemeData;
}

void UInteRealSearchListWidget::ApplyThemeStyle()
{
    UInteRealThemeData* Theme = ResolveThemeData();
    if (!Theme)
    {
        return;
    }

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

    UpdateHeaderText();
}

void UInteRealSearchListWidget::BindViewModelEvents()
{
    UInteRealPlanViewModel* ViewModel = ResolveViewModel();
    if (!ViewModel)
    {
        return;
    }

    ViewModel->OnPlanListUpdated.AddUniqueDynamic(this, &UInteRealSearchListWidget::HandlePlanListUpdated);
    ViewModel->OnDeltaVersionListUpdated.AddUniqueDynamic(this, &UInteRealSearchListWidget::HandleDeltaVersionListUpdated);
}

void UInteRealSearchListWidget::UnbindViewModelEvents()
{
    if (!PlanViewModel)
    {
        return;
    }

    PlanViewModel->OnPlanListUpdated.RemoveDynamic(this, &UInteRealSearchListWidget::HandlePlanListUpdated);
    PlanViewModel->OnDeltaVersionListUpdated.RemoveDynamic(this, &UInteRealSearchListWidget::HandleDeltaVersionListUpdated);
}

UInteRealPlanViewModel* UInteRealSearchListWidget::ResolveViewModel()
{
    if (PlanViewModel)
    {
        return PlanViewModel;
    }

    if (GetWorld())
    {
        for (TObjectIterator<UInteRealPlanViewModel> It; It; ++It)
        {
            if (It->GetWorld() == GetWorld())
            {
                PlanViewModel = *It;
                return PlanViewModel;
            }
        }
    }

    PlanViewModel = NewObject<UInteRealPlanViewModel>(this);
    return PlanViewModel;
}

void UInteRealSearchListWidget::HandleSearchFocused()
{
    OpenRecentList();
}

void UInteRealSearchListWidget::RebuildRows()
{
    if (!ScrollBox_Items)
    {
        return;
    }

    ScrollBox_Items->ClearChildren();
    ItemClickHandlers.Reset();
    UpdateHeaderText();

    if (Mode == EInteRealSearchListMode::Plan)
    {
        for (int32 Index = 0; Index < CachedPlans.Num(); ++Index)
        {
            const FUnrealPlanItem& PlanItem = CachedPlans[Index];
            if (MatchesFilter(PlanItem.GetDisplayTitle()))
            {
                AddPlanRow(PlanItem, Index);
            }
        }
    }
    else
    {
        for (int32 Index = 0; Index < CachedDeltaVersions.Num(); ++Index)
        {
            const FUnrealDeltaVersionItem& VersionItem = CachedDeltaVersions[Index];
            if (MatchesFilter(VersionItem.GetDisplayTitle()))
            {
                AddDeltaVersionRow(VersionItem, Index);
            }
        }
    }
}

void UInteRealSearchListWidget::AddPlanRow(const FUnrealPlanItem& PlanItem, int32 ItemIndex)
{
    const bool bSelected = PlanItem.id != 0 && PlanItem.id == SelectedPlanId;
    if (UButton* Button = BuildRowButton(FText::FromString(PlanItem.GetDisplayTitle()), bSelected, ItemIndex))
    {
        ScrollBox_Items->AddChild(Button);
    }
}

void UInteRealSearchListWidget::AddDeltaVersionRow(const FUnrealDeltaVersionItem& VersionItem, int32 ItemIndex)
{
    const bool bSelected = VersionItem.version == SelectedDeltaVersion;
    if (UButton* Button = BuildRowButton(FText::FromString(VersionItem.GetDisplayTitle()), bSelected, ItemIndex))
    {
        ScrollBox_Items->AddChild(Button);
    }
}

UButton* UInteRealSearchListWidget::BuildRowButton(const FText& Label, bool bSelected, int32 ItemIndex)
{
    if (!WidgetTree)
    {
        return nullptr;
    }

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

    const FLinearColor NormalColor = bSelected ? SelectedColor : TransparentColor;
    const FLinearColor RowHoverColor = bSelected ? HoverColor : HoverColor.CopyWithNewOpacity(0.72f);

    FButtonStyle ButtonStyle;
    ButtonStyle.Normal = MakeBrush(NormalColor);
    ButtonStyle.Hovered = MakeBrush(RowHoverColor);
    ButtonStyle.Pressed = MakeBrush(PressedColor);
    ButtonStyle.NormalPadding = FMargin(12.0f, 8.0f);
    ButtonStyle.PressedPadding = FMargin(12.0f, 9.0f, 12.0f, 7.0f);
    Button->SetStyle(ButtonStyle);

    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    LabelText->SetText(Label);
    LabelText->SetColorAndOpacity(FSlateColor(BodyTextColor));

    FSlateFontInfo LabelFont = LabelText->GetFont();
    LabelFont.Size = 24;
    LabelText->SetFont(LabelFont);

    UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelText);
    LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    LabelSlot->SetPadding(FMargin(8.0f, 6.0f));

    if (bSelected)
    {
        UTextBlock* CheckText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        CheckText->SetText(FText::FromString(TEXT("\u2713")));
        CheckText->SetColorAndOpacity(FSlateColor(Theme ? Theme->Main_Navy : FLinearColor::Black));

        FSlateFontInfo CheckFont = CheckText->GetFont();
        CheckFont.Size = 24;
        CheckText->SetFont(CheckFont);

        UHorizontalBoxSlot* CheckSlot = Row->AddChildToHorizontalBox(CheckText);
        CheckSlot->SetPadding(FMargin(12.0f, 6.0f, 10.0f, 6.0f));
    }

    Button->AddChild(Row);

    UInteRealSearchListItemClickHandler* Handler = NewObject<UInteRealSearchListItemClickHandler>(this);
    Handler->Owner = this;
    Handler->ItemIndex = ItemIndex;
    ItemClickHandlers.Add(Handler);
    Button->OnClicked.AddDynamic(Handler, &UInteRealSearchListItemClickHandler::HandleClicked);

    return Button;
}

bool UInteRealSearchListWidget::MatchesFilter(const FString& Title) const
{
    const FString SearchString = GetSearchString();
    return SearchString.IsEmpty() || Title.Contains(SearchString, ESearchCase::IgnoreCase);
}

FString UInteRealSearchListWidget::GetSearchString() const
{
    return EditText_Search ? EditText_Search->GetText().ToString().TrimStartAndEnd() : FString();
}

void UInteRealSearchListWidget::SetResultsVisible(bool bVisible)
{
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

void UInteRealSearchListWidget::UpdateHeaderText()
{
    if (!Text_Header)
    {
        return;
    }

    Text_Header->SetText(Mode == EInteRealSearchListMode::Plan ? PlanHeaderText : DeltaHeaderText);
    if (UInteRealThemeData* Theme = ResolveThemeData())
    {
        Text_Header->SetColorAndOpacity(FSlateColor(Theme->Body_Text));
    }
    else
    {
        Text_Header->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
    }

    FSlateFontInfo HeaderFont = Text_Header->GetFont();
    HeaderFont.Size = 22;
    Text_Header->SetFont(HeaderFont);
}

void UInteRealSearchListItemClickHandler::HandleClicked()
{
    if (Owner)
    {
        Owner->HandleItemClicked(ItemIndex);
    }
}
