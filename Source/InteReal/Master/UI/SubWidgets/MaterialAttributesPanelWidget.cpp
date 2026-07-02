#include "MaterialAttributesPanelWidget.h"

#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "InteReal/Master/UI/Components/BaseButton.h"
#include "InteReal/Master/UI/Components/BaseInput.h"
#include "InteReal/Master/UI/Components/BaseSlider.h"
#include "InteReal/SubSystems/InteRealUISubSystem.h"

void UMaterialAttributesPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Slider_Metallic)
	{
		Slider_Metallic->OnBaseValueChanged.AddDynamic(this, &UMaterialAttributesPanelWidget::HandleMetallicChanged);
	}
	if (Slider_Specular)
	{
		Slider_Specular->OnBaseValueChanged.AddDynamic(this, &UMaterialAttributesPanelWidget::HandleSpecularChanged);
	}
	if (Slider_Roughness)
	{
		Slider_Roughness->OnBaseValueChanged.AddDynamic(this, &UMaterialAttributesPanelWidget::HandleRoughnessChanged);
	}
	if (Slider_Emissive)
	{
		Slider_Emissive->OnBaseValueChanged.AddDynamic(this, &UMaterialAttributesPanelWidget::HandleEmissiveChanged);
	}
	if (Input_MetallicValue)
	{
		Input_MetallicValue->OnBaseTextCommitted.AddDynamic(this, &UMaterialAttributesPanelWidget::HandleMetallicInputCommitted);
	}
	if (Input_SpecularValue)
	{
		Input_SpecularValue->OnBaseTextCommitted.AddDynamic(this, &UMaterialAttributesPanelWidget::HandleSpecularInputCommitted);
	}
	if (Input_RoughnessValue)
	{
		Input_RoughnessValue->OnBaseTextCommitted.AddDynamic(this, &UMaterialAttributesPanelWidget::HandleRoughnessInputCommitted);
	}
	if (Input_EmissiveValue)
	{
		Input_EmissiveValue->OnBaseTextCommitted.AddDynamic(this, &UMaterialAttributesPanelWidget::HandleEmissiveInputCommitted);
	}
	if (Btn_Cancel)
	{
		Btn_Cancel->OnButtonClicked.AddDynamic(this, &UMaterialAttributesPanelWidget::HandleCancelClicked);
	}
	if (Btn_Apply)
	{
		Btn_Apply->OnButtonClicked.AddDynamic(this, &UMaterialAttributesPanelWidget::HandleApplyClicked);
	}
	if (Btn_Close)
	{
		Btn_Close->OnClicked.AddDynamic(this, &UMaterialAttributesPanelWidget::HandleCancelClicked);
	}
	if (Slider_TextureTiling)
	{
		Slider_TextureTiling->OnBaseValueChanged.AddDynamic(this, &UMaterialAttributesPanelWidget::HandleTextureTilingChanged);
	}
	if (Input_TextureTilingValue)
	{
		Input_TextureTilingValue->OnBaseTextCommitted.AddDynamic(this, &UMaterialAttributesPanelWidget::HandleTextureTilingInputCommitted);
	}
	
	InitializeFromInputValues();
}

void UMaterialAttributesPanelWidget::RefreshForMaterial(const FMaterialDataRow& MaterialData)
{
	CurrentMaterialData = MaterialData;
	SnapshotMaterialData = MaterialData;
	bHasMaterialData = true;

	bIsRefreshing = true;

	SetSliderValueSilently(Slider_Metallic, CurrentMaterialData.Metallic);
	SetSliderValueSilently(Slider_Specular, CurrentMaterialData.Specular);
	SetSliderValueSilently(Slider_Roughness, CurrentMaterialData.Roughness);
	SetSliderValueSilently(Slider_Emissive, CurrentMaterialData.Emissive);
	SetSliderValueSilently(Slider_TextureTiling, CurrentMaterialData.TextureTiling);

	RefreshValueReadouts();

	bIsRefreshing = false;
}

void UMaterialAttributesPanelWidget::HandleMetallicChanged(float NewValue)
{
	if (bIsRefreshing) return;

	CurrentMaterialData.Metallic = NewValue;
	SetInputText(Input_MetallicValue, NewValue);
	ApplyToSelectedSurface();
}

