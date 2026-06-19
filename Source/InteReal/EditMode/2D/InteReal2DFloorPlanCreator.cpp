#include "InteReal2DFloorPlanCreator.h"

#include "InteReal/Harness/Public/HarnessJsonParser.h"
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
	FString Error;
	const bool bParsed = FHarnessJsonParser::ParseFloorDataFromJsonString(JsonData, OutFloorData, Error);
	if (!bParsed)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InteReal2D] Failed to parse harness floor data: %s"), *Error);
	}

	return bParsed;
}
