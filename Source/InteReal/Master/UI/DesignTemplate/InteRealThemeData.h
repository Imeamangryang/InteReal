// InteRealThemeData.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InteRealThemeData.generated.h"

UCLASS(BlueprintType)
class INTEREAL_API UInteRealThemeData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// --- 배경 및 주요 색상 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InteReal|Colors")
	FLinearColor Background_OffWhite = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("F8F5F1")));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InteReal|Colors")
	FLinearColor Main_Navy = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("0B2A42")));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InteReal|Colors")
	FLinearColor Accent_Gold = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("AF947F")));

	// --- 카드 및 패널 색상 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InteReal|Colors")
	FLinearColor Card_BG_White = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("FFFFFF")));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InteReal|Colors")
	FLinearColor Card_BG_Tint = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("FBF9F6")));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InteReal|Colors")
	FLinearColor Card_Border = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E6DDD3")));

	// --- 텍스트 및 구분선 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InteReal|Colors")
	FLinearColor Body_Text = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("1F2F3A")));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InteReal|Colors")
	FLinearColor Sub_Divider = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("CBB9A8")));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InteReal|Colors")
	FLinearColor Sub_Text = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("6E625A")));
};