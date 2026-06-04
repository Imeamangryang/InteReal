#include "TestLightingController.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Kismet/GameplayStatics.h"
#include "Components/InputComponent.h"

#include "MaterialManager.h"


ATestLightingController::ATestLightingController()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ATestLightingController::BeginPlay()
{
    Super::BeginPlay();
    
    SetActorTickEnabled(true);
    PrimaryActorTick.bCanEverTick = true;
    
    UE_LOG(LogTemp, Log, TEXT("[PRINTLOG_HJ] ======================================================================"));
    UE_LOG(LogTemp, Log, TEXT("[PRINTLOG_HJ] 🎛️ 피그마 통합 제어 패널 기능 검증 매니저 세팅 완료"));
    UE_LOG(LogTemp, Log, TEXT("[PRINTLOG_HJ] [단축키] 시간: 1~8 | 절기: QWER | 위치: ASDF | 날씨: ZXCBV"));
    UE_LOG(LogTemp, Log, TEXT("[PRINTLOG_HJ] ======================================================================"));

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC)
    {
        PC->bShowMouseCursor = true; 
        PC->bEnableClickEvents = true; 
        PC->bEnableMouseOverEvents = true; // 선택 사항: 마우스 오버 시 강조 효과를 넣고 싶다면
    }
    
    if (PC && PC->InputComponent)
    {
        EnableInput(PC);

        PC->InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ATestLightingController::SelectAndModify);
        
        // 1~8번 키 (시간: 00:00부터 3시간 단위)
        PC->InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ATestLightingController::OnKey1);
        PC->InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ATestLightingController::OnKey2);
        PC->InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ATestLightingController::OnKey3);
        PC->InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ATestLightingController::OnKey4);
        PC->InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &ATestLightingController::OnKey5);
        PC->InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &ATestLightingController::OnKey6);
        PC->InputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &ATestLightingController::OnKey7);
        PC->InputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &ATestLightingController::OnKey8);

        // QWER 키 (계절 및 4대 절기)
        PC->InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ATestLightingController::OnKeyQ);
        PC->InputComponent->BindKey(EKeys::W, IE_Pressed, this, &ATestLightingController::OnKeyW);
        PC->InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ATestLightingController::OnKeyE);
        PC->InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ATestLightingController::OnKeyR);

        // ASDF 키 (위치 프리셋: 서울, 대전, 부산, 제주)
        PC->InputComponent->BindKey(EKeys::A, IE_Pressed, this, &ATestLightingController::OnKeyA);
        PC->InputComponent->BindKey(EKeys::S, IE_Pressed, this, &ATestLightingController::OnKeyS);
        PC->InputComponent->BindKey(EKeys::D, IE_Pressed, this, &ATestLightingController::OnKeyD);
        PC->InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ATestLightingController::OnKeyF);
        
        // ZXCBV 키 (날씨 프리셋 5종 확장)
        PC->InputComponent->BindKey(EKeys::Z, IE_Pressed, this, &ATestLightingController::OnKeyZ);
        PC->InputComponent->BindKey(EKeys::X, IE_Pressed, this, &ATestLightingController::OnKeyX);
        PC->InputComponent->BindKey(EKeys::C, IE_Pressed, this, &ATestLightingController::OnKeyC);
        PC->InputComponent->BindKey(EKeys::B, IE_Pressed, this, &ATestLightingController::OnKeyB); 
        PC->InputComponent->BindKey(EKeys::V, IE_Pressed, this, &ATestLightingController::OnKeyV);

        
        PC->InputComponent->BindKey(EKeys::O, IE_Pressed, this, &ATestLightingController::OnKeyO);
        PC->InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ATestLightingController::OnKeyP);
        PC->InputComponent->BindKey(EKeys::I, IE_Pressed, this, &ATestLightingController::OnKeyI);
        PC->InputComponent->BindKey(EKeys::U, IE_Pressed, this, &ATestLightingController::OnKeyU);
        PC->InputComponent->BindKey(EKeys::Y, IE_Pressed, this, &ATestLightingController::OnKeyY);
        PC->InputComponent->BindKey(EKeys::T, IE_Pressed, this, &ATestLightingController::OnKeyT);
        
        
        // 시간 자동 흐름 온오프 키
        PC->InputComponent->BindKey(EKeys::Nine, IE_Pressed, this, &ATestLightingController::OnKey9);
        PC->InputComponent->BindKey(EKeys::Zero, IE_Pressed, this, &ATestLightingController::OnKey0);
        
        UpdateEnvironment();
    }
}

