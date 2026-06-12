#pragma once

#include "CoreMinimal.h"
#include "InteRealDataTypes.generated.h"

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
    bool can_open_unreal = false;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString file_name;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FString status;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    TArray<FString> tags;

    UPROPERTY(BlueprintReadWrite, Category = "InteReal|Network")
    FUnrealPlanRecentProject recent_project;

    bool operator==(const FUnrealPlanItem& Other) const 
    { 
        return id == Other.id; 
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

    bool operator==(const FUnrealPlanListResponse& Other) const 
    { 
        return items == Other.items && total == Other.total; 
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
