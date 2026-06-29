#include "BackgroundManager.h"
#include "Kismet/GameplayStatics.h"

ABackgroundManager::ABackgroundManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ABackgroundManager::BeginPlay()
{
	Super::BeginPlay();
	// 게임 시작 시 도시가 안 보이도록 초기화
	SetBackgroundVisibility(false);
}

void ABackgroundManager::SetBackgroundVisibility(bool bIsVisible)
{
	// 'CityMesh' 태그가 붙은 모든 액터를 검색
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("CityMesh"), FoundActors);

	int32 FoundCount = 0;
	for (AActor* Actor : FoundActors)
	{
		if (!Actor) continue;

		// 가시성 및 충돌 설정
		Actor->SetActorHiddenInGame(!bIsVisible);
		Actor->SetActorEnableCollision(bIsVisible);
        
		// 하위 모든 컴포넌트 강제 갱신
		Actor->ForEachComponent<UPrimitiveComponent>(true, [bIsVisible](UPrimitiveComponent* Comp)
		{
			Comp->SetVisibility(bIsVisible, true);
			Comp->MarkRenderStateDirty(); 
			Comp->UpdateComponentToWorld();
		});
		FoundCount++;
	}

	UE_LOG(LogTemp, Warning, TEXT(">>> [BackgroundManager] 배경 가시성 제어 완료. 액터 수: %d, 상태: %s"), 
		FoundCount, bIsVisible ? TEXT("켜짐") : TEXT("꺼짐"));
}