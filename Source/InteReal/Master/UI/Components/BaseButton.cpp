#include "BaseButton.h"
#include "Components/TextBlock.h"
#include "InteReal/Master/UI/DesignTemplate/InteRealThemeData.h"

void UBaseButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (Txt_Label)
	{
		Txt_Label->SetText(ButtonText);
	}

	ApplyThemeStyle();
}

void UBaseButton::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Main)
	{
		// 버튼 클릭 이벤트 바인딩
		Btn_Main->OnClicked.AddUniqueDynamic(this, &UBaseButton::HandleButtonClicked);
	}
}

void UBaseButton::HandleButtonClicked()
{
	// 외부 델리게이트 브로드캐스트
	OnButtonClicked.Broadcast();
}

void UBaseButton::ApplyThemeStyle()
{
	if (!Btn_Main || !ThemeData)
	{
		return;
	}

	FButtonStyle NewStyle = Btn_Main->GetStyle();

	// 패딩 설정
	NewStyle.NormalPadding = ContentPadding;
	NewStyle.PressedPadding = ContentPadding;

	FLinearColor BgColor = bIsPrimary ? ThemeData->Main_Navy : ThemeData->Card_BG_White;
	FLinearColor TextColor = bIsPrimary ? ThemeData->Background_OffWhite : ThemeData->Main_Navy;
	FLinearColor StrokeColor = bIsPrimary ? ThemeData->Main_Navy : ThemeData->Card_Border;

	// 람다를 사용하여 각 상태의 브러시 설정 (Rounding 10, Stroke 적용)
	auto ConfigureBrush = [&](FSlateBrush& Brush, const FLinearColor& InBgColor)
	{
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(InBgColor);

		// 아웃라인(Stroke) 및 라운딩(Border) 설정
		// 기본적으로 Card_Border를 사용하며, 주 버튼은 배경색과 동일하게 처리
		Brush.OutlineSettings.Color = FSlateColor(StrokeColor);
		Brush.OutlineSettings.Width = 1.0f;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(10.0f, 10.0f, 10.0f, 10.0f);
	};

	ConfigureBrush(NewStyle.Normal, BgColor);
	ConfigureBrush(NewStyle.Hovered, BgColor.Desaturate(0.1f));
	ConfigureBrush(NewStyle.Pressed, BgColor.Desaturate(0.2f));

	Btn_Main->SetStyle(NewStyle);

	if (Txt_Label)
	{
		Txt_Label->SetColorAndOpacity(FSlateColor(TextColor));
	}
}
