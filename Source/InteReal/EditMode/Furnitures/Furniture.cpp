// Fill out your copyright notice in the Description page of Project Settings.

#include "Furniture.h"

AFurniture::AFurniture()
{
	PrimaryActorTick.bCanEverTick = false;
	PlacementState = EPlacementState::Preview;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;
}

void AFurniture::BeginPlay()
{
	Super::BeginPlay();

	// 원본 머터리얼로부터 Dynamic Instance 생성 (파라미터 변경용)
	if (UMaterialInterface* OriginalMat = MeshComponent->GetMaterial(0))
	{
		DynamicMaterial = UMaterialInstanceDynamic::Create(OriginalMat, this);
		MeshComponent->SetMaterial(0, DynamicMaterial);
	}
}

void AFurniture::SetPlacementState(EPlacementState NewState)
{
	PlacementState = NewState;

	if (!DynamicMaterial)
	{
		return;
	}
		

	// TintColor: RGB = 색상, A = 강도 (0이면 원본 색 유지)
	switch (NewState)
	{
		case EPlacementState::Preview:
			DynamicMaterial->SetVectorParameterValue(TEXT("TintColor"), FLinearColor(0.0f, 1.0f, 0.0f, 0.4f));
			break;
		case EPlacementState::Invalid:
			DynamicMaterial->SetVectorParameterValue(TEXT("TintColor"), FLinearColor(1.0f, 0.0f, 0.0f, 0.4f));
			break;
		case EPlacementState::Placed:
			DynamicMaterial->SetVectorParameterValue(TEXT("TintColor"), FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
			break;
	}
}