void UMaterialAttributesPanelWidget::HandleSpecularChanged(float NewValue)
{
	if (bIsRefreshing) return;

	CurrentMaterialData.Specular = NewValue;
	SetInputText(Input_SpecularValue, NewValue);
	ApplyToSelectedSurface();
}

void UMaterialAttributesPanelWidget::HandleRoughnessChanged(float NewValue)
{
	if (bIsRefreshing) return;

	CurrentMaterialData.Roughness = NewValue;
	SetInputText(Input_RoughnessValue, NewValue);
	ApplyToSelectedSurface();
}

void UMaterialAttributesPanelWidget::HandleEmissiveChanged(float NewValue)
{
	if (bIsRefreshing) return;

	CurrentMaterialData.Emissive = NewValue;
	SetInputText(Input_EmissiveValue, NewValue);
	ApplyToSelectedSurface();
}

void UMaterialAttributesPanelWidget::HandleTextureTilingChanged(float NewValue)
{
	if (bIsRefreshing) return;

	CurrentMaterialData.TextureTiling = FMath::Max(NewValue, 0.01f);
	SetInputText(Input_TextureTilingValue, CurrentMaterialData.TextureTiling);
	ApplyToSelectedSurface();
}

void UMaterialAttributesPanelWidget::HandleMetallicInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	const float NewValue = FMath::Clamp(FCString::Atof(*Text.ToString()), 0.0f, 1.0f);
	CurrentMaterialData.Metallic = NewValue;

	bIsRefreshing = true;
	SetSliderValueSilently(Slider_Metallic, NewValue);
	SetInputText(Input_MetallicValue, NewValue);
	bIsRefreshing = false;

	ApplyToSelectedSurface();
}

void UMaterialAttributesPanelWidget::HandleSpecularInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	const float NewValue = FMath::Clamp(FCString::Atof(*Text.ToString()), 0.0f, 1.0f);
	CurrentMaterialData.Specular = NewValue;

	bIsRefreshing = true;
	SetSliderValueSilently(Slider_Specular, NewValue);
	SetInputText(Input_SpecularValue, NewValue);
	bIsRefreshing = false;

	ApplyToSelectedSurface();
}

void UMaterialAttributesPanelWidget::HandleRoughnessInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	const float NewValue = FMath::Clamp(FCString::Atof(*Text.ToString()), 0.0f, 1.0f);
	CurrentMaterialData.Roughness = NewValue;

	bIsRefreshing = true;
	SetSliderValueSilently(Slider_Roughness, NewValue);
	SetInputText(Input_RoughnessValue, NewValue);
	bIsRefreshing = false;

	ApplyToSelectedSurface();
}

void UMaterialAttributesPanelWidget::HandleEmissiveInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	const float NewValue = FMath::Max(FCString::Atof(*Text.ToString()), 0.0f);
	CurrentMaterialData.Emissive = NewValue;

	bIsRefreshing = true;
	SetSliderValueSilently(Slider_Emissive, NewValue);
	SetInputText(Input_EmissiveValue, NewValue);
	bIsRefreshing = false;

	ApplyToSelectedSurface();
}

void UMaterialAttributesPanelWidget::HandleTextureTilingInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	const float NewValue = FMath::Clamp(FCString::Atof(*Text.ToString()), 0.01f, 20.0f);
	CurrentMaterialData.TextureTiling = NewValue;

	bIsRefreshing = true;
	SetSliderValueSilently(Slider_TextureTiling, NewValue);
	SetInputText(Input_TextureTilingValue, NewValue);
	bIsRefreshing = false;

	ApplyToSelectedSurface();
}

void UMaterialAttributesPanelWidget::HandleCancelClicked()
{
	if (bHasMaterialData)
	{
		CurrentMaterialData = SnapshotMaterialData;
		ApplyToSelectedSurface();
	}

	SetVisibility(ESlateVisibility::Hidden);
}

void UMaterialAttributesPanelWidget::HandleApplyClicked()
{
	SnapshotMaterialData = CurrentMaterialData;
}

