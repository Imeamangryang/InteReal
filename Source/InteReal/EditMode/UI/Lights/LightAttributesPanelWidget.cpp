#include "LightAttributesPanelWidget.h"
#include "ColorWheelWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Styling/SlateBrush.h"
#include "InteReal/Master/UI/Components/BaseSlider.h"
#include "InteReal/Master/UI/Components/BaseInput.h"
#include "InteReal/Master/UI/Components/BaseToggleSwitch.h"
#include "InteReal/Master/UI/Components/BaseButton.h"
#include "InteReal/EditMode/Furniture/Furniture.h"
#include "InteReal/EditMode/Furniture/LightFixture.h"

void ULightAttributesPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ColorWheel)
	{
		ColorWheel->OnValueChanged.AddDynamic(this, &ULightAttributesPanelWidget::HandleColorChanged);
	}
	if (Slider_Intensity)
	{
		Slider_Intensity->OnBaseValueChanged.AddDynamic(this, &ULightAttributesPanelWidget::HandleIntensityChanged);
	}
	if (Slider_Radius)
	{
		Slider_Radius->OnBaseValueChanged.AddDynamic(this, &ULightAttributesPanelWidget::HandleRadiusChanged);
	}
	if (Toggle_Enabled)
	{
		Toggle_Enabled->OnToggleChanged.AddDynamic(this, &ULightAttributesPanelWidget::HandleEnabledToggled);
	}
	if (Input_IntensityValue)
	{
		Input_IntensityValue->OnBaseTextCommitted.AddDynamic(this, &ULightAttributesPanelWidget::HandleIntensityInputCommitted);
	}
	if (Input_RadiusValue)
	{
		Input_RadiusValue->OnBaseTextCommitted.AddDynamic(this, &ULightAttributesPanelWidget::HandleRadiusInputCommitted);
	}
	if (Btn_Cancel)
	{
		Btn_Cancel->OnButtonClicked.AddDynamic(this, &ULightAttributesPanelWidget::HandleCancelClicked);
	}
	if (Btn_Apply)
	{
		Btn_Apply->OnButtonClicked.AddDynamic(this, &ULightAttributesPanelWidget::HandleApplyClicked);
	}
	if (Btn_Close)
	{
		Btn_Close->OnClicked.AddDynamic(this, &ULightAttributesPanelWidget::HandleCancelClicked);
	}
	if (Input_Hex)
	{
		Input_Hex->OnBaseTextCommitted.AddDynamic(this, &ULightAttributesPanelWidget::HandleHexTextCommitted);
	}
	if (Input_R)
	{
		Input_R->OnBaseTextCommitted.AddDynamic(this, &ULightAttributesPanelWidget::HandleRInputCommitted);
	}
	if (Input_G)
	{
		Input_G->OnBaseTextCommitted.AddDynamic(this, &ULightAttributesPanelWidget::HandleGInputCommitted);
	}
	if (Input_B)
	{
		Input_B->OnBaseTextCommitted.AddDynamic(this, &ULightAttributesPanelWidget::HandleBInputCommitted);
	}
}

void ULightAttributesPanelWidget::RefreshForFurniture(AFurniture* Furniture)
{
	TargetLight = Cast<ALightFixture>(Furniture);
	ALightFixture* Light = TargetLight.Get();
	if (!Light)
	{
		return;
	}

	const FLightAttributes Attributes = Light->GetLightAttributes();
	SnapshotAttributes = Attributes;

	if (ColorWheel)
	{
		ColorWheel->SetSelectedColor(Attributes.LightColor);
	}
	RefreshColorReadouts(Attributes.LightColor);
	if (Slider_Intensity)
	{
		Slider_Intensity->SetValue(Attributes.LightIntensity);
	}
	if (Input_IntensityValue && Input_IntensityValue->Input_Main)
	{
		Input_IntensityValue->Input_Main->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), Attributes.LightIntensity)));
	}
	if (Slider_Radius)
	{
		Slider_Radius->SetValue(Attributes.AttenuationRadius);
	}
	if (Input_RadiusValue && Input_RadiusValue->Input_Main)
	{
		Input_RadiusValue->Input_Main->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), Attributes.AttenuationRadius)));
	}
	if (Toggle_Enabled)
	{
		Toggle_Enabled->SetIsOn(Attributes.bEmitsLight, false);
	}
	if (Text_OnOff)
	{
		Text_OnOff->SetText(Attributes.bEmitsLight ? FText::FromString(TEXT("켜짐")) : FText::FromString(TEXT("꺼짐")));
	}
}

