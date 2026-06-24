#include "EnvController.h"

#include "InteReal/Lighting/UIManager/WeatherUISubsystem.h"
#include "InteReal/Struct/LightingDataStruct.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "Components/MeshComponent.h"

AEnvController::AEnvController()
{
    PrimaryActorTick.bCanEverTick = true;
    
    // Niagara 컴포넌트 생성 및 부착
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    WeatherNiagara = CreateDefaultSubobject<UNiagaraComponent>(TEXT("WeatherNiagara"));
    WeatherNiagara->SetupAttachment(RootComponent);
    
}

void AEnvController::BeginPlay()
{
    Super::BeginPlay();
    
    // 배치된 스태틱 메쉬 액터들에 라이트 컴포넌트 추가
    for (AActor* Actor : LightningSplineActors)
    {
        if (Actor)
        {
            UPointLightComponent* NewLight = NewObject<UPointLightComponent>(Actor);
            NewLight->RegisterComponent();
            NewLight->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
            NewLight->SetVisibility(false);
            NewLight->SetAttenuationRadius(12000.0f);
            NewLight->SetSourceRadius(1000.0f);
            NewLight->SetIntensity(100000.0f);
            NewLight->SetLightColor(FLinearColor(0.8f, 0.9f, 1.0f));
        }
    }
    
    // 초기 타겟 값 설정
    TargetSunIntensity = 10000.0f; // 기본 태양 밝기 값 설정
    TargetSkyIntensity = 1.0f;     // 기본 스카이 밝기 값 설정
    
    // 1. 컴포넌트를 부모 액터에서 완벽히 분리
    if (WeatherNiagara)
    {
        WeatherNiagara->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        
        // 이펙트 좌표를 월드 중앙으로 고정
        WeatherNiagara->SetWorldLocation(FVector(0.0f, 0.0f, 2000.0f)); 
        
        // 컴포넌트가 부모의 스케일이나 회전에 영향을 받지 않게 설정
        WeatherNiagara->SetAbsolute(true, true, true);
    }
    
    // 서브시스템 가져오기
    UWeatherUISubsystem* Sub = GetGameInstance()->GetSubsystem<UWeatherUISubsystem>();
    if (Sub)
    {
        Sub->OnEnvironmentUpdate.AddDynamic(this, &AEnvController::UpdateEnvironment);
        // 강제로 업데이트 실행
        Sub->ForceUpdate();
    }
    
    // 보간을 시작하기 전에 현재 계산된 타겟 값으로 즉시 조명 설정
    if (SunLight) SunLight->SetIntensity(TargetSunIntensity);
    if (IsValid(SkyLight) && SkyLight->GetLightComponent()) 
        SkyLight->GetLightComponent()->SetIntensity(TargetSkyIntensity);
    
    bIsInitialized = true;
    
    // 1초마다 건물 영역을 확인하여 나이아가라 파라미터를 갱신합니다.
    GetWorldTimerManager().SetTimer(BuildingScanTimer, this, &AEnvController::UpdateBuildingMask, 1.0f, true);
    
    if (SunLight)
    {
        // 1. 반사광 배율을 0으로 설정 (Specular Scale)
        SunLight->SetSpecularScale(0.0f);

        // 2. 그림자 얼룩(Shadow Acne) 방지를 위해 기울기 편향을 기본값 수준으로 복구
        SunLight->SetShadowSlopeBias(0.5f);

        // 3. 노이즈(Artifact) 유발로 인해 컨택트 섀도 비활성화 유지
        SunLight->ContactShadowLength = 0.0f;

        // 4. 그림자 시작 지점을 물체에 살짝 당기되 얼룩이 지지 않도록 0.3 설정 (기본값 0.5)
        SunLight->SetShadowBias(0.3f);
    }
}

void AEnvController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    if (!bIsInitialized) return;
    
    // 타겟 밝기로 부드럽게 보간
    if (SunLight)
    {
        SunLight->SetIntensity(FMath::FInterpTo(SunLight->Intensity, TargetSunIntensity, DeltaTime, InterpSpeed));
    }

    if (IsValid(SkyLight) && SkyLight->GetLightComponent())
    {
        SkyLight->GetLightComponent()->SetIntensity(FMath::FInterpTo(SkyLight->GetLightComponent()->Intensity, TargetSkyIntensity, DeltaTime, InterpSpeed));
    }
}

