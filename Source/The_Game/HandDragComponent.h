#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "HandDragComponent.generated.h"

class APlayerController;
class UPrimitiveComponent;

UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class THE_GAME_API UHandDragComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHandDragComponent();

	UFUNCTION(BlueprintCallable, Category="Hand Drag")
	void StartDraggingComponent(UPrimitiveComponent* Component);

	UFUNCTION(BlueprintCallable, Category="Hand Drag")
	void StopDragging();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag")
	FName LeftHandComponentName = TEXT("HandL");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag")
	FName RightHandComponentName = TEXT("HandR");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag")
	bool bEnsureClickableCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag")
	bool bEnsureMovable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag")
	bool bTemporarilyDisablePhysicsWhileDragging = true;

private:
	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> DraggedComponent;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> CachedPlayerController;

	FVector DragOffset = FVector::ZeroVector;
	float DragPlaneZ = 0.0f;
	bool bDraggedComponentWasSimulatingPhysics = false;

	UFUNCTION()
	void HandleHandClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed);

	void BindHandComponent(FName ComponentName);
	void CachePlayerController();
	UPrimitiveComponent* FindPrimitiveComponentByName(FName ComponentName) const;
	bool GetMousePointOnDragPlane(FVector& OutPoint) const;
};
