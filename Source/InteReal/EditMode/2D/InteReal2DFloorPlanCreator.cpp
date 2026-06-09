#include "InteReal2DFloorPlanCreator.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"
#include "InteReal2DFloorPlanConverter.h"

bool UInteReal2DFloorPlanCreator::BuildFromJson(const FString& JsonData)
{
	FHarnessFloorData ParsedFloorData;
	if (!ParseHarnessFloorDataFromJson(JsonData, ParsedFloorData))
	{
		Clear();
		return false;
	}

	return BuildFromHarnessData(ParsedFloorData);
}

bool UInteReal2DFloorPlanCreator::BuildFromHarnessData(const FHarnessFloorData& InFloorData)
{
	SourceFloorData = InFloorData;
	Document = FInteReal2DFloorPlanConverter::ConvertFromHarness(SourceFloorData);
	return Document.bIsValid;
}

void UInteReal2DFloorPlanCreator::Clear()
{
	SourceFloorData = FHarnessFloorData();
	Document = FInteReal2DFloorPlanDocument();
}

bool UInteReal2DFloorPlanCreator::ParseHarnessFloorDataFromJson(const FString& JsonData, FHarnessFloorData& OutFloorData) const
{
	return FJsonObjectConverter::JsonObjectStringToUStruct<FHarnessFloorData>(JsonData, &OutFloorData, 0, 0);
}