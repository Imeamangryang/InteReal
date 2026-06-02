#include "Public/HarnessNetworkComponent.h"

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UHarnessNetworkComponent::UHarnessNetworkComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	BaseUrl = TEXT("https://api.example.com/api/harness"); // Replace with actual API endpoint
}

void UHarnessNetworkComponent::RequestFloorPlanList()
{
	if (bUseMockData)
	{
		TArray<FFloorPlanInfo> MockPlans;
		TArray<FString> FileNames = {TEXT("test1"), TEXT("test2"), TEXT("test3"), TEXT("test4"), TEXT("test5"), TEXT("test6"), TEXT("test7"), TEXT("test8"), TEXT("test9"), TEXT("test10"), TEXT("test11")};
		for (const FString& Name : FileNames)
		{
			FFloorPlanInfo Info;
			Info.PlanId = Name;
			Info.PlanName = FString::Printf(TEXT("Mock %s"), *Name);
			MockPlans.Add(Info);
		}
		OnPlanListReceived.Broadcast(MockPlans);
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UHarnessNetworkComponent::OnListResponseReceived);
	Request->SetURL(BaseUrl + TEXT("/plans"));
	Request->SetVerb(TEXT("GET"));
	Request->ProcessRequest();
}

void UHarnessNetworkComponent::DownloadFloorPlanBase(const FString& PlanId)
{
	if (bUseMockData)
	{
		FString FilePath = FPaths::ProjectContentDir() / TEXT("TestData") / (PlanId + TEXT(".json"));
		FString JsonContent;
		if (FFileHelper::LoadFileToString(JsonContent, *FilePath))
		{
			OnPlanBaseDownloaded.Broadcast(JsonContent);
		}
		else
		{
			// Fallback or error handling
			OnPlanBaseDownloaded.Broadcast(TEXT("{}")); 
		}
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UHarnessNetworkComponent::OnBaseResponseReceived);
	Request->SetURL(BaseUrl + TEXT("/plans/") + PlanId + TEXT("/base"));
	Request->SetVerb(TEXT("GET"));
	Request->ProcessRequest();
}

void UHarnessNetworkComponent::DownloadFloorPlanDelta(const FString& PlanId)
{
	if (bUseMockData)
	{
		// 💡 [내부 테스트용] 이전에 저장된 로컬 파일이 있는지 확인
		FString FilePath = FPaths::ProjectContentDir() / TEXT("TestData") / (PlanId + TEXT("_delta.json"));
		FString JsonContent;
		
		if (FFileHelper::LoadFileToString(JsonContent, *FilePath))
		{
			UE_LOG(LogTemp, Log, TEXT("[Harness] 로컬 저장된 델타 파일 로드 완료: %s"), *FilePath);
			OnPlanDeltaDownloaded.Broadcast(JsonContent);
		}
		else
		{
			// 저장된 파일이 없으면 빈 데이터 반환
			OnPlanDeltaDownloaded.Broadcast(TEXT("{}"));
		}
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UHarnessNetworkComponent::OnDeltaResponseReceived);
	Request->SetURL(BaseUrl + TEXT("/plans/") + PlanId + TEXT("/delta"));
	Request->SetVerb(TEXT("GET"));
	Request->ProcessRequest();
}

void UHarnessNetworkComponent::UploadFloorPlanDelta(const FString& PlanId, const FString& JsonString)
{
	if (bUseMockData)
	{
		// 💡 [내부 테스트용] 서버 대신 로컬 파일로 저장 (Content/TestData/파일명_delta.json)
		FString FilePath = FPaths::ProjectContentDir() / TEXT("TestData") / (PlanId + TEXT("_delta.json"));
		
		bool bSuccess = FFileHelper::SaveStringToFile(JsonString, *FilePath);
		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("[Harness] 내부 테스트 저장 성공: %s"), *FilePath);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[Harness] 내부 테스트 저장 실패: 경로를 확인하세요."));
		}

		OnPlanDeltaUploaded.Broadcast(bSuccess);
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UHarnessNetworkComponent::OnUploadResponseReceived);
	Request->SetURL(BaseUrl + TEXT("/plans/") + PlanId + TEXT("/delta"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(JsonString);
	Request->ProcessRequest();
}

void UHarnessNetworkComponent::OnListResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	TArray<FFloorPlanInfo> Plans;
	if (bConnectedSuccessfully && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		TSharedPtr<FJsonValue> JsonValue;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
		if (FJsonSerializer::Deserialize(Reader, JsonValue) && JsonValue->Type == EJson::Array)
		{
			for (const TSharedPtr<FJsonValue>& Val : JsonValue->AsArray())
			{
				const TSharedPtr<FJsonObject> Obj = Val->AsObject();
				if (Obj.IsValid())
				{
					FFloorPlanInfo Info;
					Obj->TryGetStringField(TEXT("id"), Info.PlanId);
					Obj->TryGetStringField(TEXT("name"), Info.PlanName);
					Plans.Add(Info);
				}
			}
		}
	}
	OnPlanListReceived.Broadcast(Plans);
}

void UHarnessNetworkComponent::OnBaseResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	FString ResultJson;
	if (bConnectedSuccessfully && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		ResultJson = Response->GetContentAsString();
	}
	OnPlanBaseDownloaded.Broadcast(ResultJson);
}

void UHarnessNetworkComponent::OnDeltaResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	FString ResultJson;
	if (bConnectedSuccessfully && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		ResultJson = Response->GetContentAsString();
	}
	OnPlanDeltaDownloaded.Broadcast(ResultJson);
}

void UHarnessNetworkComponent::OnUploadResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	bool bSuccess = bConnectedSuccessfully && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode());
	OnPlanDeltaUploaded.Broadcast(bSuccess);
}
