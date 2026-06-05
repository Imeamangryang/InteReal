#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FMaterialDataRow.generated.h"

class UTexture2D;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct FMaterialDataRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	int32 ID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	TObjectPtr<UTexture2D> DisplayImage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	TObjectPtr<UMaterialInterface> Material = nullptr;
};