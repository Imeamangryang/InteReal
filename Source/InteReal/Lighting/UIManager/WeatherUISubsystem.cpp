#include "WeatherUISubsystem.h"

void UWeatherUISubsystem::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);
    WeatherTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, TEXT("/Game/Data/Lighting_Data_Weather")));
    CityMainTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, TEXT("/Game/Data/Lighting_Data_CityMain")));
    CityDetailTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, TEXT("/Game/Data/Lighting_Data_CityDetail")));
    SolarTermTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, TEXT("/Game/Data/Lighting_Data_SolarTerm")));
}

FName UWeatherUISubsystem::GetSolarRowName(FString NameKR) {
    for (auto& RowName : SolarTermTable->GetRowNames()) {
       auto* Data = SolarTermTable->FindRow<FSolarTermData>(RowName, TEXT(""));
       if (Data && Data->Name_KR == NameKR) return RowName;
    }
    return FName(TEXT(""));
}

TArray<FString> UWeatherUISubsystem::GetCityDetails(FName ParentCityName) {
	TArray<FString> Results;
	auto* MainData = CityMainTable->FindRow<FCityMainData>(ParentCityName, TEXT(""));
	if (!MainData) return Results;

	for (auto RowName : CityDetailTable->GetRowNames()) {
		auto* Data = CityDetailTable->FindRow<FCityDetailData>(RowName, TEXT(""));
		// 이제 RowName.ToString() 대신 Name_KR을 담습니다.
		if (Data && Data->Parent_CityID == MainData->CityID) {
			Results.Add(Data->Name_KR); 
		}
	}
	return Results;
}

TArray<FString> UWeatherUISubsystem::GetSolarTermsBySeason(FString Season) {
    TArray<FString> Results;
    for (auto RowName : SolarTermTable->GetRowNames()) {
       auto* Data = SolarTermTable->FindRow<FSolarTermData>(RowName, TEXT(""));
       if (Data && Data->Season == Season) Results.Add(Data->Name_KR);
    }
    return Results;
}

void UWeatherUISubsystem::SetCityDetail(FName ID) { CurrentCityDetailID = ID; BroadcastEnvironment(); }
void UWeatherUISubsystem::SetWeather(FName ID)    { CurrentWeatherID = ID;    BroadcastEnvironment(); }
void UWeatherUISubsystem::SetSolar(FName ID)      { CurrentSolarID = ID;      BroadcastEnvironment(); }
void UWeatherUISubsystem::SetTime(float Time)     { CurrentTime = Time;       BroadcastEnvironment(); }
void UWeatherUISubsystem::SetOrientation(float Offset) { CurrentOrientation = Offset; BroadcastEnvironment(); }

void UWeatherUISubsystem::ForceUpdate() { BroadcastEnvironment(); }

void UWeatherUISubsystem::BroadcastEnvironment() {
	
	// 1. 테이블 유효성 검사 추가 (안전성 확보)
	if (!CityDetailTable || !WeatherTable || !SolarTermTable || !CityMainTable) return;
	
    auto* D = CityDetailTable->FindRow<FCityDetailData>(CurrentCityDetailID, TEXT(""));
    auto* W = WeatherTable->FindRow<FWeatherData>(CurrentWeatherID, TEXT(""));
    auto* S = SolarTermTable->FindRow<FSolarTermData>(CurrentSolarID, TEXT(""));

    FCityMainData* C = nullptr;
    if (D) {
       for (auto RowName : CityMainTable->GetRowNames()) {
          auto* Temp = CityMainTable->FindRow<FCityMainData>(RowName, TEXT(""));
          if (Temp && Temp->CityID == D->Parent_CityID) { C = Temp; break; }
       }
    }

	// 2. 모든 데이터가 찾았을 때만 브로드캐스트
	if (W && C && D && S) {
		OnEnvironmentUpdate.Broadcast(*W, *C, *D, *S, CurrentTime, CurrentOrientation);
	} 
	else {
		// 데이터가 없으면 오류를 출력하여 어떤 RowName이 문제인지 바로 확인 가능하게 함
		UE_LOG(LogTemp, Warning, TEXT("Environment Data Load Failed!"));
	}
}
