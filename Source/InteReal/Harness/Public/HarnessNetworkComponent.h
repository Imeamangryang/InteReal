#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IHttpRequest.h"
#include "HarnessDataTypes.h"
#include "HarnessNetworkComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlanListReceived, const TArray<FFloorPlanInfo>&, PlanList);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlanBaseDownloaded, const FString&, BaseJson);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlanDeltaDownloaded, const FString&, DeltaJson);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlanDeltaUploaded, bool, bSuccess);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class INTEREAL_API UHarnessNetworkComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UHarnessNetworkComponent();

	UFUNCTION(BlueprintCallable, Category="Harness|Network")
	void RequestFloorPlanList();

	UFUNCTION(BlueprintCallable, Category="Harness|Network")
	void DownloadFloorPlanBase(const FString& PlanId);

	UFUNCTION(BlueprintCallable, Category="Harness|Network")
	void DownloadFloorPlanDelta(const FString& PlanId);

	UFUNCTION(BlueprintCallable, Category="Harness|Network")
	void UploadFloorPlanDelta(const FString& PlanId, const FString& JsonString);

	UPROPERTY(BlueprintAssignable, Category="Harness|Network")
	FOnPlanListReceived OnPlanListReceived;

	UPROPERTY(BlueprintAssignable, Category="Harness|Network")
	FOnPlanBaseDownloaded OnPlanBaseDownloaded;

	UPROPERTY(BlueprintAssignable, Category="Harness|Network")
	FOnPlanDeltaDownloaded OnPlanDeltaDownloaded;

	UPROPERTY(BlueprintAssignable, Category="Harness|Network")
	FOnPlanDeltaUploaded OnPlanDeltaUploaded;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness|Network")
	bool bUseMockData = true;

private:
	FString BaseUrl;

	void OnListResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
	void OnBaseResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
	void OnDeltaResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
	void OnUploadResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
};
