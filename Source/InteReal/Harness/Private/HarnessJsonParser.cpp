#include "InteReal/Harness/Public/HarnessJsonParser.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    bool DeserializeJsonObject(const FString& JsonString, TSharedPtr<FJsonObject>& OutObject)
    {
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }

    bool LooksLikeFloorDataObject(const TSharedPtr<FJsonObject>& Object)
    {
        return Object.IsValid() && (
            Object->Values.Contains(TEXT("vertices")) ||
            Object->Values.Contains(TEXT("half_edges")) ||
            Object->Values.Contains(TEXT("faces")) ||
            Object->Values.Contains(TEXT("nodes")) ||
            Object->Values.Contains(TEXT("edges")) ||
            Object->Values.Contains(TEXT("spaces")));
    }

    bool TryJsonValueToObject(const TSharedPtr<FJsonValue>& Value, TSharedPtr<FJsonObject>& OutObject)
    {
        if (!Value.IsValid() || Value->Type == EJson::Null)
        {
            return false;
        }

        if (Value->Type == EJson::Object)
        {
            OutObject = Value->AsObject();
            return OutObject.IsValid();
        }

        if (Value->Type == EJson::String)
        {
            return DeserializeJsonObject(Value->AsString(), OutObject);
        }

        return false;
    }

    TSharedPtr<FJsonObject> ExtractFloorDataObject(const TSharedPtr<FJsonObject>& RootObject)
    {
        if (LooksLikeFloorDataObject(RootObject))
        {
            return RootObject;
        }

        static const TArray<FString> CandidateFields = {
            TEXT("ue_topology_json"),
            TEXT("topology_json"),
            TEXT("topology"),
            TEXT("base_json"),
            TEXT("base"),
            TEXT("json"),
            TEXT("data")
        };

        for (const FString& Field : CandidateFields)
        {
            if (const TSharedPtr<FJsonValue>* Value = RootObject->Values.Find(Field))
            {
                TSharedPtr<FJsonObject> CandidateObject;
                if (TryJsonValueToObject(*Value, CandidateObject) && LooksLikeFloorDataObject(CandidateObject))
                {
                    return CandidateObject;
                }
            }
        }

        const TSharedPtr<FJsonObject>* DataObject = nullptr;
        if (RootObject->TryGetObjectField(TEXT("data"), DataObject) && DataObject && DataObject->IsValid())
        {
            for (const FString& Field : CandidateFields)
            {
                if (const TSharedPtr<FJsonValue>* Value = (*DataObject)->Values.Find(Field))
                {
                    TSharedPtr<FJsonObject> CandidateObject;
                    if (TryJsonValueToObject(*Value, CandidateObject) && LooksLikeFloorDataObject(CandidateObject))
                    {
                        return CandidateObject;
                    }
                }
            }
        }

        return RootObject;
    }

    void CopyFieldIfMissing(const TSharedPtr<FJsonObject>& Object, const FString& TargetField, const TArray<FString>& AliasFields)
    {
        if (!Object.IsValid() || Object->Values.Contains(TargetField))
        {
            return;
        }

        for (const FString& AliasField : AliasFields)
        {
            if (const TSharedPtr<FJsonValue>* Value = Object->Values.Find(AliasField))
            {
                Object->SetField(TargetField, *Value);
                return;
            }
        }
    }

    void NormalizeFloorDataObject(const TSharedPtr<FJsonObject>& Object)
    {
        CopyFieldIfMissing(Object, TEXT("project_info"), { TEXT("projectInfo"), TEXT("project") });
        CopyFieldIfMissing(Object, TEXT("vertices"), { TEXT("nodes") });
        CopyFieldIfMissing(Object, TEXT("half_edges"), { TEXT("halfEdges"), TEXT("edges") });
        CopyFieldIfMissing(Object, TEXT("faces"), { TEXT("spaces"), TEXT("rooms") });
        CopyFieldIfMissing(Object, TEXT("wall_side_measurements"), { TEXT("wallSideMeasurements") });
        CopyFieldIfMissing(Object, TEXT("surface_measurements"), { TEXT("surfaceMeasurements") });
    }
}

bool FHarnessJsonParser::LoadFloorDataFromJsonFile(const FString& FilePath, FHarnessFloorData& OutData, FString& OutError)
{
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        OutError = FString::Printf(TEXT("Failed to load file from path: %s"), *FilePath);
        return false;
    }

    return ParseFloorDataFromJsonString(JsonString, OutData, OutError);
}

bool FHarnessJsonParser::ParseFloorDataFromJsonString(const FString& JsonString, FHarnessFloorData& OutData, FString& OutError)
{
    OutData = FHarnessFloorData();

    TSharedPtr<FJsonObject> RootObject;
    if (!DeserializeJsonObject(JsonString, RootObject))
    {
        OutError = TEXT("JSON Deserialization failed. Invalid JSON format.");
        return false;
    }

    TSharedPtr<FJsonObject> FloorDataObject = ExtractFloorDataObject(RootObject);
    NormalizeFloorDataObject(FloorDataObject);

    if (!FJsonObjectConverter::JsonObjectToUStruct(FloorDataObject.ToSharedRef(), FHarnessFloorData::StaticStruct(), &OutData, 0, 0))
    {
        OutError = TEXT("Failed to map JSON fields to Topology Graph structure. Schema mismatch.");
        return false;
    }

    if (OutData.vertices.Num() == 0)
    {
        OutError = TEXT("Data integrity warning: Parsed topology contains zero vertices.");
        return false;
    }

    if (OutData.half_edges.Num() == 0)
    {
        OutError = TEXT("Data integrity warning: Parsed topology contains zero half_edges.");
        return false;
    }

    return true;
}
