#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BackgroundManager.generated.h"

UCLASS()
class INTEREAL_API ABackgroundManager : public AActor
{
	GENERATED_BODY()

public: 
	ABackgroundManager();

protected:
	virtual void BeginPlay() override;

public:
	// 태그된 모든 도시 액터를 찾아 가시성을 설정합니다.
	UFUNCTION(BlueprintCallable, Category = "Harness|Visibility")
	void SetBackgroundVisibility(bool bIsVisible);
};