void ATestLightingController::UpdateEnvironment()
{
    if (!TargetSun) return;

    // 1. [천문학적 태양 위치 계산] (구면 삼각법 적용)
    // 12시(정오) 기준 시간각 계산 (-180도 ~ 180도)
    float HourAngle = (static_cast<float>(SavedHour) - 12.0f) * 15.0f;
    float RadLat = FMath::DegreesToRadians(SavedLatitude);
    float RadDec = FMath::DegreesToRadians(SavedSeasonOffset); // 하지 +23.5, 동지 -23.5
    float RadHA = FMath::DegreesToRadians(HourAngle);

    // 고도(Altitude) 산출 공식: sin(Alt) = sin(Lat)*sin(Dec) + cos(Lat)*cos(Dec)*cos(HA)
    float SinAlt = FMath::Sin(RadLat) * FMath::Sin(RadDec) + FMath::Cos(RadLat) * FMath::Cos(RadDec) * FMath::Cos(RadHA);
    float Altitude = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(SinAlt, -1.0f, 1.0f)));

    // 방위각(Azimuth) 계산
    float CosAz = (FMath::Sin(RadDec) - FMath::Sin(RadLat) * SinAlt) / (FMath::Cos(RadLat) * FMath::Cos(FMath::Asin(FMath::Clamp(SinAlt, -1.0f, 1.0f))));
    float Azimuth = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosAz, -1.0f, 1.0f)));
    if (HourAngle > 0) Azimuth = 360.0f - Azimuth;

    // 2. [태양 회전 적용]
    FRotator NewRotation;
    NewRotation.Pitch = -Altitude; // 지평선 기준 위쪽이 -값
    NewRotation.Yaw = Azimuth + 180.0f; // 정남향 기준
    TargetSun->SetActorRotation(NewRotation);

    // 3. [물리 기반 라이팅 제어 - 보간 적용]
    if (UDirectionalLightComponent* SunComp = Cast<UDirectionalLightComponent>(TargetSun->GetComponentByClass(UDirectionalLightComponent::StaticClass())))
    {
        float TargetIntensity = (Altitude > 0) ? WeatherIntensityMax : (WeatherIntensityMax * 0.02f);
        FLinearColor TargetColor = (Altitude > 0) ? WeatherSunColor : FLinearColor(0.2f, 0.25f, 0.4f, 1.0f);

        // 보간(FInterp) 적용: 부드럽게 밝기가 변함
        float CurrentIntensity = SunComp->Intensity;
        float InterpIntensity = FMath::FInterpTo(CurrentIntensity, TargetIntensity, GetWorld()->GetDeltaSeconds(), 2.0f);
        
        SunComp->SetIntensity(InterpIntensity);
        SunComp->SetLightColor(FMath::Lerp(SunComp->GetLightColor(), TargetColor, 0.1f));
    }
    
    
    // 4. [안개 및 스카이라이트]
    if (TargetFog)
    {
        if (UExponentialHeightFogComponent* FogComp = Cast<UExponentialHeightFogComponent>(TargetFog->GetComponentByClass(UExponentialHeightFogComponent::StaticClass())))
        {
            FogComp->SetFogDensity(WeatherFogDensity);
        }
    }
    if (TargetSky && FMath::FRand() < 0.05f) TargetSky->GetLightComponent()->RecaptureSky();

    // 4. [로그 및 화면 출력 제어] (시간이 바뀔 때만 출력)
    static int32 LastLoggedHour = -1;
    static FString LastLoggedConfig = TEXT(""); // 마지막 설정 상태 저장용
    
    int32 CurrentHour = static_cast<int32>(SavedHour);
    FString CurrentConfig = FString::Printf(TEXT("%s_%s_%s"), *SavedLocationName, *SavedSeasonName, *SavedWeatherName);
    
    // 시간이 바뀌었거나, 설정(위치/절기/날씨)이 바뀌었을 때
    if (CurrentHour != LastLoggedHour || CurrentConfig != LastLoggedConfig)
    {
        LastLoggedHour = CurrentHour;
        LastLoggedConfig = CurrentConfig;
        
        UE_LOG(LogTemp, Log, TEXT("[PRINTLOG_HJ] 갱신 ➡️ %02d:00 | 📍 %s | 🗓️ %s | 🌧️ %s"), 
            CurrentHour, *SavedLocationName, *SavedSeasonName, *SavedWeatherName);

        if (GEngine)
        {
            FString ScreenMsg = FString::Printf(TEXT("[%s] %02d:00 | %s | %s"), 
                *SavedLocationName, CurrentHour, *SavedSeasonName, *SavedWeatherName);
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, ScreenMsg);
        }
    }
}

