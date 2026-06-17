#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PlacementSerializer.generated.h"

class FJsonValue;
class UInteriorPlacementSubsystem;
class UMeshComponent;

UCLASS()
class INTEREAL_API UPlacementSerializer : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UInteriorPlacementSubsystem* InSubsystem);

	FString ExportPlacedFurnituresJson() const;
	void ImportPlacedFurnituresJson(const FString& JsonString);

	FString ExportEditStateJson() const;
	void ImportEditStateJson(const FString& JsonString);

private:
	UPROPERTY()
	TObjectPtr<UInteriorPlacementSubsystem> Subsystem = nullptr;

	void ExportSurfaceMaterials(TArray<TSharedPtr<FJsonValue>>& OutArray) const;
	void ImportSurfaceMaterials(const TArray<TSharedPtr<FJsonValue>>& SurfaceArray);
	bool IsEditableSurfaceComponent(const UMeshComponent* MeshComp) const;
};