void ULightAttributesPanelWidget::HandleColorChanged(FLinearColor NewColor)
{
	RefreshColorReadouts(NewColor);
	ApplyToTarget();
}

void ULightAttributesPanelWidget::HandleHexTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	FString HexString = NewText.ToString().TrimStartAndEnd();
	HexString.RemoveFromStart(TEXT("#"));

	bool bIsValidHex = HexString.Len() == 6;
	for (const TCHAR Char : HexString)
	{
		bIsValidHex &= FChar::IsHexDigit(Char);
	}

	if (!bIsValidHex)
	{
		RefreshColorReadouts(ColorWheel ? ColorWheel->SelectedColor : FLinearColor::White);
		return;
	}
	
	const FColor ParsedSRgb = FColor::FromHex(TEXT("#") + HexString);
	const FLinearColor NewColor(ParsedSRgb);

	if (ColorWheel)
	{
		ColorWheel->SetSelectedColor(NewColor);
	}
	RefreshColorReadouts(NewColor);
	ApplyToTarget();
}

namespace
{
	int32 ExtractDigits(const FString& Text)
	{
		FString Digits;
		for (const TCHAR Char : Text)
		{
			if (FChar::IsDigit(Char))
			{
				Digits.AppendChar(Char);
			}
		}
		return Digits.IsEmpty() ? 0 : FCString::Atoi(*Digits);
	}
}

void ULightAttributesPanelWidget::HandleRInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	const FColor Current = (ColorWheel ? ColorWheel->SelectedColor : FLinearColor::White).ToFColor(true);
	const uint8 NewR = static_cast<uint8>(FMath::Clamp(ExtractDigits(Text.ToString()), 0, 255));
	ApplyRgbChannelChange(NewR, Current.G, Current.B);
}

void ULightAttributesPanelWidget::HandleGInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	const FColor Current = (ColorWheel ? ColorWheel->SelectedColor : FLinearColor::White).ToFColor(true);
	const uint8 NewG = static_cast<uint8>(FMath::Clamp(ExtractDigits(Text.ToString()), 0, 255));
	ApplyRgbChannelChange(Current.R, NewG, Current.B);
}

void ULightAttributesPanelWidget::HandleBInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	const FColor Current = (ColorWheel ? ColorWheel->SelectedColor : FLinearColor::White).ToFColor(true);
	const uint8 NewB = static_cast<uint8>(FMath::Clamp(ExtractDigits(Text.ToString()), 0, 255));
	ApplyRgbChannelChange(Current.R, Current.G, NewB);
}

void ULightAttributesPanelWidget::ApplyRgbChannelChange(uint8 NewR, uint8 NewG, uint8 NewB)
{
	const FLinearColor NewColor(FColor(NewR, NewG, NewB));

	if (ColorWheel)
	{
		ColorWheel->SetSelectedColor(NewColor);
	}
	RefreshColorReadouts(NewColor);
	ApplyToTarget();
}

