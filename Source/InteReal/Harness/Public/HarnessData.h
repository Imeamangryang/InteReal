#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "HarnessData.generated.h"

// ============================================================================
//  v3.1 Topology Data Structures
//  서버 JSON 계약 v3.1 기반 - walls / spaces / openings 독립 객체 토폴로지
// ============================================================================

// 도면 정보
USTRUCT(BlueprintType)
struct FTopologyPlanInfo
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 id = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 version = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString name;
};

// 층 정보
USTRUCT(BlueprintType)
struct FTopologyLevel
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float elevation_cm = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float default_height_cm = 240.0f;
};

// 정점 (벽 중심선 끝점 또는 공간 경계점)
USTRUCT(BlueprintType)
struct FTopologyVertex
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float x = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float y = 0.0f;

	FVector2D ToVector2D() const { return FVector2D(x, y); }
};

// 반간선 (벽 중심선에서 파생된 방향성 간선)
USTRUCT(BlueprintType)
struct FTopologyHalfEdge
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString wall_id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString vertex_start;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString vertex_end;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString twin_id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float wall_thickness = 20.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float wall_height = 240.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString type; // "WallOuter", "WallInner"
};

// 문/창문 여닫이 정보
USTRUCT(BlueprintType)
struct FTopologySwing
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString direction; // inward, outward, sliding, none
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString hinge;     // left, right, none
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float angle = 90.0f;
};

// 개구부 (문/창문)
USTRUCT(BlueprintType)
struct FTopologyOpening
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString type;              // "Door", "Window" (정규화된 레거시 타입)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString kind;              // 원본 kind: door, entrance_door, sliding_door, window, opening
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString target_edge_id;    // host wall의 primary edge ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString host_wall_id;      // host wall ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString offset_from = TEXT("start");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float offset_to_center_cm = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float width_cm = 90.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float height_cm = 210.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float z_offset_cm = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FString> connects;  // 연결하는 공간 ID 목록
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FTopologySwing swing;      // 여닫이 정보
};

// 공간 (방/거실/욕실 등)
USTRUCT(BlueprintType)
struct FTopologyFace
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString face_id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString kind;              // living_room, bedroom, bathroom 등
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString label;             // 표시용 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float height_cm = 240.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float z_offset = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FString> contour_vertex_ids;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FString> boundary_wall_ids;  // 이 공간을 둘러싸는 벽 ID 목록
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString floor_material;             // 기본 바닥 머티리얼 힌트
};

// ============================================================================
//  도면 전체 데이터 (루트 컨테이너)
// ============================================================================
USTRUCT(BlueprintType)
struct FHarnessFloorData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString schema_version;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FTopologyPlanInfo plan;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTopologyLevel> levels;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTopologyVertex> vertices;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTopologyHalfEdge> half_edges;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTopologyOpening> openings;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTopologyFace> faces;

	/**
	 * v3.1 고정 좌표 변환: JSON(top_left, x=right, y=down, mm→cm) → Unreal(Z-up)
	 * JSON X(right) → Unreal Y, JSON Y(down) → Unreal -X
	 */
	FVector2D ToHarnessPoint(const FTopologyVertex& Vertex) const
	{
		return FVector2D(-Vertex.y, Vertex.x);
	}
};

// ============================================================================
//  스타일 데이터 에셋
// ============================================================================
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