// 1번(00:00 자정) 시작 -> 3시간 간격 정밀 매핑 완료
void ATestLightingController::SetTimeByStep(int32 Step)
{
    SavedHour = static_cast<float>((Step - 1) * 3);
    UE_LOG(LogTemp, Log, TEXT("[PRINTLOG_HJ] 입력 감지 ➡️ 숫자 %d번 키 수신 (목표 시간 적용 -> [%02d:00])"), Step, static_cast<int32>(SavedHour));
    UpdateEnvironment();
}

// QWER 절기 매핑
void ATestLightingController::SetSeasonByStep(int32 SeasonStep)
{
    switch (SeasonStep)
    {
    case 1: SavedSeasonOffset = 0.0f;   SavedSeasonName = TEXT("춘분 (Spring)"); break;
    case 2: SavedSeasonOffset = 23.5f;  SavedSeasonName = TEXT("하지 (Summer)"); break;
    case 3: SavedSeasonOffset = 0.0f;   SavedSeasonName = TEXT("추분 (Autumn)"); break;
    case 4: SavedSeasonOffset = -23.5f; SavedSeasonName = TEXT("동지 (Winter)"); break;
    }
    UE_LOG(LogTemp, Log, TEXT("[PRINTLOG_HJ] 입력 감지 ➡️ 절기 변경 적용 ([%s])"), *SavedSeasonName);
    UpdateEnvironment();
}

// ASDF 국내 주요 거점 위도 실제 데이터 매핑
void ATestLightingController::SetLocationByStep(int32 LocationStep)
{
    switch (LocationStep)
    {
    case 1: SavedLatitude = 37.5665f; SavedLocationName = TEXT("서울 (Seoul)"); break;
    case 2: SavedLatitude = 36.3504f; SavedLocationName = TEXT("대전 (Daejeon)"); break; // 대전 실제 위도 반영
    case 3: SavedLatitude = 35.1796f; SavedLocationName = TEXT("부산 (Busan)"); break;   // 부산 실제 위도 반영
    case 4: SavedLatitude = 33.4996f; SavedLocationName = TEXT("제주 (Jeju)"); break;    // 제주 실제 위도 반영
    }
    UE_LOG(LogTemp, Log, TEXT("[PRINTLOG_HJ] 입력 감지 ➡️ 지역 변경 적용 ([%s])"), *SavedLocationName);
    UpdateEnvironment();
}

