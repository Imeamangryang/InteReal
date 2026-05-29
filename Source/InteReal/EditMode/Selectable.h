// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Selectable.generated.h"

UENUM(BlueprintType)
enum class ESelectionState : uint8
{
	None,
	Hovered,
	Selected
};

UCLASS()
class INTEREAL_API ASelectable : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ASelectable();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
public:
	UPROPERTY(visibleAnywhere, BlueprintReadOnly, Category="Components")
	USceneComponent* SceneRoot;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	UStaticMeshComponent* SelectionMesh;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Selectable")
	UMaterial* HoveredMat;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Selectable")
	UMaterial* SelectedMat;
	
private:
	ESelectionState SelectionState;
	
public:
	UFUNCTION(BlueprintCallable)
	void SetSelectionState(ESelectionState NewSelectionState);
	
	UFUNCTION(BlueprintPure)
	ESelectionState GetSelectionState();
};
