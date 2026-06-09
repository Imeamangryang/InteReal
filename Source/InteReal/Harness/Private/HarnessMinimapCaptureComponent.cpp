#include "Public/HarnessMinimapCaptureComponent.h"
#include "Public/HarnessPipelineManager.h"
#include "EngineUtils.h"

UHarnessMinimapCaptureComponent::UHarnessMinimapCaptureComponent()
{
	PrimaryComponentTick.bCanEverTick = true; // 위젯 등에 데이터 공급이 필요할 수 있으므로 활성화
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// 미니맵용 캡처 디폴트 세팅
	ProjectionType = ECameraProjectionMode::Orthographic;
	CaptureSource = SCS_FinalColorLDR;
	
	bCaptureEveryFrame = false;
	bCaptureOnMovement = false;
	
	ShowFlags.SetLighting(false);
	ShowFlags.SetPostProcessing(false);
    
	// 수직 아래를 바라보도록 기본 회전값 고정
	SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
}

UTextureRenderTarget2D* UHarnessMinimapCaptureComponent::GetOrCreateRenderTarget(int32 Resolution)
{
	if (!MinimapRenderTarget)
	{
		MinimapRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("DynamicMinimapRT"));
		check(MinimapRenderTarget);
		MinimapRenderTarget->InitAutoFormat(Resolution, Resolution);
		MinimapRenderTarget->ClearColor = FLinearColor::Transparent;
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
	CaptureScene();
}

void UHarnessMinimapCaptureComponent::BeginPlay()
{
	Super::BeginPlay();

	// PipelineManager 서브시스템에서 월드 상태 변경 이벤트 구독
	if (UHarnessPipelineManager* PipelineManager = GetWorld()->GetSubsystem<UHarnessPipelineManager>())
	{
		PipelineManager->OnWorldStateChanged.AddDynamic(this, &UHarnessMinimapCaptureComponent::UpdateMinimap);
	}
}

void UHarnessMinimapCaptureComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// 여기에 매 프레임 필요한 미니맵 데이터 업데이트 로직 추가 가능
}