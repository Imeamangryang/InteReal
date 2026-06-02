#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "ProceduralMeshComponent.h"
#include "FurnitureGizmoComponent.generated.h"

UCLASS(ClassGroup=(EditMode), meta=(BlueprintSpawnableComponent))
class INTEREAL_API UFurnitureGizmoComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UFurnitureGizmoComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gizmo")
	UMaterialInterface* GizmoMaterial = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Gizmo")
	void SetupFromLocalBounds(FBox LocalBounds);

	UFUNCTION(BlueprintCallable, Category = "Gizmo")
	void SetGizmoVisible(bool bIsVisible);

	UFUNCTION(BlueprintCallable, Category = "Gizmo")
	void SetGizmoColor(FLinearColor Color, float Intensity = 4.0f);

	// 드래그 중 DeltaAngle만큼만 링을 실시간으로 다시 그림
	void UpdateRadialRotationRing(float InBoundsMax, float DeltaAngleDeg);

private:
	UPROPERTY()
	UProceduralMeshComponent* RingMeshComp;

	UPROPERTY()
	UProceduralMeshComponent* ArrowsMeshComp;

	UPROPERTY()
	UMaterialInstanceDynamic* RingDynMat = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* ArrowsDynMat = nullptr;

	void BuildRotationRing(float Radius, float TubeRadius, int32 Segments, int32 TubeSegments);
	void BuildArrows(float Length, float ArrowRadius);
	void CreateCylinderMesh(TArray<FVector>& Vertices, TArray<int32>& Triangles, TArray<FVector>& Normals,
	                        FVector Start, FVector End, float Radius, int32 Segments);
};
