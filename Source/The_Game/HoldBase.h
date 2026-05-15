// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "HoldBase.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THE_GAME_API UHoldBase : public USceneComponent
{
	GENERATED_BODY()

public:
	UHoldBase();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hold")
	bool bCanGrab = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hold")
	float GripDifficulty = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hold")
	FName HoldType;

	UFUNCTION(BlueprintCallable, Category = "Hold")
	bool CanGrab() const;

	UFUNCTION(BlueprintCallable, Category = "Hold")
	FTransform GetGrabTransform() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Hold")
	void OnGrabbed(AActor* Grabber);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Hold")
	void OnReleased(AActor* Grabber);
		
};