void AEnvController::UpdateEnvironment(FWeatherData W, FCityMainData C, FCityDetailData D, FSolarTermData S, float Time, float Orientation)
{
    if (!SunLight) return;

    // 날씨 ID 확인 및 Niagara/번개 제어
    UWeatherUISubsystem* Sub = GetGameInstance()->GetSubsystem<UWeatherUISubsystem>();
    FName CurrentWeatherID = (Sub) ? Sub->GetCurrentWeatherID() : NAME_None;
    
    if (LastWeatherID != CurrentWeatherID) {
        HandleWeatherChange(CurrentWeatherID);
        LastWeatherID = CurrentWeatherID;
    }
    
    // 계산 로직
    float HourAngle = (Time - 12.0f) * 15.0f;
    float RadLat = FMath::DegreesToRadians(C.Latitude);
    float RadDec = FMath::DegreesToRadians(S.Declination);
    float RadHA = FMath::DegreesToRadians(HourAngle);

    float SinAlt = FMath::Sin(RadLat) * FMath::Sin(RadDec) + FMath::Cos(RadLat) * FMath::Cos(RadDec) * FMath::Cos(RadHA);
    float Altitude = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(SinAlt, -1.0f, 1.0f)));
    float Azimuth = FMath::RadiansToDegrees(
        FMath::Atan2(
            FMath::Sin(RadHA),
            FMath::Cos(RadDec) * FMath::Sin(RadLat) - FMath::Sin(RadDec) * FMath::Cos(RadLat) * FMath::Cos(RadHA)
        )
    );
    
    // 즉시 적용 항목
    SunLight->SetRelativeRotation(FRotator(-Altitude, Azimuth + Orientation + 180.0f, 0.0f));
    SunLight->SetLightColor(FLinearColor::MakeFromColorTemperature(W.Temperature));
    
    // 목표 밝기 계산 
    float AltitudeMultiplier = (Altitude > 0) ? 1.0f : 0.05f;
    // Clear가 아니면 밝기를 20% 수준으로 낮
    float WeatherContrast = (CurrentWeatherID == FName("Clear")) ? 1.0f : 0.2f;
    
    if (IsValid(SkyLight) && SkyLight->GetLightComponent()) SkyLight->GetLightComponent()->RecaptureSky();
        
    TargetSunIntensity = W.IntensityLux * AltitudeMultiplier * WeatherContrast * MasterIntensityMultiplier; 
    
    // 최소 밝기 설정 
    float MinNightIntensity = 0.05f; 
    TargetSkyIntensity = FMath::Max(W.SkyIntensity * AltitudeMultiplier, MinNightIntensity);

    // 태양이 졌을 때 차가운 푸른빛 틴트 적용
    if (Altitude < 0) 
    {
        // 밤에는 하늘 조명을 약간 푸른 남색 톤으로 고정
        FLinearColor NightTint = FLinearColor(0.1f, 0.15f, 0.3f);
        SkyLight->GetLightComponent()->SetLightColor(NightTint);
    } 
    else 
    {
        SkyLight->GetLightComponent()->SetLightColor(FLinearColor::White);
    }
    
    if (IsValid(Fog))
    {
        Fog->SetFogDensity(W.FogDensity);
        FLinearColor FogColor = (Altitude < 0) ? FLinearColor(0.02f, 0.02f, 0.05f) : FLinearColor::LerpUsingHSV(FLinearColor::White, FLinearColor::Gray, W.SkyIntensity);
        Fog->SetFogInscatteringColor(FogColor);
    }
}