void UMaterialAttributesPanelWidget::ApplyToSelectedSurface()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UInteRealUISubSystem* UISubsystem = GI->GetSubsystem<UInteRealUISubSystem>())
		{
			UISubsystem->NotifyWallMaterialDataChanged(CurrentMaterialData);
		}
	}
}

void UMaterialAttributesPanelWidget::RefreshValueReadouts()
{
	SetInputText(Input_MetallicValue, CurrentMaterialData.Metallic);
	SetInputText(Input_SpecularValue, CurrentMaterialData.Specular);
	SetInputText(Input_RoughnessValue, CurrentMaterialData.Roughness);
	SetInputText(Input_EmissiveValue, CurrentMaterialData.Emissive);
	SetInputText(Input_TextureTilingValue, CurrentMaterialData.TextureTiling);
}

void UMaterialAttributesPanelWidget::SetSliderValueSilently(UBaseSlider* Slider, float Value)
{
	if (!Slider) return;

	Slider->SetValue(Value);
}

void UMaterialAttributesPanelWidget::SetInputText(UBaseInput* Input, float Value) const
{
	if (!Input || !Input->Input_Main) return;

	Input->Input_Main->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), Value)));
}

void UMaterialAttributesPanelWidget::InitializeFromInputValues()
{
	bIsRefreshing = true;

	CurrentMaterialData.Metallic = ClampMaterialAttributeValue(ReadInputValue(Input_MetallicValue, CurrentMaterialData.Metallic), 0.0f, 1.0f);
	CurrentMaterialData.Specular = ClampMaterialAttributeValue(ReadInputValue(Input_SpecularValue, CurrentMaterialData.Specular), 0.0f, 1.0f);
	CurrentMaterialData.Roughness = ClampMaterialAttributeValue(ReadInputValue(Input_RoughnessValue, CurrentMaterialData.Roughness), 0.0f, 1.0f);
	CurrentMaterialData.Emissive = FMath::Max(ReadInputValue(Input_EmissiveValue, CurrentMaterialData.Emissive), 0.0f);
	CurrentMaterialData.TextureTiling = FMath::Clamp(ReadInputValue(Input_TextureTilingValue, CurrentMaterialData.TextureTiling), 0.01f, 20.0f);
	
	SetSliderValueSilently(Slider_Metallic, CurrentMaterialData.Metallic);
	SetSliderValueSilently(Slider_Specular, CurrentMaterialData.Specular);
	SetSliderValueSilently(Slider_Roughness, CurrentMaterialData.Roughness);
	SetSliderValueSilently(Slider_Emissive, CurrentMaterialData.Emissive);
	SetSliderValueSilently(Slider_TextureTiling, CurrentMaterialData.TextureTiling);

	RefreshValueReadouts();

	bIsRefreshing = false;
}

float UMaterialAttributesPanelWidget::ReadInputValue(UBaseInput* Input, float DefaultValue) const
{
	if (!Input || !Input->Input_Main)
	{
		return DefaultValue;
	}

	const FString Text = Input->Input_Main->GetText().ToString().TrimStartAndEnd();
	if (Text.IsEmpty())
	{
		return DefaultValue;
	}

	return FCString::Atof(*Text);
}

float UMaterialAttributesPanelWidget::ClampMaterialAttributeValue(float Value, float MinValue, float MaxValue) const
{
	return FMath::Clamp(Value, MinValue, MaxValue);
}

void UMaterialAttributesPanelWidget::ResetForSurfaceWithoutMaterial()
{
	CurrentMaterialData = FMaterialDataRow();
	SnapshotMaterialData = CurrentMaterialData;
	bHasMaterialData = false;

	bIsRefreshing = true;

	SetSliderValueSilently(Slider_Metallic, CurrentMaterialData.Metallic);
	SetSliderValueSilently(Slider_Specular, CurrentMaterialData.Specular);
	SetSliderValueSilently(Slider_Roughness, CurrentMaterialData.Roughness);
	SetSliderValueSilently(Slider_Emissive, CurrentMaterialData.Emissive);
	SetSliderValueSilently(Slider_TextureTiling, CurrentMaterialData.TextureTiling);

	RefreshValueReadouts();

	bIsRefreshing = false;
}