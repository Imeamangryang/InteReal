#include "Public/HarnessMinimapCaptureComponent.h"
#include "Public/HarnessPipelineManager.h"
#include "Engine/Scene.h"
#include "EngineUtils.h"

UHarnessMinimapCaptureComponent::UHarnessMinimapCaptureComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// 미니맵용 캡처 디폴트 세팅
	ApplyStableCaptureSettings();
    
	// 수직 아래를 바라보도록 기본 회전값 고정
	SetRelativeRotation(FRotator(-90.0f, -90.0f, 0.0f));
}

void UHarnessMinimapCaptureComponent::ApplyStableCaptureSettings()
{
	SetRelativeRotation(FRotator(-90.0f, -90.0f, 0.0f));

	ProjectionType = ECameraProjectionMode::Orthographic;
	CaptureSource = SCS_FinalColorLDR;
	UnlitViewmode = ESceneCaptureUnlitViewmode::Capture;

	bCaptureEveryFrame = false;
	bCaptureOnMovement = false;
	PostProcessSettings = FPostProcessSettings();
	PostProcessBlendWeight = 0.0f;

	ShowFlags.DisableFeaturesForUnlit(false);
	ShowFlags.DisableAdvancedFeatures();
	ShowFlags.SetAntiAliasing(false);
	ShowFlags.SetAtmosphere(false);
	ShowFlags.SetFog(false);
	ShowFlags.SetVolumetricFog(false);
	ShowFlags.SetDynamicShadows(false);
	ShowFlags.SetSkyLighting(false);
	ShowFlags.SetAmbientOcclusion(false);
	ShowFlags.SetScreenSpaceReflections(false);
	ShowFlags.SetBloom(false);
	ShowFlags.SetLocalExposure(false);
	ShowFlags.SetEyeAdaptation(false);
	ShowFlags.SetColorGrading(false);
	ShowFlags.SetTonemapper(false);
	ShowFlags.SetUnlitViewmode(true);

	PostProcessSettings.bOverride_AutoExposureMethod = true;
	PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
	PostProcessSettings.AutoExposureMinBrightness = 1.0f;
	PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
	PostProcessSettings.AutoExposureMaxBrightness = 1.0f;
	PostProcessSettings.bOverride_AutoExposureBias = true;
	PostProcessSettings.AutoExposureBias = 0.0f;
	PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	PostProcessSettings.bOverride_BloomIntensity = true;
	PostProcessSettings.BloomIntensity = 0.0f;
	PostProcessSettings.bOverride_SceneFringeIntensity = true;
	PostProcessSettings.SceneFringeIntensity = 0.0f;
	PostProcessSettings.bOverride_VignetteIntensity = true;
	PostProcessSettings.VignetteIntensity = 0.0f;
	PostProcessSettings.bOverride_MotionBlurAmount = true;
	PostProcessSettings.MotionBlurAmount = 0.0f;
}

UTextureRenderTarget2D* UHarnessMinimapCaptureComponent::GetOrCreateRenderTarget(int32 Resolution)
{
	ApplyStableCaptureSettings();

	if (!MinimapRenderTarget)
	{
		MinimapRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("DynamicMinimapRT"));
		check(MinimapRenderTarget);
		MinimapRenderTarget->ClearColor = FLinearColor::Black;
		MinimapRenderTarget->TargetGamma = 2.2f;
		MinimapRenderTarget->InitCustomFormat(Resolution, Resolution, PF_B8G8R8A8, false);
		MinimapRenderTarget->UpdateResourceImmediate(true);

		this->TextureTarget = MinimapRenderTarget;
	}
	return MinimapRenderTarget;
}

void UHarnessMinimapCaptureComponent::AdjustToBoundingBox(FVector2D MinBounds, FVector2D MaxBounds, float Padding)
{
	CachedMin = MinBounds;
	CachedMax = MaxBounds;

	float CenterX = (MinBounds.X + MaxBounds.X) / 2.0f;
	float CenterY = (MinBounds.Y + MaxBounds.Y) / 2.0f;
    
	// Z축은 도면 바닥보다 높은 3000으로 세팅하여 월드 좌표 이동
	SetWorldLocation(FVector(CenterX, CenterY, 3000.0f));

	float PhysicalWidth = MaxBounds.X - MinBounds.X;
	float PhysicalHeight = MaxBounds.Y - MinBounds.Y;

	// 가로/세로 중 긴 쪽을 선택하여 OrthoWidth 재설정
	this->OrthoWidth = FMath::Max(PhysicalWidth, PhysicalHeight) + Padding;
}

void UHarnessMinimapCaptureComponent::UpdateMinimap()
{
	// 명시적으로 씬을 캡처하여 렌더 타겟 업데이트
	ApplyStableCaptureSettings();

	HiddenComponents.Empty();
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			TArray<UActorComponent*> Comps = Actor->GetComponentsByTag(UPrimitiveComponent::StaticClass(), TEXT("Ceiling"));
			for (UActorComponent* Comp : Comps)
			{
				if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp))
				{
					HiddenComponents.Add(PrimComp);
				}
			}
		}
	}

	CaptureScene();
}

void UHarnessMinimapCaptureComponent::OnRegister()
{
	Super::OnRegister();

	ApplyStableCaptureSettings();
}

void UHarnessMinimapCaptureComponent::BeginPlay()
{
	Super::BeginPlay();

	ApplyStableCaptureSettings();

	// PipelineManager 서브시스템에서 월드 상태 변경 이벤트 구독
	if (UHarnessPipelineManager* PipelineManager = GetWorld()->GetSubsystem<UHarnessPipelineManager>())
	{
		PipelineManager->OnWorldStateChanged.AddDynamic(this, &UHarnessMinimapCaptureComponent::UpdateMinimap);
	}
}
