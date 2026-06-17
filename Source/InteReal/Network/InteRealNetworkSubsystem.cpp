#include "InteRealNetworkSubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/PlatformFileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    bool IsSuccessfulResponse(FHttpResponsePtr Response, bool bSucceeded)
    {
        return bSucceeded && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode());
    }

    void AppendQueryParam(FString& Query, const FString& Key, const FString& Value)
    {
        Query += Query.Contains(TEXT("?")) ? TEXT("&") : TEXT("?");
        Query += Key;
        Query += TEXT("=");
        Query += FGenericPlatformHttp::UrlEncode(Value);
    }

    bool TryDeserializeObject(const FString& Json, TSharedPtr<FJsonObject>& OutObject)
    {
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }

    bool TryDeserializeArray(const FString& Json, TArray<TSharedPtr<FJsonValue>>& OutArray)
    {
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        return FJsonSerializer::Deserialize(Reader, OutArray);
    }

    bool TrySerializeJsonValue(const TSharedPtr<FJsonValue>& Value, FString& OutJson)
    {
        if (!Value.IsValid() || Value->Type == EJson::Null)
        {
            return false;
        }

        if (Value->Type == EJson::String)
        {
            OutJson = Value->AsString();
            return true;
        }

        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
        if (Value->Type == EJson::Object)
        {
            return FJsonSerializer::Serialize(Value->AsObject().ToSharedRef(), Writer);
        }

        if (Value->Type == EJson::Array)
        {
            return FJsonSerializer::Serialize(Value->AsArray(), Writer);
        }

        return false;
    }

    FString ExtractJsonPayload(const FString& Body, const TArray<FString>& CandidateFields)
    {
        TSharedPtr<FJsonObject> Root;
        if (!TryDeserializeObject(Body, Root))
        {
            return Body;
        }

        for (const FString& Field : CandidateFields)
        {
            if (const TSharedPtr<FJsonValue>* Value = Root->Values.Find(Field))
            {
                FString Extracted;
                if (TrySerializeJsonValue(*Value, Extracted))
                {
                    return Extracted;
                }
            }
        }

        const TSharedPtr<FJsonObject>* DataObject = nullptr;
        if (Root->TryGetObjectField(TEXT("data"), DataObject) && DataObject && DataObject->IsValid())
        {
            for (const FString& Field : CandidateFields)
            {
                if (const TSharedPtr<FJsonValue>* Value = (*DataObject)->Values.Find(Field))
                {
                    FString Extracted;
                    if (TrySerializeJsonValue(*Value, Extracted))
                    {
                        return Extracted;
                    }
                }
            }
        }

        return Body;
    }

    bool TryGetObjectInt(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, int32& OutValue)
    {
        double NumberValue = 0.0;
        if (Object.IsValid() && Object->TryGetNumberField(FieldName, NumberValue))
        {
            OutValue = static_cast<int32>(NumberValue);
            return true;
        }

        return false;
    }

    bool TryGetObjectString(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, FString& OutValue)
    {
        return Object.IsValid() && Object->TryGetStringField(FieldName, OutValue) && !OutValue.IsEmpty();
    }

    FUnrealPlanItem MakePlanItemFromObject(const TSharedPtr<FJsonObject>& Object)
    {
        FUnrealPlanItem Item;
        if (!Object.IsValid())
        {
            return Item;
        }

        FJsonObjectConverter::JsonObjectToUStruct(Object.ToSharedRef(), FUnrealPlanItem::StaticStruct(), &Item, 0, 0);

        if (Item.id == 0)
        {
            TryGetObjectInt(Object, TEXT("plan_id"), Item.id);
        }

        FString StringValue;
        if (Item.title.IsEmpty() && TryGetObjectString(Object, TEXT("title"), StringValue))
        {
            Item.title = StringValue;
        }

        if (Item.name.IsEmpty())
        {
            if (TryGetObjectString(Object, TEXT("name"), StringValue) ||
                TryGetObjectString(Object, TEXT("display_name"), StringValue) ||
                TryGetObjectString(Object, TEXT("project_name"), StringValue) ||
                TryGetObjectString(Object, TEXT("file_name"), StringValue))
            {
                Item.name = StringValue;
            }
        }

        bool bCanOpen = false;
        if (Object->TryGetBoolField(TEXT("can_open_unreal"), bCanOpen) ||
            Object->TryGetBoolField(TEXT("canOpenUnreal"), bCanOpen) ||
            Object->TryGetBoolField(TEXT("executable"), bCanOpen) ||
            Object->TryGetBoolField(TEXT("is_executable"), bCanOpen))
        {
            Item.can_open_unreal = bCanOpen;
        }
        else
        {
            Item.can_open_unreal = true;
        }

        return Item;
    }

    void AppendPlanArray(const TArray<TSharedPtr<FJsonValue>>& Array, FUnrealPlanListResponse& OutResponse)
    {
        for (const TSharedPtr<FJsonValue>& Value : Array)
        {
            if (Value.IsValid() && Value->Type == EJson::Object)
            {
                OutResponse.items.Add(MakePlanItemFromObject(Value->AsObject()));
            }
        }
    }

    bool ParsePlanListResponse(const FString& Body, FUnrealPlanListResponse& OutResponse)
    {
        FJsonObjectConverter::JsonObjectStringToUStruct(Body, &OutResponse);
        if (OutResponse.items.Num() > 0)
        {
            if (OutResponse.total == 0)
            {
                OutResponse.total = OutResponse.items.Num();
            }
            return true;
        }

        TArray<TSharedPtr<FJsonValue>> RootArray;
        if (TryDeserializeArray(Body, RootArray))
        {
            AppendPlanArray(RootArray, OutResponse);
            OutResponse.total = OutResponse.items.Num();
            return OutResponse.items.Num() > 0;
        }

        TSharedPtr<FJsonObject> Root;
        if (!TryDeserializeObject(Body, Root))
        {
            return false;
        }

        TryGetObjectInt(Root, TEXT("total"), OutResponse.total);

        const TArray<FString> ArrayFields = { TEXT("items"), TEXT("plans"), TEXT("results") };
        for (const FString& Field : ArrayFields)
        {
            const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
            if (Root->TryGetArrayField(Field, Array))
            {
                AppendPlanArray(*Array, OutResponse);
                if (OutResponse.total == 0)
                {
                    OutResponse.total = OutResponse.items.Num();
                }
                return OutResponse.items.Num() > 0;
            }
        }

        const TSharedPtr<FJsonObject>* DataObject = nullptr;
        if (Root->TryGetObjectField(TEXT("data"), DataObject) && DataObject && DataObject->IsValid())
        {
            TryGetObjectInt(*DataObject, TEXT("total"), OutResponse.total);
            for (const FString& Field : ArrayFields)
            {
                const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
                if ((*DataObject)->TryGetArrayField(Field, Array))
                {
                    AppendPlanArray(*Array, OutResponse);
                    if (OutResponse.total == 0)
                    {
                        OutResponse.total = OutResponse.items.Num();
                    }
                    return OutResponse.items.Num() > 0;
                }
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* DataArray = nullptr;
        if (Root->TryGetArrayField(TEXT("data"), DataArray))
        {
            AppendPlanArray(*DataArray, OutResponse);
            if (OutResponse.total == 0)
            {
                OutResponse.total = OutResponse.items.Num();
            }
            return OutResponse.items.Num() > 0;
        }

        return false;
    }

    FUnrealDeltaVersionItem MakeDeltaVersionFromObject(const TSharedPtr<FJsonObject>& Object, int32 PlanId)
    {
        FUnrealDeltaVersionItem Item;
        Item.plan_id = PlanId;

        if (!Object.IsValid())
        {
            return Item;
        }

        FJsonObjectConverter::JsonObjectToUStruct(Object.ToSharedRef(), FUnrealDeltaVersionItem::StaticStruct(), &Item, 0, 0);

        if (Item.version_id == 0)
        {
            TryGetObjectInt(Object, TEXT("id"), Item.version_id);
        }

        if (Item.id == 0)
        {
            Item.id = Item.version_id;
        }

        if (Item.plan_id == 0)
        {
            Item.plan_id = PlanId;
        }

        if (Item.version == 0)
        {
            TryGetObjectInt(Object, TEXT("version_number"), Item.version);
        }

        if (Item.version == 0)
        {
            TryGetObjectInt(Object, TEXT("delta_version"), Item.version);
        }

        FString StringValue;
        if (Item.title.IsEmpty() &&
            (TryGetObjectString(Object, TEXT("title"), StringValue) ||
             TryGetObjectString(Object, TEXT("display_name"), StringValue)))
        {
            Item.title = StringValue;
        }

        return Item;
    }

    void AppendDeltaVersionArray(const TArray<TSharedPtr<FJsonValue>>& Array, int32 PlanId, FUnrealDeltaVersionListResponse& OutResponse)
    {
        for (const TSharedPtr<FJsonValue>& Value : Array)
        {
            if (!Value.IsValid())
            {
                continue;
            }

            FUnrealDeltaVersionItem Item;
            Item.plan_id = PlanId;

            if (Value->Type == EJson::Number)
            {
                Item.version = static_cast<int32>(Value->AsNumber());
                Item.title = FString::Printf(TEXT("Version %d"), Item.version);
                OutResponse.items.Add(Item);
            }
            else if (Value->Type == EJson::Object)
            {
                OutResponse.items.Add(MakeDeltaVersionFromObject(Value->AsObject(), PlanId));
            }
        }
    }

    bool ParseDeltaVersionListResponse(const FString& Body, int32 PlanId, FUnrealDeltaVersionListResponse& OutResponse)
    {
        FJsonObjectConverter::JsonObjectStringToUStruct(Body, &OutResponse);
        if (OutResponse.items.Num() > 0)
        {
            for (FUnrealDeltaVersionItem& Item : OutResponse.items)
            {
                if (Item.plan_id == 0)
                {
                    Item.plan_id = PlanId;
                }
            }

            if (OutResponse.total == 0)
            {
                OutResponse.total = OutResponse.items.Num();
            }
            return true;
        }

        TArray<TSharedPtr<FJsonValue>> RootArray;
        if (TryDeserializeArray(Body, RootArray))
        {
            AppendDeltaVersionArray(RootArray, PlanId, OutResponse);
            OutResponse.total = OutResponse.items.Num();
            return true;
        }

        TSharedPtr<FJsonObject> Root;
        if (!TryDeserializeObject(Body, Root))
        {
            return false;
        }

        TryGetObjectInt(Root, TEXT("total"), OutResponse.total);

        const TArray<FString> ArrayFields = { TEXT("items"), TEXT("versions"), TEXT("delta_versions"), TEXT("results") };
        for (const FString& Field : ArrayFields)
        {
            const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
            if (Root->TryGetArrayField(Field, Array))
            {
                AppendDeltaVersionArray(*Array, PlanId, OutResponse);
                if (OutResponse.total == 0)
                {
                    OutResponse.total = OutResponse.items.Num();
                }
                return true;
            }
        }

        const TSharedPtr<FJsonObject>* DataObject = nullptr;
        if (Root->TryGetObjectField(TEXT("data"), DataObject) && DataObject && DataObject->IsValid())
        {
            TryGetObjectInt(*DataObject, TEXT("total"), OutResponse.total);
            for (const FString& Field : ArrayFields)
            {
                const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
                if ((*DataObject)->TryGetArrayField(Field, Array))
                {
                    AppendDeltaVersionArray(*Array, PlanId, OutResponse);
                    if (OutResponse.total == 0)
                    {
                        OutResponse.total = OutResponse.items.Num();
                    }
                    return true;
                }
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* DataArray = nullptr;
        if (Root->TryGetArrayField(TEXT("data"), DataArray))
        {
            AppendDeltaVersionArray(*DataArray, PlanId, OutResponse);
            if (OutResponse.total == 0)
            {
                OutResponse.total = OutResponse.items.Num();
            }
            return true;
        }

        return false;
    }

    FUnrealPlanListResponse BuildMockPlanList(const FUnrealPlanSearchParams& Params)
    {
        FUnrealPlanListResponse Response;
        const TArray<int32> RecentOrder = { 10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11 };

        for (int32 PlanId : RecentOrder)
        {
            FUnrealPlanItem Plan;
            Plan.id = PlanId;
            Plan.name = FString::Printf(TEXT("Mock Plan %d"), PlanId);
            Plan.title = Plan.name;
            Plan.file_name = FString::Printf(TEXT("test%d.json"), PlanId);
            Plan.status = TEXT("saved");
            Plan.can_open_unreal = true;

            if (!Params.q.IsEmpty() && !Plan.GetDisplayTitle().Contains(Params.q, ESearchCase::IgnoreCase))
            {
                continue;
            }

            if (!Params.file_name.IsEmpty() && !Plan.file_name.Contains(Params.file_name, ESearchCase::IgnoreCase))
            {
                continue;
            }

            if (!Params.status.IsEmpty() && !Plan.status.Equals(Params.status, ESearchCase::IgnoreCase))
            {
                continue;
            }

            if (!Params.tag.IsEmpty())
            {
                continue;
            }

            Response.items.Add(Plan);
        }

        Response.total = Response.items.Num();

        if (Params.skip > 0 || Params.limit > 0)
        {
            const int32 StartIndex = FMath::Clamp(Params.skip, 0, Response.items.Num());
            const int32 Count = Params.limit > 0 ? FMath::Min(Params.limit, Response.items.Num() - StartIndex) : Response.items.Num() - StartIndex;
            TArray<FUnrealPlanItem> PagedItems;
            for (int32 Index = StartIndex; Index < StartIndex + Count; ++Index)
            {
                PagedItems.Add(Response.items[Index]);
            }
            Response.items = MoveTemp(PagedItems);
        }

        return Response;
    }

    FUnrealDeltaVersionListResponse BuildMockDeltaVersionList(int32 PlanId)
    {
        FUnrealDeltaVersionListResponse Response;

        int32 Version = 1;
        while (true)
        {
            const FString FilePath = FPaths::ProjectContentDir() / TEXT("TestData") / FString::Printf(TEXT("test%d_delta_v%d.json"), PlanId, Version);
            if (!FPaths::FileExists(FilePath))
            {
                break;
            }

            FUnrealDeltaVersionItem Item;
            Item.plan_id = PlanId;
            Item.version = Version;
            Item.version_id = Version;
            Item.id = Version;
            Item.status = TEXT("saved");
            Item.title = FString::Printf(TEXT("Version %d"), Version);
            Response.items.Add(Item);
            Version++;
        }

        if (Response.items.Num() == 0)
        {
            const FString LatestPath = FPaths::ProjectContentDir() / TEXT("TestData") / FString::Printf(TEXT("test%d_delta.json"), PlanId);
            if (FPaths::FileExists(LatestPath))
            {
                FUnrealDeltaVersionItem Latest;
                Latest.plan_id = PlanId;
                Latest.version = 0;
                Latest.title = TEXT("Latest");
                Latest.is_latest = true;
                Response.items.Add(Latest);
            }
        }

        Response.total = Response.items.Num();
        return Response;
    }
}

void UInteRealNetworkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

FString UInteRealNetworkSubsystem::BuildEndpoint(FString EndpointFormat, int32 PlanId, int32 Version) const
{
    EndpointFormat.ReplaceInline(TEXT("{PlanId}"), *FString::FromInt(PlanId));
    EndpointFormat.ReplaceInline(TEXT("{planId}"), *FString::FromInt(PlanId));
    EndpointFormat.ReplaceInline(TEXT("{id}"), *FString::FromInt(PlanId));

    if (Version != INDEX_NONE)
    {
        EndpointFormat.ReplaceInline(TEXT("{Version}"), *FString::FromInt(Version));
        EndpointFormat.ReplaceInline(TEXT("{version}"), *FString::FromInt(Version));
    }

    return EndpointFormat;
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UInteRealNetworkSubsystem::CreateRequest(const FString& Verb, const FString& Endpoint)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetVerb(Verb);
    Request->SetURL(GetBaseApiUrl() + Endpoint);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    
    // 가이드 필수 사항: 모든 엔드포인트 JWT 인증
    if (!AuthToken.IsEmpty())
    {
        Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    }
    
    return Request;
}

// --- 1. 에셋 API 구현 ---

void UInteRealNetworkSubsystem::FetchAllAssets(int32 Skip, int32 Limit, FOnAssetsReceived OnComplete)
{
    if (bUseMockData)
    {
        FUnrealAssetListResponse Res;
        Res.total = 1; Res.skip = Skip; Res.limit = Limit;
        FInteRealAssetData A; A.id = 1; A.name = TEXT("MockAsset"); A.unreal_path = TEXT("/Game/Mock");
        Res.items.Add(A);
        OnComplete.ExecuteIfBound(true, Res); return;
    }

    auto Req = CreateRequest(TEXT("GET"), FString::Printf(TEXT("/assets?skip=%d&limit=%d"), Skip, Limit));
    Req->OnProcessRequestComplete().BindLambda([OnComplete](FHttpRequestPtr, FHttpResponsePtr R, bool S) {
        FUnrealAssetListResponse Res;
        if (S && R.IsValid() && EHttpResponseCodes::IsOk(R->GetResponseCode())) {
            FJsonObjectConverter::JsonObjectStringToUStruct(R->GetContentAsString(), &Res);
            OnComplete.ExecuteIfBound(true, Res);
        } else OnComplete.ExecuteIfBound(false, Res);
    });
    Req->ProcessRequest();
}

void UInteRealNetworkSubsystem::FetchAssetsByEstimate(int32 EstimateId, int32 Skip, int32 Limit, FOnAssetsReceived OnComplete)
{
    if (bUseMockData)
    {
        FUnrealAssetListResponse Res; Res.total = 1;
        OnComplete.ExecuteIfBound(true, Res); return;
    }

    auto Req = CreateRequest(TEXT("GET"), FString::Printf(TEXT("/estimates/%d/assets?skip=%d&limit=%d"), EstimateId, Skip, Limit));
    Req->OnProcessRequestComplete().BindLambda([OnComplete](FHttpRequestPtr, FHttpResponsePtr R, bool S) {
        FUnrealAssetListResponse Res;
        if (S && R.IsValid() && EHttpResponseCodes::IsOk(R->GetResponseCode())) {
            FJsonObjectConverter::JsonObjectStringToUStruct(R->GetContentAsString(), &Res);
            OnComplete.ExecuteIfBound(true, Res);
        } else OnComplete.ExecuteIfBound(false, Res);
    });
    Req->ProcessRequest();
}

void UInteRealNetworkSubsystem::FetchAssetsByUser(int32 UserId, int32 Skip, int32 Limit, FOnAssetsReceived OnComplete)
{
    if (bUseMockData)
    {
        FUnrealAssetListResponse Res; Res.total = 1;
        OnComplete.ExecuteIfBound(true, Res); return;
    }

    auto Req = CreateRequest(TEXT("GET"), FString::Printf(TEXT("/users/%d/assets?skip=%d&limit=%d"), UserId, Skip, Limit));
    Req->OnProcessRequestComplete().BindLambda([OnComplete](FHttpRequestPtr, FHttpResponsePtr R, bool S) {
        FUnrealAssetListResponse Res;
        if (S && R.IsValid() && EHttpResponseCodes::IsOk(R->GetResponseCode())) {
            FJsonObjectConverter::JsonObjectStringToUStruct(R->GetContentAsString(), &Res);
            OnComplete.ExecuteIfBound(true, Res);
        } else OnComplete.ExecuteIfBound(false, Res);
    });
    Req->ProcessRequest();
}

// --- 2. 도면 API 구현 ---

void UInteRealNetworkSubsystem::FetchPlans(const FUnrealPlanSearchParams& Params, FOnPlansReceived OnComplete)
{
    FetchExecutablePlans(Params, OnComplete);
    return;

    if (bUseMockData)
    {
        FUnrealPlanListResponse Res;
        // 기존 Harness 테스트용 데이터 매핑 (test1 ~ test11)
        for (int32 i = 1; i <= 11; ++i)
        {
            FUnrealPlanItem P;
            P.id = i;
            P.name = FString::Printf(TEXT("Mock Plan %d"), i);
            P.file_name = FString::Printf(TEXT("test%d.json"), i);
            P.can_open_unreal = true;
            Res.items.Add(P);
        }
        OnComplete.ExecuteIfBound(true, Res); return;
    }

    FString Query = FString::Printf(TEXT("/plans?skip=%d&limit=%d&sort=%s&registration_status=%s"), 
        Params.skip, Params.limit, *Params.sort, *Params.registration_status);
    if (!Params.q.IsEmpty()) Query += FString::Printf(TEXT("&q=%s"), *Params.q);

    auto Req = CreateRequest(TEXT("GET"), Query);
    Req->OnProcessRequestComplete().BindLambda([OnComplete](FHttpRequestPtr, FHttpResponsePtr R, bool S) {
        FUnrealPlanListResponse Res;
        if (S && R.IsValid() && EHttpResponseCodes::IsOk(R->GetResponseCode())) {
            FJsonObjectConverter::JsonObjectStringToUStruct(R->GetContentAsString(), &Res);
            OnComplete.ExecuteIfBound(true, Res);
        } else OnComplete.ExecuteIfBound(false, Res);
    });
    Req->ProcessRequest();
}

void UInteRealNetworkSubsystem::FetchExecutablePlans(const FUnrealPlanSearchParams& Params, FOnPlansReceived OnComplete)
{
    if (bUseMockData)
    {
        OnComplete.ExecuteIfBound(true, BuildMockPlanList(Params));
        return;
    }

    FString Query = PlansEndpoint;
    AppendQueryParam(Query, TEXT("skip"), FString::FromInt(Params.skip));
    AppendQueryParam(Query, TEXT("limit"), FString::FromInt(Params.limit));

    if (!Params.sort.IsEmpty())
    {
        AppendQueryParam(Query, TEXT("sort"), Params.sort);
    }

    if (!Params.registration_status.IsEmpty())
    {
        AppendQueryParam(Query, TEXT("registration_status"), Params.registration_status);
    }

    if (!Params.q.IsEmpty())
    {
        AppendQueryParam(Query, TEXT("q"), Params.q);
    }

    if (!Params.file_name.IsEmpty())
    {
        AppendQueryParam(Query, TEXT("file_name"), Params.file_name);
    }

    if (!Params.tag.IsEmpty())
    {
        AppendQueryParam(Query, TEXT("tag"), Params.tag);
    }

    if (!Params.status.IsEmpty())
    {
        AppendQueryParam(Query, TEXT("status"), Params.status);
    }

    auto Req = CreateRequest(TEXT("GET"), Query);
    Req->OnProcessRequestComplete().BindLambda([OnComplete, bExecutableOnly = Params.executable_only](FHttpRequestPtr, FHttpResponsePtr R, bool S) {
        FUnrealPlanListResponse Res;
        if (IsSuccessfulResponse(R, S)) {
            ParsePlanListResponse(R->GetContentAsString(), Res);
            if (bExecutableOnly)
            {
                Res.items.RemoveAll([](const FUnrealPlanItem& Item) {
                    return !Item.can_open_unreal;
                });
            }
            if (Res.total == 0 || Res.total < Res.items.Num())
            {
                Res.total = Res.items.Num();
            }
            OnComplete.ExecuteIfBound(true, Res);
        } else OnComplete.ExecuteIfBound(false, Res);
    });
    Req->ProcessRequest();
}

void UInteRealNetworkSubsystem::FetchBaseTopology(int32 PlanId, const FUeTopologyExportRequest& ExportParams, FOnTopologyReceived OnComplete)
{
    FetchUeTopology(PlanId, ExportParams, OnComplete);
    return;

    if (bUseMockData)
    {
        // 💡 [Harness 이관] Content/TestData/test{id}.json 로드
        FString FilePath = FPaths::ProjectContentDir() / TEXT("TestData") / FString::Printf(TEXT("test%d.json"), PlanId);
        FString JsonContent;
        if (FFileHelper::LoadFileToString(JsonContent, *FilePath))
        {
            OnComplete.ExecuteIfBound(true, JsonContent);
        }
        else
        {
            OnComplete.ExecuteIfBound(false, TEXT("{}"));
        }
        return;
    }

    FString Query = FString::Printf(TEXT("/plans/%d/base?layout_type=%s&scale_unit=%s"), 
        PlanId, *ExportParams.layout_type, *ExportParams.scale_unit);

    auto Req = CreateRequest(TEXT("GET"), Query);
    Req->OnProcessRequestComplete().BindLambda([OnComplete](FHttpRequestPtr, FHttpResponsePtr R, bool S) {
        if (S && R.IsValid() && EHttpResponseCodes::IsOk(R->GetResponseCode())) {
            OnComplete.ExecuteIfBound(true, R->GetContentAsString());
        } else OnComplete.ExecuteIfBound(false, TEXT(""));
    });
    Req->ProcessRequest();
}

// --- 3. Delta API 구현 ---

void UInteRealNetworkSubsystem::FetchUeTopology(int32 PlanId, const FUeTopologyExportRequest& ExportParams, FOnTopologyReceived OnComplete)
{
    if (bUseMockData)
    {
        const FString FilePath = FPaths::ProjectContentDir() / TEXT("TestData") / FString::Printf(TEXT("test%d.json"), PlanId);
        FString JsonContent;
        if (FFileHelper::LoadFileToString(JsonContent, *FilePath))
        {
            OnComplete.ExecuteIfBound(true, JsonContent);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[Network] Mock topology file not found: %s"), *FilePath);
            OnComplete.ExecuteIfBound(false, TEXT("{}"));
        }
        return;
    }

    FString Query = BuildEndpoint(UeTopologyEndpointFormat, PlanId);
    if (!ExportParams.layout_type.IsEmpty())
    {
        AppendQueryParam(Query, TEXT("layout_type"), ExportParams.layout_type);
    }
    if (!ExportParams.scale_unit.IsEmpty())
    {
        AppendQueryParam(Query, TEXT("scale_unit"), ExportParams.scale_unit);
    }
    if (!ExportParams.reference_coordinate_system.IsEmpty())
    {
        AppendQueryParam(Query, TEXT("reference_coordinate_system"), ExportParams.reference_coordinate_system);
    }
    if (!ExportParams.coordinate_policy.IsEmpty())
    {
        AppendQueryParam(Query, TEXT("coordinate_policy"), ExportParams.coordinate_policy);
    }
    AppendQueryParam(Query, TEXT("default_wall_height_cm"), FString::SanitizeFloat(ExportParams.default_wall_height_cm));
    AppendQueryParam(Query, TEXT("default_door_height_cm"), FString::SanitizeFloat(ExportParams.default_door_height_cm));
    AppendQueryParam(Query, TEXT("default_window_height_cm"), FString::SanitizeFloat(ExportParams.default_window_height_cm));
    AppendQueryParam(Query, TEXT("default_window_sill_height_cm"), FString::SanitizeFloat(ExportParams.default_window_sill_height_cm));
    AppendQueryParam(Query, TEXT("visual_scale_factor_cm_per_px"), FString::SanitizeFloat(ExportParams.visual_scale_factor_cm_per_px));
    AppendQueryParam(Query, TEXT("vertex_merge_tolerance_cm"), FString::SanitizeFloat(ExportParams.vertex_merge_tolerance_cm));

    auto Req = CreateRequest(TEXT("GET"), Query);
    Req->OnProcessRequestComplete().BindLambda([OnComplete](FHttpRequestPtr, FHttpResponsePtr R, bool S) {
        if (IsSuccessfulResponse(R, S)) {
            const FString Payload = ExtractJsonPayload(R->GetContentAsString(), {
                TEXT("ue_topology_json"),
                TEXT("topology_json"),
                TEXT("topology"),
                TEXT("base_json"),
                TEXT("base"),
                TEXT("json"),
                TEXT("data")
            });
            OnComplete.ExecuteIfBound(true, Payload);
        } else OnComplete.ExecuteIfBound(false, TEXT(""));
    });
    Req->ProcessRequest();
}

void UInteRealNetworkSubsystem::FetchDeltaVersions(int32 PlanId, FOnDeltaVersionsReceived OnComplete)
{
    if (bUseMockData)
    {
        OnComplete.ExecuteIfBound(true, BuildMockDeltaVersionList(PlanId));
        return;
    }

    auto Req = CreateRequest(TEXT("GET"), BuildEndpoint(DeltaVersionsEndpointFormat, PlanId));
    Req->OnProcessRequestComplete().BindLambda([OnComplete, PlanId](FHttpRequestPtr, FHttpResponsePtr R, bool S) {
        FUnrealDeltaVersionListResponse Res;
        if (IsSuccessfulResponse(R, S)) {
            ParseDeltaVersionListResponse(R->GetContentAsString(), PlanId, Res);
            OnComplete.ExecuteIfBound(true, Res);
        } else OnComplete.ExecuteIfBound(false, Res);
    });
    Req->ProcessRequest();
}

void UInteRealNetworkSubsystem::FetchLatestDelta(int32 PlanId, FOnDeltaReceived OnComplete)
{
    if (bUseMockData)
    {
        // 💡 [Harness 이관] Content/TestData/test{id}_delta.json 로드
        FString FilePath = FPaths::ProjectContentDir() / TEXT("TestData") / FString::Printf(TEXT("test%d_delta.json"), PlanId);
        FString JsonContent;
        if (FFileHelper::LoadFileToString(JsonContent, *FilePath))
        {
            OnComplete.ExecuteIfBound(true, JsonContent);
        }
        else
        {
            OnComplete.ExecuteIfBound(true, TEXT("{}"));
        }
        return;
    }

    auto Req = CreateRequest(TEXT("GET"), BuildEndpoint(LatestDeltaEndpointFormat, PlanId));
    Req->OnProcessRequestComplete().BindLambda([OnComplete](FHttpRequestPtr, FHttpResponsePtr R, bool S) {
        if (IsSuccessfulResponse(R, S)) {
            const FString Payload = ExtractJsonPayload(R->GetContentAsString(), {
                TEXT("delta_json"),
                TEXT("delta"),
                TEXT("style_json"),
                TEXT("style"),
                TEXT("json"),
                TEXT("data")
            });
            OnComplete.ExecuteIfBound(true, Payload);
        } else OnComplete.ExecuteIfBound(false, TEXT(""));
    });
    Req->ProcessRequest();
}

void UInteRealNetworkSubsystem::FetchDeltaByVersion(int32 PlanId, int32 Version, FOnDeltaReceived OnComplete)
{
    if (bUseMockData)
    {
        // 로컬 버전 파일 로드 시도
        FString FilePath = FPaths::ProjectContentDir() / TEXT("TestData") / FString::Printf(TEXT("test%d_delta_v%d.json"), PlanId, Version);
        FString JsonContent;
        if (FFileHelper::LoadFileToString(JsonContent, *FilePath))
        {
            OnComplete.ExecuteIfBound(true, JsonContent);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[Network] Mock version file not found: %s. Falling back to latest."), *FilePath);
            FetchLatestDelta(PlanId, OnComplete);
        }
        return;
    }

    auto Req = CreateRequest(TEXT("GET"), BuildEndpoint(DeltaVersionEndpointFormat, PlanId, Version));
    Req->OnProcessRequestComplete().BindLambda([OnComplete](FHttpRequestPtr, FHttpResponsePtr R, bool S) {
        if (IsSuccessfulResponse(R, S)) {
            const FString Payload = ExtractJsonPayload(R->GetContentAsString(), {
                TEXT("delta_json"),
                TEXT("delta"),
                TEXT("style_json"),
                TEXT("style"),
                TEXT("json"),
                TEXT("data")
            });
            OnComplete.ExecuteIfBound(true, Payload);
        } else OnComplete.ExecuteIfBound(false, TEXT(""));
    });
    Req->ProcessRequest();
}

void UInteRealNetworkSubsystem::SaveDelta(int32 PlanId, const FString& DeltaJson, FOnDeltaSaved OnComplete)
{
    UE_LOG(LogTemp, Log, TEXT("[Network] SaveDelta Request - Plan: %d, MockMode: %s, DataSize: %d"), 
        PlanId, bUseMockData ? TEXT("TRUE") : TEXT("FALSE"), DeltaJson.Len());

    if (bUseMockData)
    {
        // 1. 최신 상태 유지를 위해 기본 delta.json 덮어쓰기
        FString BaseRelativePath = FPaths::ProjectContentDir() / TEXT("TestData") / FString::Printf(TEXT("test%d_delta.json"), PlanId);
        FString BaseAbsolutePath = FPaths::ConvertRelativePathToFull(BaseRelativePath);
        bool bSavedBase = FFileHelper::SaveStringToFile(DeltaJson, *BaseAbsolutePath);

        // 2. 로컬 버전 관리를 위해 비어있는 v{번호} 파일 찾아서 생성
        int32 Version = 1;
        FString VersionAbsolutePath;
        while (true)
        {
            FString VersionRelativePath = FPaths::ProjectContentDir() / TEXT("TestData") / FString::Printf(TEXT("test%d_delta_v%d.json"), PlanId, Version);
            VersionAbsolutePath = FPaths::ConvertRelativePathToFull(VersionRelativePath);
            if (!FPaths::FileExists(VersionAbsolutePath))
            {
                break;
            }
            Version++;
        }
        
        bool bSavedVersion = FFileHelper::SaveStringToFile(DeltaJson, *VersionAbsolutePath);
        bool bSaved = bSavedBase && bSavedVersion;

        if (bSaved)
        {
            UE_LOG(LogTemp, Log, TEXT("[Network] Successfully saved mock delta to: %s (Version: %d)"), *VersionAbsolutePath, Version);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[Network] FAILED to save mock delta. Check path or permissions."));
        }
        
        FUnrealOkResponse Res; Res.ok = bSaved;
        OnComplete.ExecuteIfBound(bSaved, Res);
        return;
    }

    auto Req = CreateRequest(TEXT("POST"), BuildEndpoint(LatestDeltaEndpointFormat, PlanId));
    Req->SetContentAsString(DeltaJson);
    Req->OnProcessRequestComplete().BindLambda([OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Res, bool bSucceeded) {
        FUnrealOkResponse Response;
        if (bSucceeded && Res.IsValid() && (Res->GetResponseCode() == 200 || Res->GetResponseCode() == 201)) {
            FJsonObjectConverter::JsonObjectStringToUStruct(Res->GetContentAsString(), &Response);
            OnComplete.ExecuteIfBound(true, Response);
        } else {
            OnComplete.ExecuteIfBound(false, Response);
        }
    });
    Req->ProcessRequest();
}
