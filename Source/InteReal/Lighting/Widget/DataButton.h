#pragma once
#include "CoreMinimal.h"
#include "Components/Button.h"
#include "DataButton.generated.h"

UCLASS()
class INTEREAL_API UDataButton : public UButton {
	GENERATED_BODY()
public:
	// 여기에 절기 이름이나 도시 이름을 저장합니다.
	UPROPERTY() FString ButtonData; 
};