#include "EditModePlayerController.h"
#include "InteReal/EditMode/Managers/InteriorPlacementManager.h"
#include "InteReal/EditMode/Furnitures/Furniture.h"
#include "InteReal/EditMode/Furnitures/FurnitureData.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

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
	EIC->BindAction(IA_Rotate, ETriggerEvent::Started, this, &AEditModePlayerController::OnRotatePreview);

	// 테스트용
	InputComponent->BindKey(EKeys::G, IE_Pressed, this, &AEditModePlayerController::ToggleGrid);
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AEditModePlayerController::OnTestSpawn);
}

void AEditModePlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateCursorHit();

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

void AEditModePlayerController::OnRotatePreview()
{
	if (!PlacementManager || !PlacementManager->HasActivePreview())
	{
		return;
	}

	PlacementManager->RotatePreview(90.0f);
}

void AEditModePlayerController::OnTestSpawn()
{
	if (!PlacementManager || !bIsHitting)
	{
		return;
	}
	StartFurniturePlacement(PlacementManager->FurnitureDataList[0]);
}

void AEditModePlayerController::ReceiveWebCommand(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[EditMode] Invalid web command JSON: %s"), *JsonString);
		return;
	}

	FString Action;
	if (!Root->TryGetStringField(TEXT("action"), Action) || !PlacementManager)
	{
		return;
	}

	if (Action == TEXT("SELECT_KIND"))
	{
		int32 ID = Root->GetIntegerField(TEXT("furnitureId"));
		UFurnitureData* Data = PlacementManager->FindFurnitureDataByID(ID);
		if (Data)
		{
			StartFurniturePlacement(Data);
		}
	}
	else if (Action == TEXT("ROTATE"))
	{
		if (PlacementManager->HasActivePreview())
		{
			PlacementManager->RotatePreview(90.0f);
		}
	}
	else if (Action == TEXT("CONFIRM"))
	{
		if (PlacementManager->HasActivePreview())
		{
			PlacementManager->ConfirmFurniture();
			// TODO: ExportPlacedFurnituresJson() 결과를 PixelStreaming 플러그인으로 웹에 역송출
		}
	}
	else if (Action == TEXT("CANCEL"))
	{
		if (PlacementManager->HasActivePreview())
		{
			PlacementManager->CancelPreview();
		}
	}
	else if (Action == TEXT("LOAD"))
	{
		FString Payload;
		if (Root->TryGetStringField(TEXT("data"), Payload))
		{
			PlacementManager->ImportPlacedFurnituresJson(Payload);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[EditMode] Unknown web action: %s"), *Action);
	}
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
