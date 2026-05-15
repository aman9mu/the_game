#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "HandDragComponent.generated.h"

class APlayerController;
class UPrimitiveComponent;

// Controls which world-space plane the mouse ray is projected onto while dragging a hand.
UENUM(BlueprintType)
enum class EHandDragPlaneMode : uint8
{
	CameraFacing UMETA(DisplayName="Camera Facing"),
	Horizontal UMETA(DisplayName="Horizontal")
};

// Locks the whole climber onto a 2D movement slice while still allowing vertical motion.
UENUM(BlueprintType)
enum class EHandDragAxisLock : uint8
{
	None UMETA(DisplayName="None"),
	X UMETA(DisplayName="Lock X"),
	Y UMETA(DisplayName="Lock Y")
};

// Selects which local body axis should point roughly along the line between both hands.
UENUM(BlueprintType)
enum class EBodyRotationAxis : uint8
{
	X UMETA(DisplayName="Local X"),
	Y UMETA(DisplayName="Local Y")
};

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

	// Camera-facing drag feels like screen-space movement; horizontal preserves the original flat-drag behavior.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag")
	EHandDragPlaneMode DragPlaneMode = EHandDragPlaneMode::CameraFacing;

	// Lock Y means X/Z climbing. Lock X means Y/Z climbing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag|Axis Lock")
	EHandDragAxisLock MovementAxisLock = EHandDragAxisLock::Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag|Body Constraint")
	FName BodyComponentName = TEXT("Body");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag|Body Constraint")
	bool bKeepBodyWithinArmReach = true;

	// Extra reach added to both arm lengths, useful if the default constraint feels too tight.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag|Body Constraint", meta=(ClampMin="0.0"))
	float ArmLengthSlack = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag|Body Constraint")
	bool bEnableBodyPhysics = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag|Body Constraint")
	bool bEnableBodyGravity = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag|Body Constraint")
	bool bDampenBodyVelocityAtReachLimit = true;

	// Rope-style forces let the body hang from the hands while gravity still acts on it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag|Body Physics")
	bool bUseBodyTetherForces = true;

	// False makes arms pull only when stretched. True also pushes when compressed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag|Body Physics")
	bool bMaintainFixedArmDistance = false;

	// When the dragged hand reaches full extension, move the body with it like a climbing pull.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag|Body Physics")
	bool bMoveBodyWhenDraggedHandReachesLimit = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag|Body Physics", meta=(ClampMin="0.0"))
	float BodyTetherStiffness = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag|Body Physics", meta=(ClampMin="0.0"))
	float BodyTetherDamping = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag|Body Physics", meta=(ClampMin="0.0"))
	float MaxTetherAcceleration = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag|Body Physics")
	bool bUseHardReachLimit = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag|Body Physics", meta=(ClampMin="0.0"))
	float HardReachLimitSlack = 10.0f;

	// Keeps the body visually lined up with the hands and prevents uncontrolled physics spin.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag|Body Rotation")
	bool bAlignBodyRotationToHands = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag|Body Rotation")
	EBodyRotationAxis BodyHandAlignmentAxis = EBodyRotationAxis::X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag|Body Rotation", meta=(ClampMin="0.0"))
	float BodyRotationInterpSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag|Body Rotation", meta=(ClampMin="0.0"))
	float BodyAngularDamping = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Drag|Body Rotation", meta=(ClampMin="0.0"))
	float MaxBodyAngularSpeed = 120.0f;

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
	TObjectPtr<UPrimitiveComponent> LeftHandComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> RightHandComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> BodyComponent;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> CachedPlayerController;

	FVector DragOffset = FVector::ZeroVector;
	FVector DragPlaneOrigin = FVector::ZeroVector;
	FVector DragPlaneNormal = FVector::UpVector;

	// Saved at BeginPlay so the initial Blueprint pose defines the arm reach.
	float LeftArmLength = 0.0f;
	float RightArmLength = 0.0f;

	// Shared X or Y coordinate used to keep hands/body on one climbing plane.
	float LockedAxisValue = 0.0f;
	bool bArmLengthsInitialized = false;
	bool bLockedAxisValueInitialized = false;
	bool bDraggedComponentWasSimulatingPhysics = false;

	UFUNCTION()
	void HandleHandClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed);

	void CacheTrackedComponents();
	void InitializeAxisLock();
	void InitializeArmLengths();
	FVector ApplyBodyConstraint(const FVector& DesiredDraggedLocation);
	void ApplyBodyTetherForces();
	void ApplyTetherForce(const UPrimitiveComponent* HandComponent, float TargetArmLength);
	FVector ApplyMovementAxisLock(const FVector& Location) const;
	bool ConstrainBodyToArmReach();
	void ConfigureBodyPhysics();
	void DampenBodyVelocityForCorrection(const FVector& PreviousLocation, const FVector& CorrectedLocation);
	void DampenLockedAxisVelocity();
	void EnforceMovementAxisLock();
	void StabilizeBodyRotation(float DeltaTime);
	FRotator GetDesiredBodyRotation() const;
	FVector GetPlaneNormalForAxisLock() const;
	FVector GetBodyLocationWithinArmReach(const FVector& DesiredBodyLocation, float ExtraReachSlack = 0.0f) const;
	void SetDragPlaneForComponent(const UPrimitiveComponent* Component);
	void BindHandComponent(FName ComponentName);
	void CachePlayerController();
	UPrimitiveComponent* FindPrimitiveComponentByName(FName ComponentName) const;
	UPrimitiveComponent* GetOtherHandComponent() const;
	float GetArmLengthForComponent(const UPrimitiveComponent* Component) const;
	float GetArmRestLengthForComponent(const UPrimitiveComponent* Component) const;
	bool CanApplyBodyConstraint() const;
	bool GetMousePointOnDragPlane(FVector& OutPoint) const;
};
