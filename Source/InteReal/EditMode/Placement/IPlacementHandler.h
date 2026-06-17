#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IPlacementHandler.generated.h"

class AFurniture;
class UInteriorPlacementSubsystem;

UENUM(BlueprintType)
enum class EGizmoTransformAxis : uint8
{
	None,
	MoveX,
	MoveY,
	MoveZ,
	RotateYaw,
	RotatePitch,
	RotateRoll,
};

UINTERFACE(MinimalAPI)
class UPlacementHandler : public UInterface
{
	GENERATED_BODY()
};

class INTEREAL_API IPlacementHandler
{
	GENERATED_BODY()
public:
	virtual void Initialize(UInteriorPlacementSubsystem* InSubsystem) = 0;
	
	virtual bool CanHandle(const FHitResult& Hit) const = 0;

	// 이 핸들러가 소유하는 표면에 배치된 가구인지
	virtual bool OwnsFurniture(const AFurniture* Furniture) const = 0;

	// 프리뷰 위치 업데이트
	virtual void UpdatePreview(AFurniture* Preview, const FHitResult& Hit) = 0;

	// 배치 확정 (그리드 점유, Attach 등 표면별 후처리)
	virtual void OnConfirm(AFurniture* Furniture) = 0;

	// 제거 시 정리 (그리드 셀 해제 etc)
	virtual void OnRemove(AFurniture* Furniture) = 0;

	// 기즈모 이동 — 표면마다 동작이 다르므로 핸들러가 담당, 기본 구현은 비어있음
	virtual void BeginGizmoMove(AFurniture* Target) {}
	virtual void UpdateGizmoMove(AFurniture* Target, FVector Cursor, EGizmoTransformAxis Axis) {}
	virtual void UpdateGizmoMoveFree(AFurniture* Target, FVector TargetLoc) {}
	virtual void FinalizeGizmoMove(AFurniture* Target) {}
	virtual void AbortGizmoMove(AFurniture* Target) {}
};
