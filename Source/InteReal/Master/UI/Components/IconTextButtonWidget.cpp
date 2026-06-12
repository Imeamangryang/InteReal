#include "IconTextButtonWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"


void UIconTextButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	
	ApplyAppearance();
}

void UIconTextButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (RootButton)
	{
		RootButton->OnClicked.AddUniqueDynamic(this, &UIconTextButtonWidget::HandleButtonClicked);
	}

	ApplyAppearance();
}

void UIconTextButtonWidget::NativeDestruct()
{
	if (RootButton)
	{
		RootButton->OnClicked.RemoveDynamic(this, &UIconTextButtonWidget::HandleButtonClicked);
	}

	Super::NativeDestruct();
}

void UIconTextButtonWidget::ApplyAppearance()
{
	if (RootButton)
	{
		RootButton->SetStyle(ButtonStyle);
	}

	if (IconImage)
	{
		if (IconTexture)
		{
			IconImage->SetBrushFromTexture(IconTexture, false);
			IconImage->SetDesiredSizeOverride(IconSize);
			IconImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			IconImage->SetVisibility(
				bHideIconWhenTextureIsNull
					? ESlateVisibility::Collapsed
					: ESlateVisibility::SelfHitTestInvisible
			);
		}
	}

	if (LabelTextBlock)
	{
		LabelTextBlock->SetText(LabelText);
		LabelTextBlock->SetColorAndOpacity(FSlateColor(TextColor));
		LabelTextBlock->SetFont(TextFont);
		LabelTextBlock->SetJustification(ETextJustify::Center);
		LabelTextBlock->SetAutoWrapText(false);
		LabelTextBlock->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void UIconTextButtonWidget::SetButtonId(FName InButtonId)
{
	ButtonId = InButtonId;
}

void UIconTextButtonWidget::SetLabelText(const FText& InText)
{
	LabelText = InText;
	ApplyAppearance();
}

void UIconTextButtonWidget::SetIconTexture(UTexture2D* InTexture)
{
	IconTexture = InTexture;
	ApplyAppearance();
}

void UIconTextButtonWidget::SetIconSize(FVector2D InSize)
{
	IconSize = InSize;
	ApplyAppearance();
}

void UIconTextButtonWidget::SetButtonEnabled(bool bEnabled)
{
	SetIsEnabled(bEnabled);

	if (RootButton)
	{
		RootButton->SetIsEnabled(bEnabled);
	}
}

void UIconTextButtonWidget::HandleButtonClicked()
{
	OnIconTextButtonClicked.Broadcast(ButtonId, this);
}