#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "BlockoPawn.generated.h"

class UStaticMeshComponent;

UCLASS()
class THE_GAME_API ABlockoPawn : public APawn
{
	GENERATED_BODY()

public:

	ABlockoPawn();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USceneComponent* Root;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMeshComponent* BlockA;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMeshComponent* BlockB;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMeshComponent* Connector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DragHeight = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsDragging = false;

	UFUNCTION(BlueprintCallable, Category = "Drag")
	void StartDragging();

	UFUNCTION(BlueprintCallable, Category = "Drag")
	void StopDragging();
};
