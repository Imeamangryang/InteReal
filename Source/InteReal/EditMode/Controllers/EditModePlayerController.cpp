#include "EditModePlayerController.h"
#include "InteReal/EditMode/Managers/InteriorPlacementManager.h"
#include "InteReal/EditMode/Furnitures/Furniture.h"
#include "InteReal/EditMode/Gizmo/FurnitureGizmoComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"
#include "Blueprint/UserWidget.h"
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

	if (PlacementTabWidget)
	{
		PlacementTabInstance = CreateWidget<UUserWidget>(this, PlacementTabWidget);
		if (PlacementTabInstance)
		{
			PlacementTabInstance->AddToViewport();

			InputMode.SetWidgetToFocus(PlacementTabInstance->TakeWidget());
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(InputMode);
			bShowMouseCursor = true;
		}
	}

	if (TooltipWidgetClass)
	{
		TooltipInstance = CreateWidget<UPlacementTooltipWidget>(this, TooltipWidgetClass);
		if (TooltipInstance)
		{
			TooltipInstance->AddToViewport();
			TooltipInstance->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void AEditModePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC) return;

	EIC->BindAction(IA_Place, ETriggerEvent::Started,   this, &AEditModePlayerController::OnPlace);
	EIC->BindAction(IA_Place, ETriggerEvent::Completed, this, &AEditModePlayerController::OnPlaceReleased);
	EIC->BindAction(IA_Remove, ETriggerEvent::Started,  this, &AEditModePlayerController::OnRemove);
	EIC->BindAction(IA_Rotate, ETriggerEvent::Started,  this, &AEditModePlayerController::OnRotatePreview);

	InputComponent->BindKey(EKeys::G, IE_Pressed, this, &AEditModePlayerController::ToggleGrid);
}

void AEditModePlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateCursorHit();
	UpdateTooltip();

	// 기즈모 링 드래그 중 → 래디얼 추적 Yaw 회전
	if (bIsDraggingGizmo && SelectedFurniture)
	{
		FVector WorldOrigin, WorldDir;
		DeprojectMousePositionToWorld(WorldOrigin, WorldDir);
		FPlane Plane(SelectedFurniture->GetActorLocation(), FVector::UpVector);
		FVector Hit = FMath::LinePlaneIntersection(WorldOrigin, WorldOrigin + WorldDir * 100000.f, Plane);
		FVector Center = SelectedFurniture->GetActorLocation();

		float CurrentAngle = FMath::RadiansToDegrees(FMath::Atan2(Hit.Y - Center.Y, Hit.X - Center.X));
		float DeltaAngle = CurrentAngle - DragStartAngleDeg;

		FRotator NewRot = DragStartFurnitureRot;
		NewRot.Yaw = FRotator::NormalizeAxis(NewRot.Yaw + DeltaAngle);

		if (IsInputKeyDown(EKeys::LeftControl))
		{
			NewRot.Yaw = FMath::GridSnap(NewRot.Yaw, 15.0f);
		}

		SelectedFurniture->SetActorRotation(NewRot);

		// 드래그 각도만큼 링을 실시간으로 다시 그려서 게이지 시각화
		if (UFurnitureGizmoComponent* GizmoComp = SelectedFurniture->FindComponentByClass<UFurnitureGizmoComponent>())
		{
			float BoundsMax = SelectedFurniture->GetComponentsBoundingBox().GetExtent().GetMax();
			GizmoComp->UpdateRadialRotationRing(BoundsMax, DeltaAngle);
		}
		return;
	}

	// 프리뷰 가구 이동
	if (!PlacementManager || !bIsHitting) return;
	if (!PlacementManager->HasActivePreview()) return;
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
	if (!PlacementManager) return;
	bGridVisible = !bGridVisible;
	PlacementManager->SetGridVisible(bGridVisible);
}

void AEditModePlayerController::OnPlace()
{
	if (PlacementManager && PlacementManager->HasActivePreview())
	{
		if (bIsHitting) PlacementManager->ConfirmFurniture();
		return;
	}

	if (!bIsHitting)
	{
		DeselectFurniture();
		return;
	}
	
	UPrimitiveComponent* HitComp = LastCursorHit.GetComponent();
	if (HitComp && HitComp->GetFName() == FName(TEXT("RingMeshComp")) && SelectedFurniture)
	{
		bIsDraggingGizmo = true;
		DragStartFurnitureRot = SelectedFurniture->GetActorRotation();

		FVector WorldOrigin, WorldDir;
		DeprojectMousePositionToWorld(WorldOrigin, WorldDir);
		FPlane Plane(SelectedFurniture->GetActorLocation(), FVector::UpVector);
		FVector Hit = FMath::LinePlaneIntersection(WorldOrigin, WorldOrigin + WorldDir * 100000.f, Plane);
		FVector Center = SelectedFurniture->GetActorLocation();
		DragStartAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Hit.Y - Center.Y, Hit.X - Center.X));
		return;
	}
	
	if (AFurniture* HitFurniture = Cast<AFurniture>(LastCursorHit.GetActor()))
	{
		if (HitFurniture->GetPlacementState() == EPlacementState::Placed)
		{
			if (SelectedFurniture == HitFurniture)
			{
				// 이미 선택된 가구를 다시 클릭 → 이동 모드 진입
				const FFurnitureDataRow* Row = PlacementManager->FindFurnitureRowByID(HitFurniture->FurnitureID);
				if (Row)
				{
					PlacementManager->RemoveFurniture(HitFurniture); // Grid 점유 해제 + Destroy
					SelectedFurniture = nullptr; // StartFurniturePlacement 내부에서 DeselectFurniture 재호출 방지
					bIsDraggingGizmo = false;
					StartFurniturePlacement(*Row);
				}
				return;
			}

			// 처음 클릭 → 선택만
			SelectFurniture(HitFurniture);
			return;
		}
	}
	
	DeselectFurniture();
}

