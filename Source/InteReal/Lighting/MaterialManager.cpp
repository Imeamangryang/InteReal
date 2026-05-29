#include "MaterialManager.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/UnrealMathUtility.h" // FMath 사용을 위해 추가

AMaterialManager::AMaterialManager()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AMaterialManager::SetTargetMesh(UStaticMeshComponent* NewMesh)
{
    if (NewMesh)
    {
       CurrentTargetMesh = NewMesh;
       UE_LOG(LogTemp, Log, TEXT("[MaterialManager] 타겟 설정 완료: %s"), *CurrentTargetMesh->GetOwner()->GetName());
    }
}

void AMaterialManager::UpdateMaterialProperty(FName ParamName, float Value)
{
    if (!CurrentTargetMesh) return;

    UMaterialInstanceDynamic* DynMat = CurrentTargetMesh->CreateDynamicMaterialInstance(0);
    
    if (DynMat)
    {
       DynMat->SetScalarParameterValue(ParamName, Value);
       UE_LOG(LogTemp, Log, TEXT("[MaterialManager] %s의 %s 값을 %.2f로 업데이트"), 
          *CurrentTargetMesh->GetOwner()->GetName(), *ParamName.ToString(), Value);
    }
}

void AMaterialManager::AdjustRoughness(float Delta)
{
    // 값을 더하고 0.0 ~ 1.0 사이로 고정
    CurrentRoughness = FMath::Clamp(CurrentRoughness + Delta, 0.0f, 1.0f);
    UpdateMaterialProperty(FName("Roughness"), CurrentRoughness);
}