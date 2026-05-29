#include "WeatherUISubsystem.h"

void UWeatherUISubsystem::Initialize(FSubsystemCollectionBase& Collection) {
	Super::Initialize(Collection);
	WeatherTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, TEXT("/Game/Data/Lighting_Data_Weather")));
	CityMainTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, TEXT("/Game/Data/Lighting_Data_CityMain")));
	CityDetailTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, TEXT("/Game/Data/Lighting_Data_CityDetail")));
	SolarTermTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, TEXT("/Game/Data/Lighting_Data_SolarTerm")));
}

void UWeatherUISubsystem::ApplyEnvironment(FName City, FName Weather, FName Solar, float Time) {
	auto* W = WeatherTable->FindRow<FWeatherData>(Weather, TEXT(""));
	auto* C = CityMainTable->FindRow<FCityMainData>(City, TEXT(""));
	auto* D = CityDetailTable->FindRow<FCityDetailData>(City, TEXT(""));
	auto* S = SolarTermTable->FindRow<FSolarTermData>(Solar, TEXT(""));

	if (W && C && D && S) OnEnvironmentUpdate.Broadcast(*W, *C, *D, *S, Time);
}