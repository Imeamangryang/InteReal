#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "HarnessData.generated.h"

USTRUCT(BlueprintType)
struct FTopologyCoordinateSystemMetadata
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool flipX = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool flipY = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool swapXY = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString target;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float scaleCmPerPx = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float scaleMmPerPx = 10.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString coordinatePolicy;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString imageXToUnrealAxis;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString imageYToUnrealAxis;
};

USTRUCT(BlueprintType)
struct FTopologyMetadata
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTopologyCoordinateSystemMetadata coordinateSystem;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float sourceImageWidth = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float sourceImageHeight = 0.0f;
};

USTRUCT(BlueprintType)
struct FTopologyProjectInfo
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString layout_type;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString scale_unit = TEXT("cm");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString reference_coordinate_system;
};

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
struct FTopologyPoint2D
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float x = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float y = 0.0f;

    FVector2D ToVector2D() const { return FVector2D(x, y); }
};

USTRUCT(BlueprintType)
struct FTopologyHalfEdge
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString wall_id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString side;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString vertex_start;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString vertex_end;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString twin_id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float wall_thickness = 20.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float wall_ratio_start = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float wall_ratio_end = 1.0f;
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString door_kind;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString door_direction;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString window_role;
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
struct FTopologyWallSideMeasurement
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString wall_id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString side;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString span_id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float start_ratio = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float end_ratio = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString room_id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString room_name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float length_cm = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString status;
};

USTRUCT(BlueprintType)
struct FTopologySurfaceMeasurement
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString surface_type;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString wall_id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString side;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString span_id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float start_distance_cm = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float end_distance_cm = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float length_cm = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTopologyPoint2D start_point;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTopologyPoint2D end_point;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString status;
};

USTRUCT(BlueprintType)
struct FHarnessFloorData
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTopologyProjectInfo project_info;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTopologyMetadata metadata;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTopologyVertex> vertices;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTopologyHalfEdge> half_edges;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTopologyWallSideMeasurement> wall_side_measurements;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTopologySurfaceMeasurement> surface_measurements;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTopologyOpening> openings;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTopologyFace> faces;

    bool UsesDirectUnrealCoordinates() const
    {
        const FTopologyCoordinateSystemMetadata& CoordinateSystem = metadata.coordinateSystem;
        const bool bExplicitUnrealAxes =
            CoordinateSystem.imageXToUnrealAxis.Equals(TEXT("X"), ESearchCase::IgnoreCase) &&
            CoordinateSystem.imageYToUnrealAxis.Equals(TEXT("-Y"), ESearchCase::IgnoreCase);

        return CoordinateSystem.target.Equals(TEXT("unreal"), ESearchCase::IgnoreCase) ||
            CoordinateSystem.coordinatePolicy.Equals(TEXT("ue_z_up_y_negative"), ESearchCase::IgnoreCase) ||
            bExplicitUnrealAxes;
    }

    bool UsesNegativeImageYCoordinates() const
    {
        const FTopologyCoordinateSystemMetadata& CoordinateSystem = metadata.coordinateSystem;
        return CoordinateSystem.coordinatePolicy.Equals(TEXT("ue_z_up_y_negative"), ESearchCase::IgnoreCase) ||
            CoordinateSystem.imageYToUnrealAxis.Equals(TEXT("-Y"), ESearchCase::IgnoreCase);
    }

    FVector2D ToHarnessPoint(const FTopologyVertex& Vertex) const
    {
        const bool bStrictUnrealCoordinatePayload =
            metadata.coordinateSystem.target.IsEmpty() &&
            metadata.coordinateSystem.coordinatePolicy.IsEmpty() &&
            project_info.reference_coordinate_system.Contains(TEXT("UE"), ESearchCase::IgnoreCase);

        if (bStrictUnrealCoordinatePayload || UsesDirectUnrealCoordinates())
        {
            return FVector2D(Vertex.x, UsesNegativeImageYCoordinates() ? -Vertex.y : Vertex.y);
        }

        return FVector2D(Vertex.y, Vertex.x);
    }
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
