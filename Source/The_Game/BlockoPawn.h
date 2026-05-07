#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "BlockoPawn.generated.h"

class UStaticMeshComponent
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
	UPROPERTY(VisibleAnywhere)
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* BlockA;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* BlockB;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Connector;

	bool bDragging = false;
};
