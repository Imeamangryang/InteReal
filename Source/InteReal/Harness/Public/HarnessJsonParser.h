#pragma once

#include "CoreMinimal.h"
#include "HarnessData.h"

class INTEREAL_API FHarnessJsonParser
{
public:
    // 파일 경로로부터 토폴로지 JSON을 읽어 구조체에 바인딩
    static bool LoadFloorDataFromJsonFile(const FString& FilePath, FHarnessFloorData& OutData, FString& OutError);
    
    // JSON 문자열을 직접 토폴로지 구조체로 파싱
    static bool ParseFloorDataFromJsonString(const FString& JsonString, FHarnessFloorData& OutData, FString& OutError);
};