// DEPRECATED: 모든 로직은 UInteriorPlacementSubsystem으로 이전되었습니다.
// Harness 등 외부 코드 호환성을 위해 thin facade로만 유지합니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "InteReal/EditMode/Furniture/Furniture.h"
#include "InteReal/EditMode/Furniture/FFurnitureDataRow.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "InteReal/Harness/Public/HarnessData.h"
#include "InteriorPlacementManager.generated.h"

class UMeshComponent;

UCLASS()
class INTEREAL_API AInteriorPlacementManager : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Manual")
	float GridCellSize = 10.0f;

	// HarnessSaveManagerComponent가 직접 참조하는 FurnitureClass
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture")
	TSubclassOf<AFurniture> FurnitureClass;
	
	EPlacementInvalidReason InvalidReason = EPlacementInvalidReason::None;

	// ===== Harness / 외부 코드에서 사용하는 API =====

	UFUNCTION(BlueprintCallable)
	void InitializeFromFloorData(const FHarnessFloorData& FloorData, float Cell = 50.0f)
	{
		if (UInteriorPlacementSubsystem* PS = GetPS())
		{
			PS->InitializeFromFloorData(FloorData, Cell);
		}
	}

	const FFurnitureDataRow* FindFurnitureRowByID(int32 TargetID) const
	{
		if (UInteriorPlacementSubsystem* PS = GetPS())
		{
			return PS->FindFurnitureRowByID(TargetID);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable)
	void ImportPlacedFurnituresJson(const FString& JsonString)
	{
		if (UInteriorPlacementSubsystem* PS = GetPS())
		{
			PS->ImportPlacedFurnituresJson(JsonString);
		}
	}

	// ===== EditModePlayerController가 사용하는 API =====

	bool HasActivePreview() const
	{
		if (UInteriorPlacementSubsystem* PS = GetPS())
		{
			return PS->HasActivePreview();
		}
		return false;
	}

	void UpdatePreviewLocation(const FHitResult& CursorHit)
	{
		if (UInteriorPlacementSubsystem* PS = GetPS())
		{
			PS->UpdatePreviewLocation(CursorHit);
			InvalidReason = PS->InvalidReason;
		}
	}

	void SetGridVisible(bool bVisible)
	{
		if (UInteriorPlacementSubsystem* PS = GetPS())
		{
			PS->SetGridVisible(bVisible);
		}
	}

	void ConfirmFurniture(bool bContinuePlacement = false)
	{
		if (UInteriorPlacementSubsystem* PS = GetPS())
		{
			PS->ConfirmFurniture(bContinuePlacement);
		}
	}

	void RemoveFurniture(AFurniture* Target)
	{
		if (UInteriorPlacementSubsystem* PS = GetPS())
		{
			PS->RemoveFurniture(Target);
		}
	}

	void BeginGizmoMove(AFurniture* Target)
	{
		if (UInteriorPlacementSubsystem* PS = GetPS())
		{
			PS->BeginGizmoMove(Target);
		}
	}

	void UpdateGizmoMoveLocation(FVector CursorOnGround, AFurniture* Target, const FString& Axis)
	{
		if (UInteriorPlacementSubsystem* PS = GetPS())
		{
			PS->UpdateGizmoMoveLocation(CursorOnGround, Target, ParseGizmoAxis(Axis));
		}
	}

	void FinalizeGizmoMove(AFurniture* Target)
	{
		if (UInteriorPlacementSubsystem* PS = GetPS())
		{
			PS->FinalizeGizmoMove(Target);
		}
	}

	void CancelPreview()
	{
		if (UInteriorPlacementSubsystem* PS = GetPS())
		{
			PS->CancelPreview();
		}
	}

	void RotatePreview(float AngleDeg = 90.0f)
	{
		if (UInteriorPlacementSubsystem* PS = GetPS())
		{
			PS->RotatePreview(AngleDeg);
		}
	}

	void CreatePreviewFurnitureFromRow(FVector RayPosition, FRotator Rotation, const FFurnitureDataRow& InFurnitureRow)
	{
		if (UInteriorPlacementSubsystem* PS = GetPS())
		{
			PS->CreatePreviewFurnitureFromRow(RayPosition, Rotation, InFurnitureRow);
		}
	}

private:
	UInteriorPlacementSubsystem* GetPS() const
	{
		return GetWorld() ? GetWorld()->GetSubsystem<UInteriorPlacementSubsystem>() : nullptr;
	}

	static EGizmoTransformAxis ParseGizmoAxis(const FString& Axis)
	{
		if (Axis == TEXT("MoveX") || Axis == TEXT("X"))
		{
			return EGizmoTransformAxis::MoveX;
		}
		if (Axis == TEXT("MoveY") || Axis == TEXT("Y"))
		{
			return EGizmoTransformAxis::MoveY;
		}
		if (Axis == TEXT("MoveZ") || Axis == TEXT("Z"))
		{
			return EGizmoTransformAxis::MoveZ;
		}
		return EGizmoTransformAxis::None;
	}
};