void AEditModePlayerController::OnPlaceReleased()
{
	if (bIsDraggingGizmo && SelectedFurniture)
	{
		// 드래그 종료 후 링을 360도 풀 링으로 원복
		if (UFurnitureGizmoComponent* GizmoComp = SelectedFurniture->FindComponentByClass<UFurnitureGizmoComponent>())
		{
			FBox Bounds = SelectedFurniture->GetComponentsBoundingBox();
			GizmoComp->SetupFromLocalBounds(Bounds.TransformBy(SelectedFurniture->GetActorTransform().Inverse()));
		}
	}
	bIsDraggingGizmo = false;
}

void AEditModePlayerController::OnRemove()
{
	if (!PlacementManager) return;

	// 프리뷰 중 → 취소
	if (PlacementManager->HasActivePreview())
	{
		PlacementManager->CancelPreview();
		return;
	}
	
	if (SelectedFurniture)
	{
		DeselectFurniture();
		return;
	}
	
	if (AFurniture* HitFurniture = Cast<AFurniture>(LastCursorHit.GetActor()))
	{
		PlacementManager->RemoveFurniture(HitFurniture);
	}
}

void AEditModePlayerController::OnRotatePreview()
{
	// 프리뷰 중 → 90도 스냅
	if (PlacementManager && PlacementManager->HasActivePreview())
	{
		PlacementManager->RotatePreview(90.0f);
		return;
	}

	// 배치된 가구 선택 중 → 90도 스냅
	if (SelectedFurniture)
	{
		FRotator Rot = SelectedFurniture->GetActorRotation();
		Rot.Yaw = FRotator::NormalizeAxis(Rot.Yaw + 90.0f);
		SelectedFurniture->SetActorRotation(Rot);
	}
}

void AEditModePlayerController::SelectFurniture(AFurniture* Furniture)
{
	if (SelectedFurniture == Furniture) return;
	DeselectFurniture();
	SelectedFurniture = Furniture;
	SelectedFurniture->SetSelected(true);
}

void AEditModePlayerController::DeselectFurniture()
{
	if (SelectedFurniture)
	{
		SelectedFurniture->SetSelected(false);
		SelectedFurniture = nullptr;
	}
	bIsDraggingGizmo = false;
}

void AEditModePlayerController::UpdateTooltip()
{
	if (!TooltipInstance || !PlacementManager) return;

	// 프리뷰 중이고 Invalid 상태일 때만 표시
	if (!PlacementManager->HasActivePreview() ||
		PlacementManager->InvalidReason == EPlacementInvalidReason::None)
	{
		TooltipInstance->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	TooltipInstance->ShowReason(PlacementManager->InvalidReason);

	// 마우스 커서 옆에 위치
	float MouseX, MouseY;
	GetMousePosition(MouseX, MouseY);
	TooltipInstance->SetPositionInViewport(FVector2D(MouseX + 16.f, MouseY + 16.f), false);
	TooltipInstance->SetVisibility(ESlateVisibility::HitTestInvisible);
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
		int32 ID = 0;
		if (Root->TryGetNumberField(TEXT("furnitureId"), ID))
		{
			const FFurnitureDataRow* Row = PlacementManager->FindFurnitureRowByID(ID);
			if (Row) StartFurniturePlacement(*Row);
		}
	}
	else if (Action == TEXT("ROTATE"))
	{
		if (PlacementManager->HasActivePreview()) PlacementManager->RotatePreview(90.0f);
	}
	else if (Action == TEXT("CONFIRM"))
	{
		if (PlacementManager->HasActivePreview()) PlacementManager->ConfirmFurniture();
	}
	else if (Action == TEXT("CANCEL"))
	{
		if (PlacementManager->HasActivePreview()) PlacementManager->CancelPreview();
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

void AEditModePlayerController::StartFurniturePlacement(const FFurnitureDataRow& FurnitureRow)
{
	if (!PlacementManager) return;

	DeselectFurniture(); // 가구 들기 시작하면 선택 해제

	if (PlacementManager->HasActivePreview())
	{
		PlacementManager->CancelPreview();
	}

	const FVector SpawnLoc = bIsHitting ? CurrentCursorWorldLoc : FVector::ZeroVector;
	PlacementManager->CreatePreviewFurnitureFromRow(SpawnLoc, FRotator::ZeroRotator, FurnitureRow);
}
