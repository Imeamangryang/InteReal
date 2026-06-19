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

    struct FJsonFieldAlias
    {
        FString TargetField;
        TArray<FString> AliasFields;
    };

    FJsonFieldAlias MakeFieldAlias(const FString& TargetField, const FString& AliasField)
    {
        FJsonFieldAlias Result;
        Result.TargetField = TargetField;
        Result.AliasFields.Add(AliasField);
        return Result;
    }

    void NormalizeObjectFields(const TSharedPtr<FJsonObject>& Object, const TArray<FJsonFieldAlias>& FieldAliases)
    {
        if (!Object.IsValid())
        {
            return;
        }

        for (const FJsonFieldAlias& FieldAlias : FieldAliases)
        {
            CopyFieldIfMissing(Object, FieldAlias.TargetField, FieldAlias.AliasFields);
        }
    }

    void NormalizeArrayObjectFields(const TSharedPtr<FJsonObject>& Object, const FString& ArrayField, const TArray<FJsonFieldAlias>& FieldAliases)
    {
        if (!Object.IsValid())
        {
            return;
        }

        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object->TryGetArrayField(ArrayField, Values) || !Values)
        {
            return;
        }

        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            if (Value.IsValid() && Value->Type == EJson::Object)
            {
                NormalizeObjectFields(Value->AsObject(), FieldAliases);
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

        NormalizeArrayObjectFields(Object, TEXT("openings"), {
            MakeFieldAlias(TEXT("width_cm"), TEXT("width_mm")),
            MakeFieldAlias(TEXT("height_cm"), TEXT("height_mm")),
            MakeFieldAlias(TEXT("z_offset_cm"), TEXT("z_offset_mm"))
        });

        NormalizeArrayObjectFields(Object, TEXT("faces"), {
            MakeFieldAlias(TEXT("height_cm"), TEXT("height_mm")),
            MakeFieldAlias(TEXT("z_offset"), TEXT("z_offset_mm"))
        });

        NormalizeArrayObjectFields(Object, TEXT("half_edges"), {
            MakeFieldAlias(TEXT("wall_thickness"), TEXT("wall_thickness_mm"))
        });

        NormalizeArrayObjectFields(Object, TEXT("wall_side_measurements"), {
            MakeFieldAlias(TEXT("length_cm"), TEXT("length_mm"))
        });

        NormalizeArrayObjectFields(Object, TEXT("surface_measurements"), {
            MakeFieldAlias(TEXT("start_distance_cm"), TEXT("start_distance_mm")),
            MakeFieldAlias(TEXT("end_distance_cm"), TEXT("end_distance_mm")),
            MakeFieldAlias(TEXT("length_cm"), TEXT("length_mm"))
        });
    }

    float GetLengthUnitToCentimeters(const FString& Unit)
    {
        if (Unit.Equals(TEXT("mm"), ESearchCase::IgnoreCase) ||
            Unit.Equals(TEXT("millimeter"), ESearchCase::IgnoreCase) ||
            Unit.Equals(TEXT("millimeters"), ESearchCase::IgnoreCase))
        {
            return 0.1f;
        }

        if (Unit.Equals(TEXT("m"), ESearchCase::IgnoreCase) ||
            Unit.Equals(TEXT("meter"), ESearchCase::IgnoreCase) ||
            Unit.Equals(TEXT("meters"), ESearchCase::IgnoreCase))
        {
            return 100.0f;
        }

        return 1.0f;
    }

    void ConvertFloorDataToCentimeters(FHarnessFloorData& Data)
    {
        const float UnitToCmScale = GetLengthUnitToCentimeters(Data.project_info.scale_unit);
        if (FMath::IsNearlyEqual(UnitToCmScale, 1.0f, UE_SMALL_NUMBER))
        {
            Data.project_info.scale_unit = TEXT("cm");
            return;
        }

        for (FTopologyVertex& Vertex : Data.vertices)
        {
            Vertex.x *= UnitToCmScale;
            Vertex.y *= UnitToCmScale;
        }

        for (FTopologyHalfEdge& Edge : Data.half_edges)
        {
            Edge.wall_thickness *= UnitToCmScale;
        }

        for (FTopologyOpening& Opening : Data.openings)
        {
            Opening.width_cm *= UnitToCmScale;
            Opening.height_cm *= UnitToCmScale;
            Opening.z_offset_cm *= UnitToCmScale;
        }

        for (FTopologyFace& Face : Data.faces)
        {
            Face.height_cm *= UnitToCmScale;
            Face.z_offset *= UnitToCmScale;
        }

        for (FTopologyWallSideMeasurement& Measurement : Data.wall_side_measurements)
        {
            Measurement.length_cm *= UnitToCmScale;
        }

        for (FTopologySurfaceMeasurement& Measurement : Data.surface_measurements)
        {
            Measurement.start_distance_cm *= UnitToCmScale;
            Measurement.end_distance_cm *= UnitToCmScale;
            Measurement.length_cm *= UnitToCmScale;
            Measurement.start_point.x *= UnitToCmScale;
            Measurement.start_point.y *= UnitToCmScale;
            Measurement.end_point.x *= UnitToCmScale;
            Measurement.end_point.y *= UnitToCmScale;
        }

        Data.project_info.scale_unit = TEXT("cm");
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

    ConvertFloorDataToCentimeters(OutData);

    return true;
}