// ZXCBV 5대 환경 날씨 스펙 하드코딩 적용
void ATestLightingController::SetWeatherByStep(int32 WeatherStep)
{
    switch (WeatherStep)
    {
    case 1: // Z: 맑음
        WeatherIntensityMax = 120000.0f;
        WeatherSunColor = FLinearColor(1.0f, 0.95f, 0.9f, 1.0f);
        WeatherFogDensity = 0.005f;
        SavedWeatherName = TEXT("맑음 (Clear)");
        break;
    case 2: // X: 흐림
        WeatherIntensityMax = 25000.0f;
        WeatherSunColor = FLinearColor(0.65f, 0.68f, 0.72f, 1.0f);
        WeatherFogDensity = 0.02f;
        SavedWeatherName = TEXT("흐림 (Cloudy)");
        break;
    case 3: // C: 비
        WeatherIntensityMax = 120000.0f * 0.1f; // 극심한 조도 저하
        WeatherSunColor = FLinearColor(0.4f, 0.45f, 0.5f, 1.0f);
        WeatherFogDensity = 0.04f;
        SavedWeatherName = TEXT("비 (Rainy)");
        break;
    case 4: // B: 눈
        WeatherIntensityMax = 40000.0f; // 눈 구름 반사광 감안한 조도
        WeatherSunColor = FLinearColor(0.9f, 0.92f, 0.98f, 1.0f); // 약간 푸르스름하고 밝은 백색광 조율
        WeatherFogDensity = 0.035f; // 강설로 인한 흐림 연출
        SavedWeatherName = TEXT("눈 (Snowy)");
        break;
    case 5: // V: 안개
        WeatherIntensityMax = 5000.0f;
        WeatherSunColor = FLinearColor(0.75f, 0.72f, 0.68f, 1.0f);
        WeatherFogDensity = 0.15f; // 완벽한 시야 차폐 농도
        SavedWeatherName = TEXT("짙은 안개 (Foggy)");
        break;
    }
    UE_LOG(LogTemp, Log, TEXT("[PRINTLOG_HJ] 입력 감지 ➡️ 기후 날씨 변경 적용 ([%s])"), *SavedWeatherName);
    UpdateEnvironment();
}

void ATestLightingController::SelectAndModify()
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    FHitResult Hit;

    // 마우스 커서 위치에서 오브젝트 탐색
    if (PC->GetHitResultUnderCursor(ECC_Visibility, false, Hit))
    {
        UStaticMeshComponent* HitMesh = Cast<UStaticMeshComponent>(Hit.GetComponent());
        
        // 씬에 배치된 MaterialManager 액터를 찾아옴
        AMaterialManager* MyMatManager = Cast<AMaterialManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AMaterialManager::StaticClass()));

        if (MyMatManager && HitMesh)
        {
            MyMatManager->SetTargetMesh(HitMesh);
            UE_LOG(LogTemp, Log, TEXT("성공: %s를 MaterialManager에 등록함"), *Hit.GetActor()->GetName());
        }
    }
}

void ATestLightingController::AdjustTargetRoughness(float Delta)
{
    AMaterialManager* MM = Cast<AMaterialManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AMaterialManager::StaticClass()));
    if (MM) MM->AdjustRoughness(Delta);
}

void ATestLightingController::AdjustTargetMetallic(float Delta)
{
    AMaterialManager* MM = Cast<AMaterialManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AMaterialManager::StaticClass()));
    if (MM)  MM->AdjustMetallic(MM->Metallic + Delta);
}

void ATestLightingController::AdjustTargetSpecular(float Delta)
{
    AMaterialManager* MM = Cast<AMaterialManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AMaterialManager::StaticClass()));
    if (MM) MM->AdjustSpecular(MM->Specular + Delta);
}

void ATestLightingController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    if (bIsTimeFlowing)
    {
        // 1. 시간 누적 (TimeSpeed가 1.0이면 1초에 1시간 이동)
        SavedHour += DeltaTime * TimeSpeed;
        
        // 24시간 순환
        if (SavedHour >= 24.0f) SavedHour -= 24.0f;
        
        // 2. 매 프레임 부드럽게 업데이트
        UpdateEnvironment();
    }
}
