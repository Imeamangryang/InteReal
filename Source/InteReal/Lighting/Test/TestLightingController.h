#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TestLightingController.generated.h"


UCLASS()
class INTEREAL_API ATestLightingController : public AActor
{
    GENERATED_BODY()
    
public: 
    ATestLightingController();

protected:
    virtual void BeginPlay() override;

public: 
    // [에디터 연동] 제어 대상 환경 필수 액터들
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test PBL")
    class ADirectionalLight* TargetSun;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test PBL")
    class ASkyLight* TargetSky;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test PBL")
    class AExponentialHeightFog* TargetFog;

    // [상태 기억 변수] 
    float SavedHour = 12.0f;
    float SavedSeasonOffset = 0.0f; // 적위
    float SavedLatitude = 37.5665f;     // 기본값: 서울 위도
    float WeatherIntensityMax = 120000.0f; 
    FLinearColor WeatherSunColor = FLinearColor::White;
    float WeatherFogDensity = 0.01f;

    // UI 및 로그 출력용 스트링 변수
    FString SavedSeasonName = TEXT("춘분 (Spring)");
    FString SavedLocationName = TEXT("서울");
    FString SavedWeatherName = TEXT("맑음 (Clear)");

    // 마스터 연산 적용 시스템
    void UpdateEnvironment();

    // 외부 UI(UMG) 확장용 API
    UFUNCTION(BlueprintCallable, Category = "Test PBL")
    void SetTimeByStep(int32 Step);

    UFUNCTION(BlueprintCallable, Category = "Test PBL")
    void SetSeasonByStep(int32 SeasonStep);

    UFUNCTION(BlueprintCallable, Category = "Test PBL")
    void SetLocationByStep(int32 LocationStep);

    UFUNCTION(BlueprintCallable, Category = "Test PBL")
    void SetWeatherByStep(int32 WeatherStep);
    
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void SelectAndModify();

private:
    // 키보드 인풋 바인딩 래퍼
    void OnKey1() { SetTimeByStep(1); } void OnKey2() { SetTimeByStep(2); }
    void OnKey3() { SetTimeByStep(3); } void OnKey4() { SetTimeByStep(4); }
    void OnKey5() { SetTimeByStep(5); } void OnKey6() { SetTimeByStep(6); }
    void OnKey7() { SetTimeByStep(7); } void OnKey8() { SetTimeByStep(8); }

    void OnKeyQ() { SetSeasonByStep(1); } void OnKeyW() { SetSeasonByStep(2); }
    void OnKeyE() { SetSeasonByStep(3); } void OnKeyR() { SetSeasonByStep(4); }

    void OnKeyA() { SetLocationByStep(1); } void OnKeyS() { SetLocationByStep(2); }
    void OnKeyD() { SetLocationByStep(3); } void OnKeyF() { SetLocationByStep(4); }

    void OnKeyZ() { SetWeatherByStep(1); } void OnKeyX() { SetWeatherByStep(2); }
    void OnKeyC() { SetWeatherByStep(3); } void OnKeyB() { SetWeatherByStep(4); } // B: 눈 (Snowy)
    void OnKeyV() { SetWeatherByStep(5); } // V: 안개 (Foggy)
    
    void OnKeyO() { AdjustTargetRoughness(0.1f); }
    void OnKeyP() { AdjustTargetRoughness(-0.1f); }
    void AdjustTargetRoughness(float Delta);
    
    
    // Tick 활성화 및 시간 흐름 제어 변수
public:
    virtual void Tick(float DeltaTime) override;

    bool bIsTimeFlowing = false;
    float TimeAccumulator = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test PBL | Time Control")
    float TimeSpeed = 1.0f; // 초당 흐르는 시간량 (1.0 = 1시간/초)

private:
    void OnKey9() { bIsTimeFlowing = true; UE_LOG(LogTemp, Log, TEXT("[PRINTLOG_HJ] 시간 흐름 시작")); }
    void OnKey0() { bIsTimeFlowing = false; UE_LOG(LogTemp, Log, TEXT("[PRINTLOG_HJ] 시간 흐름 정지")); }
    
};