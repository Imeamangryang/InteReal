#include "InteRealNetworkSubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

void UInteRealNetworkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
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

void UInteRealNetworkSubsystem::FetchBaseTopology(int32 PlanId, const FUeTopologyExportRequest& ExportParams, FOnTopologyReceived OnComplete)
{
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

    auto Req = CreateRequest(TEXT("GET"), FString::Printf(TEXT("/plans/%d/delta"), PlanId));
    Req->OnProcessRequestComplete().BindLambda([OnComplete](FHttpRequestPtr, FHttpResponsePtr R, bool S) {
        if (S && R.IsValid() && EHttpResponseCodes::IsOk(R->GetResponseCode())) {
            OnComplete.ExecuteIfBound(true, R->GetContentAsString());
        } else OnComplete.ExecuteIfBound(false, TEXT(""));
    });
    Req->ProcessRequest();
}

void UInteRealNetworkSubsystem::SaveDelta(int32 PlanId, const FString& DeltaJson, FOnDeltaSaved OnComplete)
{
    if (bUseMockData)
    {
        // 💡 [Harness 이관] Content/TestData/test{id}_delta.json 저장
        FString FilePath = FPaths::ProjectContentDir() / TEXT("TestData") / FString::Printf(TEXT("test%d_delta.json"), PlanId);
        bool bSaved = FFileHelper::SaveStringToFile(DeltaJson, *FilePath);
        
        FUnrealOkResponse Res; Res.ok = bSaved;
        OnComplete.ExecuteIfBound(bSaved, Res);
        return;
    }

    auto Req = CreateRequest(TEXT("POST"), FString::Printf(TEXT("/plans/%d/delta"), PlanId));
    Req->SetContentAsString(DeltaJson);
    Req->OnProcessRequestComplete().BindLambda([OnComplete](FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSucceeded) {
        FUnrealOkResponse Response;
        if (bSucceeded && Res.IsValid() && (Res->GetResponseCode() == 200 || Res->GetResponseCode() == 201)) {
            FJsonObjectConverter::JsonObjectStringToUStruct(Res->GetContentAsString(), &Response);
            OnComplete.ExecuteIfBound(true, Response);
        } else OnComplete.ExecuteIfBound(false, Response);
    });
    Req->ProcessRequest();
}
