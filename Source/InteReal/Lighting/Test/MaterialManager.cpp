#include "MaterialManager.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/UnrealMathUtility.h"

AMaterialManager::AMaterialManager()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AMaterialManager::SetTargetMesh(UStaticMeshComponent* NewMesh)
{
    if (NewMesh)
    {
        CurrentTargetMesh = NewMesh;
        CurrentTargetMesh->CreateDynamicMaterialInstance(0); 
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
    Roughness = FMath::Clamp(Roughness + Delta, 0.0f, 1.0f);
    UpdateMaterialProperty(FName("Roughness"), Roughness);
}
void AMaterialManager::AdjustMetallic(float Value)
{
    Metallic = FMath::Clamp(Value, 0.0f, 1.0f);
    UpdateMaterialProperty(FName("Metallic"), Metallic);
}

void AMaterialManager::AdjustSpecular(float Value)
{
    Specular = FMath::Clamp(Value, 0.0f, 1.0f);
    UpdateMaterialProperty(FName("Specular"), Specular);
}