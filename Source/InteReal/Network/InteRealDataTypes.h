#pragma once

#include "CoreMinimal.h"
#include "InteRealDataTypes.generated.h"

/** 검색 리스트 모드 (Project -> Plan -> Version) */
UENUM(BlueprintType)
enum class EInteRealSearchListMode : uint8
{
    Project,
    Plan,
    Version
};

/** 에셋 개별 데이터 */
USTRUCT(BlueprintType)
struct FInteRealAssetData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 id = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString asset_type;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString name;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString display_name;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString unreal_asset_id;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString unreal_path;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString thumbnail_url;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    float width = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    float depth = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    float height = 0.0f;

    bool operator==(const FInteRealAssetData& Other) const 
    { 
        return id == Other.id; 
    }
};

/** 에셋 리스트 응답 */
USTRUCT(BlueprintType)
struct FUnrealAssetListResponse
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    TArray<FInteRealAssetData> items;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 total = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 skip = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 limit = 50;

    bool operator==(const FUnrealAssetListResponse& Other) const 
    { 
        return items == Other.items && total == Other.total; 
    }
};

/** 도면 검색/정렬 파라미터 */
USTRUCT(BlueprintType)
struct FUnrealPlanSearchParams
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString q;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString project_id;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    bool executable_only = true;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString file_name;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString tag;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString status;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString registration_status = TEXT("all");

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString sort = TEXT("newest");

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 skip = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 limit = 50;
};

/** 최근 프로젝트 정보 */
USTRUCT(BlueprintType)
struct FUnrealPlanRecentProject
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 id = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString project_name;

    bool operator==(const FUnrealPlanRecentProject& Other) const 
    { 
        return id == Other.id; 
    }
};

/** 도면(Plan) 개별 항목 */
USTRUCT(BlueprintType)
struct FUnrealPlanItem
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 id = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString name;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString title;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    bool can_open_unreal = false;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString preview_api_url;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 project_id = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString project_name;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 source_floorplan_id = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString file_name;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString mime_type;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int64 file_size = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString preview_url;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString memo;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString status;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString metadata_json;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 version = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 registered_project_count = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString created_at;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString updated_at;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    TArray<FString> tags;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FUnrealPlanRecentProject recent_project;

    bool operator==(const FUnrealPlanItem& Other) const 
    { 
        return id == Other.id; 
    }

    FString GetDisplayTitle() const
    {
        if (!title.IsEmpty())
        {
            return title;
        }

        return name;
    }
};

/** 도면 등록 상태 요약 */
USTRUCT(BlueprintType)
struct FFloorplanRegistrationSummary
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 all = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 registered = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 unregistered = 0;

    bool operator==(const FFloorplanRegistrationSummary& Other) const
    {
        return all == Other.all && registered == Other.registered && unregistered == Other.unregistered;
    }
};

/** 도면 리스트 응답 */
USTRUCT(BlueprintType)
struct FUnrealPlanListResponse
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    TArray<FUnrealPlanItem> items;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 total = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FFloorplanRegistrationSummary summary;

    bool operator==(const FUnrealPlanListResponse& Other) const 
    { 
        return items == Other.items && total == Other.total && summary == Other.summary; 
    }
};

/** 프로젝트별 도면 목록 필터에 사용하는 프로젝트 항목 */
USTRUCT(BlueprintType)
struct FUnrealProjectItem
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 id = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString name;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 plan_count = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString latest_updated_at;

    bool operator==(const FUnrealProjectItem& Other) const
    {
        return id == Other.id;
    }
};

/** 프로젝트 목록 응답 */
USTRUCT(BlueprintType)
struct FUnrealProjectListResponse
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    TArray<FUnrealProjectItem> items;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 total = 0;

    bool operator==(const FUnrealProjectListResponse& Other) const
    {
        return items == Other.items && total == Other.total;
    }
};

/** Delta version item returned by the Unreal API. */
USTRUCT(BlueprintType)
struct FUnrealDeltaVersionItem
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 id = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 version_id = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 plan_id = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 version = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString title;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString name;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString style_name;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString status;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString created_at;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString updated_at;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    bool is_latest = false;

    bool operator==(const FUnrealDeltaVersionItem& Other) const
    {
        return plan_id == Other.plan_id && version == Other.version && version_id == Other.version_id && id == Other.id;
    }

    FString GetDisplayTitle() const
    {
        if (!title.IsEmpty())
        {
            return title;
        }

        if (!name.IsEmpty())
        {
            return name;
        }

        if (!style_name.IsEmpty())
        {
            return style_name;
        }

        return version > 0 ? FString::Printf(TEXT("Version %d"), version) : TEXT("Latest");
    }
};

/** Delta version list response. */
USTRUCT(BlueprintType)
struct FUnrealDeltaVersionListResponse
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    TArray<FUnrealDeltaVersionItem> items;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 total = 0;

    bool operator==(const FUnrealDeltaVersionListResponse& Other) const
    {
        return items == Other.items && total == Other.total;
    }
};

/** 저장된 Delta 조회 응답 */
USTRUCT(BlueprintType)
struct FUnrealDeltaResponse
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 version = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString delta_json;

    bool operator==(const FUnrealDeltaResponse& Other) const
    {
        return version == Other.version && delta_json == Other.delta_json;
    }
};

/** Base Topology 내보내기 요청 파라미터 */
USTRUCT(BlueprintType)
struct FUeTopologyExportRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString layout_type = TEXT("Korean_Standard_3Room_Apartment");

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString scale_unit = TEXT("cm");

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString reference_coordinate_system = TEXT("UE_Z_Up_Y_Right");

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString coordinate_policy = TEXT("ue_z_up_y_negative");

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    float default_wall_height_cm = 260.0f;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    float default_door_height_cm = 200.0f;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    float default_window_height_cm = 150.0f;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    float default_window_sill_height_cm = 90.0f;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    float visual_scale_factor_cm_per_px = 1.0f;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    float vertex_merge_tolerance_cm = 0.001f;
};

/** UE Topology export artifact 응답 */
USTRUCT(BlueprintType)
struct FUeTopologyExportArtifactResponse
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 artifact_id = 0;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString format;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString download_url;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString content_json;

    bool operator==(const FUeTopologyExportArtifactResponse& Other) const
    {
        return artifact_id == Other.artifact_id && format == Other.format && download_url == Other.download_url && content_json == Other.content_json;
    }
};

/** 일반적인 성공 응답 */
USTRUCT(BlueprintType)
struct FUnrealOkResponse
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    bool ok = false;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    int32 version = 0;

    bool operator==(const FUnrealOkResponse& Other) const 
    { 
        return ok == Other.ok && version == Other.version; 
    }
};