void AEnvController::HandleWeatherChange(FName WeatherID)
{
    // 컴포넌트 유효성 확인
    if (!WeatherNiagara) 
    {
        UE_LOG(LogTemp, Error, TEXT("WeatherNiagara Component is NULL!"));
        return;
    }

    // 맵에 키가 있는지 확인
    if (WeatherEffectsMap.Contains(WeatherID) && WeatherEffectsMap[WeatherID] != nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("Attempting to activate effect: %s"), *WeatherID.ToString());
        
        WeatherNiagara->SetAsset(WeatherEffectsMap[WeatherID]);
        
        WeatherNiagara->SetVariableFloat(FName("SpawnRadius"), AvoidanceRadius);
        
        // 확실한 활성화 
        WeatherNiagara->ResetSystem(); // 시스템 내부 상태 완전히 초기화
        WeatherNiagara->Activate(true);
        WeatherNiagara->SetVisibility(true); // 혹시 숨겨져 있는지 확인
        // [디버그 로그] 컴포넌트 상태 확인
        bool bIsActive = WeatherNiagara->IsActive();
        UE_LOG(LogTemp, Warning, TEXT("Effect activated. IsActive: %s"), bIsActive ? TEXT("True") : TEXT("False"));
    }
    else 
    {
        UE_LOG(LogTemp, Warning, TEXT("No effect found for WeatherID: %s. Deactivating."), *WeatherID.ToString());
        WeatherNiagara->Deactivate();
    }

    // 번개 타이머 제어
    if (WeatherID == FName("Stormy")) {
        GetWorldTimerManager().SetTimer(LightningTimerHandle, this, &AEnvController::TriggerRandomLightning, FMath::RandRange(2.0f, 5.0f), true);
    } else {
        GetWorldTimerManager().ClearTimer(LightningTimerHandle);
    }
}
void AEnvController::TriggerRandomLightning()
{
    if (LightningSplineActors.Num() == 0) return;

    // 랜덤하게 2~4개 인덱스 선택
    int32 NumToStrike = FMath::RandRange(2, 4);
    TArray<int32> Indices;
    while(Indices.Num() < NumToStrike)
    {
        int32 RandIdx = FMath::RandRange(0, LightningSplineActors.Num() - 1);
        if(!Indices.Contains(RandIdx)) Indices.Add(RandIdx);
    }

    // 선택된 액터들 활성화
    for(int32 Index : Indices)
    {
        AActor* Selected = LightningSplineActors[Index];
        
        // 메시 보이기
        Selected->SetActorHiddenInGame(false);
        
        // 해당 액터 내부의 PointLight 찾아서 켜기
        UPointLightComponent* PL = Selected->FindComponentByClass<UPointLightComponent>();
        if (PL) 
        {
            // 라이트 위치를 지면이 아닌 하늘 높이로 배치
            FVector LightningSkyPos = Selected->GetActorLocation();
            LightningSkyPos.Z += 3000.0f; 
            
            PL->SetWorldLocation(LightningSkyPos);
            
            // [광원 강화 설정]
            PL->SetAttenuationRadius(12000.0f); // 영향 범위 확대
            PL->SetSourceRadius(1000.0f);       // 광원을 구체처럼 만들어 빛이 부드럽게 퍼짐
            PL->SetIntensity(100000.0f);        // 광량 대폭 상향
            PL->SetLightColor(FLinearColor(0.8f, 0.9f, 1.0f)); // 푸른빛이 섞인 흰색
            PL->SetVisibility(true);
            
            PL->SetSpecularScale(0.0f);
            PL->SetShadowSlopeBias(0.0f);
        }

        // 0.1 ~ 0.2초 사이의 랜덤한 시간으로 설정
        float RandomDuration = FMath::RandRange(0.1f, 0.2f);
        
        FTimerHandle ResetHandle;
        GetWorldTimerManager().SetTimer(ResetHandle, [Selected, PL]() {
            Selected->SetActorHiddenInGame(true);
            if(PL) PL->SetVisibility(false);
        }, RandomDuration, false);
    }
    // 전체 하늘 번쩍임
    if (LightningLight) 
    {
        LightningLight->SetWorldLocation(FVector(0, 0, 5000.0f));
        LightningLight->SetAttenuationRadius(30000.0f); // 맵 전체를 덮는 범위
        LightningLight->SetSourceRadius(5000.0f);       // 전체 하늘이 번쩍이도록 설정
        LightningLight->SetIntensity(250000.0f);        // 아주 강한 밝기
        LightningLight->SetVisibility(true);
    
        FTimerHandle GlobalResetHandle;
        GetWorldTimerManager().SetTimer(GlobalResetHandle, [this]() {
            LightningLight->SetVisibility(false);
        }, 0.05f, false);
    }
}
// [건물 스캔 로직 구현]
void AEnvController::UpdateBuildingMask()
{
    if (!WeatherNiagara) return;

    FBox TotalBuildingBounds(ForceInit);
    bool bFoundBuilding = false;

    // 월드에 있는 모든 액터를 순회하며 "Floor" 태그를 가진 메쉬 검색
    for (TActorIterator<AActor> ActorItr(GetWorld()); ActorItr; ++ActorItr)
    {
        TArray<UActorComponent*> FloorComponents = ActorItr->GetComponentsByTag(UMeshComponent::StaticClass(), FName("Floor"));
        
        for (UActorComponent* Comp : FloorComponents)
        {
            UMeshComponent* MeshComp = Cast<UMeshComponent>(Comp);
            if (MeshComp)
            {
                TotalBuildingBounds += MeshComp->Bounds.GetBox();
                bFoundBuilding = true;
            }
        }
    }

    // 건물을 찾았다면 나이아가라 시스템에 Center와 Extent 전달
    if (bFoundBuilding && TotalBuildingBounds.IsValid)
    {
        FVector Center = TotalBuildingBounds.GetCenter();
        FVector Extent = TotalBuildingBounds.GetExtent();

        // 파티클이 건물 내부 위/아래로 들어오는 것을 완벽히 막기 위해 Z축을 아주 크게 설정
        Center.Z = 0.0f;
        Extent.Z = 5000.0f; 

        WeatherNiagara->SetVariableVec3(FName("BuildingCenter"), Center);
        WeatherNiagara->SetVariableVec3(FName("BuildingExtent"), Extent);
    }
}