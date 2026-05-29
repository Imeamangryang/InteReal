#include "InteReal/Harness/Public/HarnessJsonParser.h"

#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

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
    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        OutError = TEXT("JSON Deserialization failed. Invalid JSON format.");
        return false;
    }

    // [중요] 최신 FHarnessFloorData 구조체의 StaticStruct를 넘겨주어 
    // 내부의 nodes, edges, openings, spaces 배열을 계층형으로 자동 매핑합니다.
    if (!FJsonObjectConverter::JsonObjectToUStruct(RootObject.ToSharedRef(), FHarnessFloorData::StaticStruct(), &OutData, 0, 0))
    {
        OutError = TEXT("Failed to map JSON fields to Topology Graph structure. Schema mismatch.");
        return false;
    }

    // 필수 데이터 무결성 검증 (정준님의 시스템 안정성을 위한 방어 코드)
    if (OutData.vertices.Num() == 0)
    {
        OutError = TEXT("Data integrity warning: Parsed topology contains zero vertices.");
        return false;
    }

    return true;
}