void ULightAttributesPanelWidget::RefreshColorReadouts(const FLinearColor& Color)
{
	const FColor SRgb = Color.ToFColor(true);

	if (Input_Hex && Input_Hex->Input_Main)
	{
		// "#"는 BaseInput의 고정 접두 라벨(PrefixText)이 보여주므로 여기서는 16진수 6자리만 넣는다
		Input_Hex->Input_Main->SetText(FText::FromString(FString::Printf(TEXT("%02X%02X%02X"), SRgb.R, SRgb.G, SRgb.B)));
	}
	if (Input_R && Input_R->Input_Main)
	{
		Input_R->Input_Main->SetText(FText::FromString(FString::Printf(TEXT("%d"), SRgb.R)));
	}
	if (Input_G && Input_G->Input_Main)
	{
		Input_G->Input_Main->SetText(FText::FromString(FString::Printf(TEXT("%d"), SRgb.G)));
	}
	if (Input_B && Input_B->Input_Main)
	{
		Input_B->Input_Main->SetText(FText::FromString(FString::Printf(TEXT("%d"), SRgb.B)));
	}
	if (Border_ColorSwatch)
	{
		FSlateBrush SwatchBrush = Border_ColorSwatch->Background;
		SwatchBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		SwatchBrush.TintColor = FSlateColor(Color);
		SwatchBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		SwatchBrush.OutlineSettings.CornerRadii = FVector4(5.0, 5.0, 5.0, 5.0);
		Border_ColorSwatch->SetBrush(SwatchBrush);
	}
}

void ULightAttributesPanelWidget::HandleIntensityChanged(float NewValue)
{
	if (Input_IntensityValue && Input_IntensityValue->Input_Main)
	{
		Input_IntensityValue->Input_Main->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), NewValue)));
	}
	ApplyToTarget();
}

void ULightAttributesPanelWidget::HandleRadiusChanged(float NewValue)
{
	if (Input_RadiusValue && Input_RadiusValue->Input_Main)
	{
		Input_RadiusValue->Input_Main->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), NewValue)));
	}
	ApplyToTarget();
}

void ULightAttributesPanelWidget::HandleEnabledToggled(bool bIsOn)
{
	if (Text_OnOff)
	{
		Text_OnOff->SetText(bIsOn ? FText::FromString(TEXT("켜짐")) : FText::FromString(TEXT("꺼짐")));
	}
	ApplyToTarget();
}

void ULightAttributesPanelWidget::HandleIntensityInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	const float NewValue = FCString::Atof(*Text.ToString());
	if (Slider_Intensity)
	{
		Slider_Intensity->SetValue(NewValue);
	}
	if (Input_IntensityValue && Input_IntensityValue->Input_Main)
	{
		Input_IntensityValue->Input_Main->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), NewValue)));
	}
	ApplyToTarget();
}

void ULightAttributesPanelWidget::HandleRadiusInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	const float NewValue = FCString::Atof(*Text.ToString());
	if (Slider_Radius)
	{
		Slider_Radius->SetValue(NewValue);
	}
	if (Input_RadiusValue && Input_RadiusValue->Input_Main)
	{
		Input_RadiusValue->Input_Main->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), NewValue)));
	}
	ApplyToTarget();
}

void ULightAttributesPanelWidget::HandleCancelClicked()
{
	if (ALightFixture* Light = TargetLight.Get())
	{
		Light->SetLightAttributes(SnapshotAttributes);
	}

	SetVisibility(ESlateVisibility::Hidden);
}

void ULightAttributesPanelWidget::HandleApplyClicked()
{
	if (ALightFixture* Light = TargetLight.Get())
	{
		SnapshotAttributes = Light->GetLightAttributes();
	}
}

void ULightAttributesPanelWidget::ApplyToTarget()
{
	ALightFixture* Light = TargetLight.Get();
	if (!Light)
	{
		return;
	}

	FLightAttributes Attributes;

	Attributes.LightFixtureType = Light->GetLightAttributes().LightFixtureType;
	Attributes.bEmitsLight = Toggle_Enabled && Toggle_Enabled->bIsOn;
	Attributes.LightColor = ColorWheel ? ColorWheel->SelectedColor : FLinearColor::White;
	Attributes.LightIntensity = Slider_Intensity ? Slider_Intensity->GetValue() : 8.0f;
	Attributes.AttenuationRadius = Slider_Radius ? Slider_Radius->GetValue() : 1000.0f;

	Light->SetLightAttributes(Attributes);
}
