#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PlacementHistoryHandler.generated.h"

class UInteriorPlacementSubsystem;
class UPlacementSerializer;

UCLASS()
class INTEREAL_API UPlacementHistoryHandler : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UInteriorPlacementSubsystem* InSubsystem);

	void RecordSnapshot();
	void Undo();
	void Redo();
	bool CanUndo() const { return UndoStack.Num() > 0; }
	bool CanRedo() const { return RedoStack.Num() > 0; }

	bool IsRestoringHistory() const { return bRestoringHistory; }

	FString ExportPlacedFurnitureJson() const;
	void ImportPlacedFurnitureJson(const FString& JsonString);
	FString ExportEditStateJson() const;
	void ImportEditStateJson(const FString& JsonString);

	void ReceiveWebCommand(const FString& JsonString);

private:
	UPROPERTY()
	UInteriorPlacementSubsystem* Subsystem = nullptr;

	UPROPERTY()
	TObjectPtr<UPlacementSerializer> Serializer = nullptr;

	static constexpr int32 MaxHistoryCount = 100;
	TArray<FString> UndoStack;
	TArray<FString> RedoStack;
	bool bRestoringHistory = false;

	bool bHasPendingGizmoSnapshot = false;
	FString PendingGizmoSnapshot;

	void PushSnapshot(const FString& Snapshot);

public:
	void BeginGizmoSnapshot();
	void CommitGizmoSnapshot();
	void DiscardGizmoSnapshot();
};
