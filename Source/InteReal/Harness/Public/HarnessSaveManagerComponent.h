#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HarnessDataTypes.h"
#include "Engine/DataTable.h"
#include "HarnessSaveManagerComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class INTEREAL_API UHarnessSaveManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UHarnessSaveManagerComponent();

	UFUNCTION(BlueprintCallable, Category="Harness|Save")
	FString SaveInteriorState();

	UFUNCTION(BlueprintCallable, Category="Harness|Save")
	void LoadInteriorState(const FString& JsonString);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Harness|Save")
	TObjectPtr<UDataTable> FurnitureDataTable;

private:
	void ClearInterior();
};
