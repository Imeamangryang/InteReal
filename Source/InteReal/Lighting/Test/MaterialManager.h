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

	// 현재 러프니스 값을 기억할 변수
	float CurrentRoughness = 0.5f;
	
	// 타겟을 설정하는 함수 (클릭 시 호출)
	UFUNCTION(BlueprintCallable, Category = "Material Control")
	void SetTargetMesh(UStaticMeshComponent* NewMesh);

	// 머터리얼의 파라미터(Roughness, Metallic 등)를 변경하는 함수
	UFUNCTION(BlueprintCallable, Category = "Material Control")
	void UpdateMaterialProperty(FName ParamName, float Value);
	
	// 러프니스 조절용 함수 추가
	UFUNCTION(BlueprintCallable, Category = "Material Control")
	void AdjustRoughness(float Delta);
	
};