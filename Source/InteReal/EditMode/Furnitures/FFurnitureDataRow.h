#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FFurnitureDataRow.generated.h"

class AFurniture;
class UStaticMesh;
class UTexture2D;

USTRUCT(BlueprintType)
struct FFurnitureDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 ID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName = FText::FromString(TEXT("Furniture"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UTexture2D> DisplayImage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMesh> FurnitureMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FIntPoint Dimensions = FIntPoint(1, 1);
};
