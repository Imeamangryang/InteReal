#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "HarnessData.generated.h"

USTRUCT(BlueprintType)
struct FTopologyVertex
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float x = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float y = 0.0f;

    FVector2D ToVector2D() const { return FVector2D(x, y); }
};

USTRUCT(BlueprintType)
struct FTopologyHalfEdge
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString vertex_start;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString vertex_end;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString twin_id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float wall_thickness = 20.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString type; // "WallOuter", "WallInner", "WallLintel"
};

USTRUCT(BlueprintType)
struct FTopologyOpening
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString type; // "Door", "Window"
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString target_edge_id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float width_cm = 90.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float height_cm = 200.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float z_offset_cm = 0.0f;
};

USTRUCT(BlueprintType)
struct FTopologyFace
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString face_id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString label; // "LivingRoom", "MasterBedroom"
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float height_cm = 260.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float z_offset = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FString> contour_vertex_ids;
};

USTRUCT(BlueprintType)
struct FHarnessFloorData
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTopologyVertex> vertices;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTopologyHalfEdge> half_edges;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTopologyOpening> openings;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTopologyFace> faces;
};

UCLASS(BlueprintType)
class INTEREAL_API UHarnessStyleDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DataDriven")
    TMap<FString, TObjectPtr<UStaticMesh>> MeshMap;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DataDriven")
    TMap<FString, TObjectPtr<UMaterialInterface>> MaterialMap;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DataDriven")
    TObjectPtr<UMaterialInterface> DefaultFallbackMaterial = nullptr;
};

USTRUCT(BlueprintType)
struct FHarnessStyleRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
    TObjectPtr<UStaticMesh> Mesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
    TObjectPtr<UMaterialInterface> Material = nullptr;
};