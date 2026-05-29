// Fill out your copyright notice in the Description page of Project Settings.


#include "Selectable.h"


// Sets default values
ASelectable::ASelectable()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>("Scene Root");
	SetRootComponent(SceneRoot);

	SelectionMesh = CreateDefaultSubobject<UStaticMeshComponent>("Selection Mesh");
	SelectionMesh->SetupAttachment(SceneRoot);
	SelectionMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f), false, nullptr, ETeleportType::None);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshAsset(TEXT("/Engine/BasicShapes/Plane.Plane"));

	if (PlaneMeshAsset.Succeeded())
	{
		SelectionMesh->SetStaticMesh(PlaneMeshAsset.Object);
	}

	SelectionMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SelectionMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	SelectionMesh->SetGenerateOverlapEvents(false);
}

// Called when the game starts or when spawned
void ASelectable::BeginPlay()
{
	Super::BeginPlay();
	
	SetSelectionState(ESelectionState::None);
}

// Called every frame
void ASelectable::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

ESelectionState ASelectable::GetSelectionState()
{
	return SelectionState;
}

void ASelectable::SetSelectionState(ESelectionState NewSelectionState)
{
	SelectionState = NewSelectionState;

	switch (SelectionState)
	{
	case ESelectionState::None:
		SelectionMesh->SetVisibility(false, false);
		break;

	case ESelectionState::Hovered:
		SelectionMesh->SetVisibility(true, false);
		SelectionMesh->SetMaterial(0, HoveredMat);
		break;

	case ESelectionState::Selected:
		SelectionMesh->SetVisibility(true, false);
		SelectionMesh->SetMaterial(0, SelectedMat);
	}
}
