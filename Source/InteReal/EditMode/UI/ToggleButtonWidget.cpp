#include "ToggleButtonWidget.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Image.h"

void UToggleButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Icon)
	{
		Btn_Icon->OnClicked.AddDynamic(this, &UToggleButtonWidget::HandleClicked);
		Btn_Icon->OnHovered.AddDynamic(this, &UToggleButtonWidget::HandleHovered);
		Btn_Icon->OnUnhovered.AddDynamic(this, &UToggleButtonWidget::HandleUnhovered);
		Btn_Icon->OnPressed.AddDynamic(this, &UToggleButtonWidget::HandlePressed);
		Btn_Icon->OnReleased.AddDynamic(this, &UToggleButtonWidget::HandleReleased);
	}

	if (IconTexture)
	{
		SetIconAndLabel(IconTexture, FText::GetEmpty());
	}

	RefreshBorderColor();
}

void UToggleButtonWidget::SetActiveVisual(bool bActive)
{
	bIsActive = bActive;
	RefreshBorderColor();
}

void UToggleButtonWidget::SetIconAndLabel(UTexture2D* NewIcon, const FText& NewLabel)
{
	if (Img_Icon && NewIcon)
	{
		Img_Icon->SetBrushFromTexture(NewIcon);
	}
}

void UToggleButtonWidget::RefreshBorderColor()
{
	if (!B_Color)
	{
		return;
	}

	FLinearColor Color;
	if (bIsPressed)
	{
		Color = PressedColor;
	}
	else if (bIsActive)
	{
		Color = ActiveColor;
	}
	else if (bIsHovered)
	{
		Color = HoverColor;
	}
	else
	{
		Color = InactiveColor;
	}

	B_Color->SetBrushColor(Color);
}

void UToggleButtonWidget::HandleClicked()
{
	OnToggled.Broadcast();
}

void UToggleButtonWidget::HandleHovered()
{
	bIsHovered = true;
	RefreshBorderColor();
}

void UToggleButtonWidget::HandleUnhovered()
{
	bIsHovered = false;
	RefreshBorderColor();
}

void UToggleButtonWidget::HandlePressed()
{
	bIsPressed = true;
	RefreshBorderColor();
}

void UToggleButtonWidget::HandleReleased()
{
	bIsPressed = false;
	RefreshBorderColor();
}
