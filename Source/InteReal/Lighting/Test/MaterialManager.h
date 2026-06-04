#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MaterialManager.generated.h"


UCLASS()
class INTEREAL_API AMaterialManager : public AActor
{
	GENERATED_BODY()
    
public:    
	AMaterialManager();

	// 현재 선택된 가구의 메쉬 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Material Control")
	UStaticMeshComponent* CurrentTargetMesh;
	
	// 현재 Roughness 값을 기억할 변수
	float Roughness = 0.5f;
	// 현재 Metallic 값을 기억할 변수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Control")
	float Metallic = 0.0f;
	// 현재 Specular 값을 기억할 변수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Control")
	float Specular = 0.5f;
	
	// 타겟을 설정하는 함수
	UFUNCTION(BlueprintCallable, Category = "Material Control")
	void SetTargetMesh(UStaticMeshComponent* NewMesh);

	// 머터리얼의 파라미터(Roughness, Metallic 등)를 변경하는 함수
	UFUNCTION(BlueprintCallable, Category = "Material Control")
	void UpdateMaterialProperty(FName ParamName, float Value);
	
	// 개별 조절 함수 (위젯 슬라이더용)
	UFUNCTION(BlueprintCallable, Category = "Material Control")
	void AdjustRoughness(float Value);

	UFUNCTION(BlueprintCallable, Category = "Material Control")
	void AdjustMetallic(float Value);

	UFUNCTION(BlueprintCallable, Category = "Material Control")
	void AdjustSpecular(float Value);
};
