#include "LightFixture.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/SphereComponent.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"

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

	IconBillboardComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("IconBillboardComponent"));
	IconBillboardComponent->SetupAttachment(MeshComponent);
	IconBillboardComponent->SetAbsolute(false, true, true);
	IconBillboardComponent->SetHiddenInGame(true);
	IconBillboardComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	IconBillboardComponent->bIsScreenSizeScaled = true;
	IconBillboardComponent->ScreenSize = 0.0025f;

	RadiusIndicatorComponent = CreateDefaultSubobject<USphereComponent>(TEXT("RadiusIndicatorComponent"));
	RadiusIndicatorComponent->SetupAttachment(MeshComponent);
	RadiusIndicatorComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RadiusIndicatorComponent->SetHiddenInGame(true);
	RadiusIndicatorComponent->ShapeColor = FColor(255, 214, 130);
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

	if (RadiusIndicatorComponent)
	{
		RadiusIndicatorComponent->SetSphereRadius(FMath::Max(1.0f, InAttributes.AttenuationRadius));
		RadiusIndicatorComponent->ShapeColor = InAttributes.LightColor.ToFColor(true);
	}

	UpdateIconMaterialParameters();
	UpdateIndicatorVisibility();
}

void ALightFixture::SetPlacementState(EPlacementState NewState)
{
	Super::SetPlacementState(NewState);
	
	if (CollisionBoxComponent)
	{
		CollisionBoxComponent->SetHiddenInGame(true);
	}
	
	if (const UInteriorPlacementSubsystem* PS = GetWorld()
		                                            ? GetWorld()->GetSubsystem<UInteriorPlacementSubsystem>()
		                                            : nullptr)
	{
		bIconForcedHidden = !PS->AreLightFixtureIconsCurrentlyVisible();
	}

	UpdateIndicatorVisibility();
}

void ALightFixture::SetSelected(bool bSelected)
{
	Super::SetSelected(bSelected);
	bIsSelected = bSelected;
	UpdateIconMaterialParameters();
	UpdateIndicatorVisibility();
}

void ALightFixture::SetIconForcedHidden(bool bInForcedHidden)
{
	bIconForcedHidden = bInForcedHidden;
	UpdateIndicatorVisibility();
}

void ALightFixture::UpdateIconMaterialParameters()
{
	if (!IconBillboardComponent)
	{
		return;
	}

	IconBillboardComponent->SetSprite(ResolveIconTexture());
	IconBillboardComponent->ScreenSize = FMath::Clamp(IconPixelSize / 12800.0f, 0.00005f, 0.02f);
	IconBillboardComponent->MarkRenderStateDirty();
}

UTexture2D* ALightFixture::ResolveIconTexture() const
{
	UTexture2D* TypeIcon = nullptr;
	switch (CurrentLightAttributes.LightFixtureType)
	{
	case ELightFixtureType::Spot: TypeIcon = IconTexture_Spot;
		break;
	case ELightFixtureType::Rect: TypeIcon = IconTexture_Rect;
		break;
	default: TypeIcon = IconTexture_Point;
		break;
	}
	return TypeIcon ? TypeIcon : GetFurnitureDataRow().DisplayImage.Get();
}

void ALightFixture::UpdateIndicatorVisibility()
{
	const bool bShowIcon = !bIconForcedHidden;
	const bool bShowRadius = bShowIcon && bIsSelected && CurrentLightAttributes.bEmitsLight;

	UE_LOG(LogTemp, Warning, TEXT("[IconDebug] UpdateIndicatorVisibility: bIconForcedHidden=%s bShowIcon=%s"),
		bIconForcedHidden ? TEXT("true") : TEXT("false"),
		bShowIcon ? TEXT("true") : TEXT("false"));

	if (IconBillboardComponent)
	{
		IconBillboardComponent->SetHiddenInGame(!bShowIcon);
	}
	if (RadiusIndicatorComponent)
	{
		RadiusIndicatorComponent->SetHiddenInGame(!bShowRadius);
	}
}
