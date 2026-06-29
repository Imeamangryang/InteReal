#include "LightFixture.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"

ALightFixture::ALightFixture()
{
	SpotLightComponent = CreateDefaultSubobject<USpotLightComponent>(TEXT("SpotLightComponent"));
	SpotLightComponent->SetupAttachment(MeshComponent);
	SpotLightComponent->SetVisibility(false);
	SpotLightComponent->IntensityUnits = ELightUnits::Candelas;

	RectLightComponent = CreateDefaultSubobject<URectLightComponent>(TEXT("RectLightComponent"));
	RectLightComponent->SetupAttachment(MeshComponent);
	RectLightComponent->SetVisibility(false);
	RectLightComponent->IntensityUnits = ELightUnits::Candelas;
}

void ALightFixture::ApplyFurnitureRow(const FFurnitureDataRow& InFurnitureRow)
{
	Super::ApplyFurnitureRow(InFurnitureRow);
	SetLightAttributes(InFurnitureRow.LightAttributes);
}

void ALightFixture::SetLightAttributes(const FLightAttributes& InAttributes)
{
	CurrentLightAttributes = InAttributes;

	const bool bIsSpot = InAttributes.LightFixtureType == ELightFixtureType::Spot;
	const bool bIsRect = InAttributes.LightFixtureType == ELightFixtureType::Rect;
	const bool bIsPoint = !bIsSpot && !bIsRect;

	if (LightComponent)
	{
		LightComponent->SetVisibility(InAttributes.bEmitsLight && bIsPoint);
	}
	if (SpotLightComponent)
	{
		SpotLightComponent->SetVisibility(InAttributes.bEmitsLight && bIsSpot);
	}
	if (RectLightComponent)
	{
		RectLightComponent->SetVisibility(InAttributes.bEmitsLight && bIsRect);
	}

	ULightComponent* ActiveLight = bIsSpot
		? static_cast<ULightComponent*>(SpotLightComponent)
		: bIsRect
			? static_cast<ULightComponent*>(RectLightComponent)
			: static_cast<ULightComponent*>(LightComponent);

	if (ActiveLight)
	{
		ActiveLight->SetLightColor(InAttributes.LightColor);
		ActiveLight->SetIntensity(InAttributes.LightIntensity);
	}
	
	if (ULocalLightComponent* LocalLight = Cast<ULocalLightComponent>(ActiveLight))
	{
		LocalLight->SetAttenuationRadius(InAttributes.AttenuationRadius);
	}
}
