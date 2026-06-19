#include "PlacementHistoryHandler.h"
#include "PlacementSerializer.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void UPlacementHistoryHandler::Initialize(UInteriorPlacementSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
	Serializer = NewObject<UPlacementSerializer>(this);
	Serializer->Initialize(InSubsystem);
}

void UPlacementHistoryHandler::RecordSnapshot()
{
	PushSnapshot(ExportEditStateJson());
}

void UPlacementHistoryHandler::PushSnapshot(const FString& Snapshot)
{
	if (Snapshot.IsEmpty())
	{
		return;
	}
	if (UndoStack.Num() > 0 && UndoStack.Last() == Snapshot)
	{
		return;
	}

	UndoStack.Add(Snapshot);
	RedoStack.Empty();

	if (UndoStack.Num() > MaxHistoryCount)
	{
		UndoStack.RemoveAt(0);
	}
}

void UPlacementHistoryHandler::Undo()
{
	if (UndoStack.IsEmpty())
	{
		return;
	}

	const FString CurrentSnapshot = ExportEditStateJson();
	RedoStack.Add(CurrentSnapshot);
	const FString Previous = UndoStack.Pop();

	bRestoringHistory = true;
	ImportEditStateJson(Previous);
	bRestoringHistory = false;
}

void UPlacementHistoryHandler::Redo()
{
	if (RedoStack.IsEmpty())
	{
		return;
	}

	const FString CurrentSnapshot = ExportEditStateJson();
	UndoStack.Add(CurrentSnapshot);
	const FString Next = RedoStack.Pop();

	bRestoringHistory = true;
	ImportEditStateJson(Next);
	bRestoringHistory = false;
}

void UPlacementHistoryHandler::BeginGizmoSnapshot()
{
	PendingGizmoSnapshot = ExportEditStateJson();
	bHasPendingGizmoSnapshot = true;
}

void UPlacementHistoryHandler::CommitGizmoSnapshot()
{
	if (bHasPendingGizmoSnapshot)
	{
		PushSnapshot(PendingGizmoSnapshot);
	}
	bHasPendingGizmoSnapshot = false;
	PendingGizmoSnapshot.Empty();
}

void UPlacementHistoryHandler::DiscardGizmoSnapshot()
{
	bHasPendingGizmoSnapshot = false;
	PendingGizmoSnapshot.Empty();
}

FString UPlacementHistoryHandler::ExportPlacedFurnitureJson() const
{
	return Serializer ? Serializer->ExportPlacedFurnituresJson() : FString();
}

void UPlacementHistoryHandler::ImportPlacedFurnitureJson(const FString& JsonString)
{
	if (!bRestoringHistory)
	{
		RecordSnapshot();
	}
	if (Serializer)
	{
		Serializer->ImportPlacedFurnituresJson(JsonString);
	}
}

FString UPlacementHistoryHandler::ExportEditStateJson() const
{
	return Serializer ? Serializer->ExportEditStateJson() : FString();
}

void UPlacementHistoryHandler::ImportEditStateJson(const FString& JsonString)
{
	if (Serializer)
	{
		Serializer->ImportEditStateJson(JsonString);
	}
}

void UPlacementHistoryHandler::ReceiveWebCommand(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return;
	}

	FString Action;
	if (!Root->TryGetStringField(TEXT("action"), Action))
	{
		return;
	}

	if (Action == TEXT("LOAD"))
	{
		FString Payload;
		if (Root->TryGetStringField(TEXT("data"), Payload))
		{
			ImportPlacedFurnitureJson(Payload);
		}
	}
}
