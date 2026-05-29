#include "EditModePlayerController.h"
#include "InteReal/EditMode/Managers/InteriorPlacementManager.h"
#include "InteReal/EditMode/Furnitures/Furniture.h"
#include "InteReal/EditMode/Furnitures/FurnitureData.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"

AEditModePlayerController::AEditModePlayerController()
{
	bShowMouseCursor = true;
	PrimaryActorTick.bCanEverTick = true;
	PlacementManager = nullptr;
}

void AEditModePlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (IMC_EditMode)
		{
			Subsystem->AddMappingContext(IMC_EditMode, 0);
		}
	}

	if (!PlacementManager)
	{
		for (TActorIterator<AInteriorPlacementManager> It(GetWorld()); It; ++It)
		{
			PlacementManager = *It;
			break;
		}
	}
}

void AEditModePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
		return;
	}

	EIC->BindAction(IA_Place, ETriggerEvent::Started, this, &AEditModePlayerController::OnPlace);
	EIC->BindAction(IA_Remove, ETriggerEvent::Started, this, &AEditModePlayerController::OnRemove);

	InputComponent->BindKey(EKeys::G, IE_Pressed, this, &AEditModePlayerController::ToggleGrid);
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AEditModePlayerController::OnTestSpawn);
}

void AEditModePlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateCursorHit();

	if (PlacementManager && bGridVisible)
	{
		PlacementManager->DrawBounds();
	}

	if (!PlacementManager || !bIsHitting)
	{
		return;
	}
	if (!PlacementManager->HasActivePreview())
	{
		return;
	}

	PlacementManager->UpdatePreviewLocation(CurrentCursorWorldLoc);
}

void AEditModePlayerController::UpdateCursorHit()
{
	bIsHitting = GetHitResultUnderCursorByChannel(
		UEngineTypes::ConvertToTraceType(ECC_Visibility), true, LastCursorHit
	);

	if (bIsHitting)
	{
		CurrentCursorWorldLoc = LastCursorHit.Location;
	}
}

void AEditModePlayerController::ToggleGrid()
{
	if (!PlacementManager)
	{
		return;
	}

	bGridVisible = !bGridVisible;
	PlacementManager->SetGridVisible(bGridVisible);
}

void AEditModePlayerController::OnPlace()
{
	if (!PlacementManager || !bIsHitting)
	{
		return;
	}
	if (!PlacementManager->HasActivePreview())
	{
		return;
	}

	PlacementManager->ConfirmFurniture();
}

void AEditModePlayerController::OnRemove()
{
	if (!PlacementManager)
	{
		return;
	}

	if (PlacementManager->HasActivePreview())
	{
		PlacementManager->CancelPreview();
		return;
	}

	AFurniture* HitFurniture = Cast<AFurniture>(LastCursorHit.GetActor());
	if (!HitFurniture)
	{
		return;
	}

	PlacementManager->RemoveFurniture(HitFurniture);
}

void AEditModePlayerController::OnTestSpawn()
{
	if (!PlacementManager || !bIsHitting)
	{
		return;
	}
	StartFurniturePlacement(PlacementManager->FurnitureDataList[0]);
}

void AEditModePlayerController::StartFurniturePlacement(UFurnitureData* FurnitureData)
{
	if (!PlacementManager || !FurnitureData)
	{
		return;
	}

	if (PlacementManager->HasActivePreview())
	{
		PlacementManager->CancelPreview();
	}

	const FVector PreviewSpawnLocation = bIsHitting ? CurrentCursorWorldLoc : FVector::ZeroVector;

	PlacementManager->CreatePreviewFurnitureFromData(
		PreviewSpawnLocation,
		FRotator::ZeroRotator,
		FurnitureData
	);
}