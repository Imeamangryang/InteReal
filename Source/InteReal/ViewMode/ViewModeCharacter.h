#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ViewModeCharacter.generated.h"

class UCameraComponent;

UCLASS()
class INTEREAL_API AViewModeCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AViewModeCharacter();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ViewMode")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	// 눈높이 설정 (에디터에서 수정 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewMode")
	float EyeHeight = 160.0f;
};
