#include "HandDragComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	// Projects a point back onto/inside a sphere. Used for hand reach and body reach limits.
	FVector ClampPointToRadius(const FVector& Point, const FVector& Center, float Radius)
	{
		const FVector Delta = Point - Center;
		const float Distance = Delta.Size();
		if (Distance <= Radius || Distance <= KINDA_SMALL_NUMBER)
		{
			return Point;
		}

		return Center + (Delta / Distance) * Radius;
	}
}

UHandDragComponent::UHandDragComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Run before physics so tether forces and stabilizing corrections affect this frame's simulation.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UHandDragComponent::BeginPlay()
{
	Super::BeginPlay();

	CachePlayerController();
	CacheTrackedComponents();

	// Set up the movement plane before measuring arm lengths so the initial pose is consistent.
	InitializeAxisLock();
	EnforceMovementAxisLock();
	InitializeArmLengths();
	ConfigureBodyPhysics();
	BindHandComponent(LeftHandComponentName);
	BindHandComponent(RightHandComponentName);
}

void UHandDragComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Keep all moving parts on the selected X/Z or Y/Z climbing plane.
	EnforceMovementAxisLock();

	if (!DraggedComponent)
	{
		// No active hand drag: the body is just hanging from both hands under gravity.
		ApplyBodyTetherForces();
		ConstrainBodyToArmReach();
		StabilizeBodyRotation(DeltaTime);
		return;
	}

	CachePlayerController();
	if (!CachedPlayerController || !CachedPlayerController->IsInputKeyDown(EKeys::LeftMouseButton))
	{
		StopDragging();
		ApplyBodyTetherForces();
		ConstrainBodyToArmReach();
		StabilizeBodyRotation(DeltaTime);
		return;
	}

	FVector MousePoint;
	if (GetMousePointOnDragPlane(MousePoint))
	{
		// Mouse movement proposes a hand location; constraints may move the body if the arm is taut.
		const FVector DesiredLocation = ApplyMovementAxisLock(MousePoint + DragOffset);
		const FVector ConstrainedLocation = ApplyBodyConstraint(DesiredLocation);
		DraggedComponent->SetWorldLocation(ConstrainedLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	ApplyBodyTetherForces();
	ConstrainBodyToArmReach();
	StabilizeBodyRotation(DeltaTime);
}

void UHandDragComponent::StartDraggingComponent(UPrimitiveComponent* Component)
{
	if (!Component)
	{
		return;
	}

	if (DraggedComponent && DraggedComponent != Component)
	{
		StopDragging();
	}

	CacheTrackedComponents();
	InitializeAxisLock();
	EnforceMovementAxisLock();
	InitializeArmLengths();
	ConfigureBodyPhysics();

	DraggedComponent = Component;
	CachePlayerController();
	bDraggedComponentWasSimulatingPhysics = Component->IsSimulatingPhysics();

	// Hands are positioned directly while dragged; disable physics temporarily if a hand uses it.
	if (bTemporarilyDisablePhysicsWhileDragging && bDraggedComponentWasSimulatingPhysics)
	{
		Component->SetSimulatePhysics(false);
	}

	SetDragPlaneForComponent(Component);

	FVector MousePoint;
	DragOffset = GetMousePointOnDragPlane(MousePoint)
		? Component->GetComponentLocation() - ApplyMovementAxisLock(MousePoint)
		: FVector::ZeroVector;
}

void UHandDragComponent::StopDragging()
{
	if (DraggedComponent && bTemporarilyDisablePhysicsWhileDragging && bDraggedComponentWasSimulatingPhysics)
	{
		DraggedComponent->SetSimulatePhysics(true);
	}

	DraggedComponent = nullptr;
	bDraggedComponentWasSimulatingPhysics = false;
	DragOffset = FVector::ZeroVector;
}

void UHandDragComponent::HandleHandClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed)
{
	if (ButtonPressed == EKeys::LeftMouseButton)
	{
		StartDraggingComponent(TouchedComponent);
	}
}

void UHandDragComponent::CacheTrackedComponents()
{
	// BP_TESTPAWN owns these components, so resolve them by Blueprint component name at runtime.
	LeftHandComponent = FindPrimitiveComponentByName(LeftHandComponentName);
	RightHandComponent = FindPrimitiveComponentByName(RightHandComponentName);
	BodyComponent = FindPrimitiveComponentByName(BodyComponentName);

	if (bEnsureMovable && BodyComponent)
	{
		BodyComponent->SetMobility(EComponentMobility::Movable);
	}
}

void UHandDragComponent::InitializeAxisLock()
{
	if (bLockedAxisValueInitialized || MovementAxisLock == EHandDragAxisLock::None)
	{
		return;
	}

	const UPrimitiveComponent* SourceComponent = BodyComponent ? BodyComponent.Get() : LeftHandComponent.Get();
	if (!SourceComponent)
	{
		return;
	}

	const FVector SourceLocation = SourceComponent->GetComponentLocation();

	// The locked plane is captured from the initial body/hand placement.
	LockedAxisValue = MovementAxisLock == EHandDragAxisLock::X ? SourceLocation.X : SourceLocation.Y;
	bLockedAxisValueInitialized = true;
}

void UHandDragComponent::InitializeArmLengths()
{
	if (bArmLengthsInitialized)
	{
		return;
	}

	if (!BodyComponent || !LeftHandComponent || !RightHandComponent)
	{
		LeftArmLength = 0.0f;
		RightArmLength = 0.0f;
		bArmLengthsInitialized = false;
		return;
	}

	const FVector BodyLocation = BodyComponent->GetComponentLocation();

	// The starting distances are treated as the natural arm reach.
	LeftArmLength = FVector::Distance(BodyLocation, LeftHandComponent->GetComponentLocation());
	RightArmLength = FVector::Distance(BodyLocation, RightHandComponent->GetComponentLocation());
	bArmLengthsInitialized = true;
}

FVector UHandDragComponent::ApplyBodyConstraint(const FVector& DesiredDraggedLocation)
{
	// Active climbing step:
	// 1. Let the dragged hand move within its arm reach from the body.
	// 2. If it tries to move beyond that reach, pull the body with it.
	// 3. Keep the body within reach of the stationary hand, so that hand acts as the anchor.
	if (!CanApplyBodyConstraint())
	{
		return ApplyMovementAxisLock(DesiredDraggedLocation);
	}

	const UPrimitiveComponent* OtherHandComponent = GetOtherHandComponent();
	if (!OtherHandComponent)
	{
		return ApplyMovementAxisLock(DesiredDraggedLocation);
	}

	const float DraggedArmLength = GetArmLengthForComponent(DraggedComponent);
	const float OtherArmLength = GetArmLengthForComponent(OtherHandComponent);
	if (DraggedArmLength <= 0.0f || OtherArmLength <= 0.0f)
	{
		return ApplyMovementAxisLock(DesiredDraggedLocation);
	}

	const FVector BodyLocation = BodyComponent->GetComponentLocation();
	const FVector OtherHandLocation = OtherHandComponent->GetComponentLocation();
	const float MaxHandSeparation = DraggedArmLength + OtherArmLength;

	FVector ConstrainedHandLocation = ApplyMovementAxisLock(DesiredDraggedLocation);
	FVector BodyTarget = ApplyMovementAxisLock(BodyLocation);

	if (bMoveBodyWhenDraggedHandReachesLimit)
	{
		const FVector DesiredHandToBody = BodyTarget - ConstrainedHandLocation;
		const float DesiredHandToBodyDistance = DesiredHandToBody.Size();
		if (DesiredHandToBodyDistance > DraggedArmLength && DesiredHandToBodyDistance > KINDA_SMALL_NUMBER)
		{
			// Move the body just enough to keep the dragged arm at full extension.
			BodyTarget = ConstrainedHandLocation + DesiredHandToBody / DesiredHandToBodyDistance * DraggedArmLength;
			BodyTarget = ApplyMovementAxisLock(BodyTarget);

			// Then keep that body movement legal relative to both hands.
			BodyTarget = ClampPointToRadius(BodyTarget, OtherHandLocation, OtherArmLength);
			BodyTarget = ClampPointToRadius(BodyTarget, ConstrainedHandLocation, DraggedArmLength);
			BodyTarget = ApplyMovementAxisLock(BodyTarget);

			if (!BodyTarget.Equals(BodyLocation, KINDA_SMALL_NUMBER))
			{
				BodyComponent->SetWorldLocation(BodyTarget, false, nullptr, ETeleportType::TeleportPhysics);
				DampenBodyVelocityForCorrection(BodyLocation, BodyTarget);
				DampenLockedAxisVelocity();
			}
		}
	}

	ConstrainedHandLocation = ClampPointToRadius(ConstrainedHandLocation, BodyTarget, DraggedArmLength);

	FVector OtherToDragged = ConstrainedHandLocation - OtherHandLocation;

	const float DesiredHandSeparation = OtherToDragged.Size();
	if (MaxHandSeparation > 0.0f && DesiredHandSeparation > MaxHandSeparation)
	{
		ConstrainedHandLocation = ApplyMovementAxisLock(OtherHandLocation + (OtherToDragged / DesiredHandSeparation) * MaxHandSeparation);
	}

	return ConstrainedHandLocation;
}

void UHandDragComponent::ApplyBodyTetherForces()
{
	// Passive support: both hands behave like ropes pulling the physics body back when stretched.
	if (!bUseBodyTetherForces || !CanApplyBodyConstraint() || !BodyComponent->IsSimulatingPhysics())
	{
		return;
	}

	ApplyTetherForce(LeftHandComponent.Get(), GetArmRestLengthForComponent(LeftHandComponent.Get()));
	ApplyTetherForce(RightHandComponent.Get(), GetArmRestLengthForComponent(RightHandComponent.Get()));
}

void UHandDragComponent::ApplyTetherForce(const UPrimitiveComponent* HandComponent, float TargetArmLength)
{
	if (!HandComponent || !BodyComponent || TargetArmLength <= 0.0f)
	{
		return;
	}

	const FVector BodyLocation = ApplyMovementAxisLock(BodyComponent->GetComponentLocation());
	const FVector HandLocation = ApplyMovementAxisLock(HandComponent->GetComponentLocation());
	const FVector HandToBody = BodyLocation - HandLocation;
	const float Distance = HandToBody.Size();
	if (Distance <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector Direction = HandToBody / Distance;
	const float DistanceError = Distance - TargetArmLength;
	const bool bIsDraggedHand = HandComponent == DraggedComponent;

	// Rope mode: slack arms do nothing. Fixed-distance mode also pushes when compressed.
	if ((!bMaintainFixedArmDistance || bIsDraggedHand) && DistanceError <= 0.0f)
	{
		return;
	}

	const float RadialVelocity = FVector::DotProduct(BodyComponent->GetPhysicsLinearVelocity(), Direction);

	// Spring-damper acceleration toward the target rope length.
	const float AccelerationMagnitude = FMath::Clamp(
		-DistanceError * BodyTetherStiffness - RadialVelocity * BodyTetherDamping,
		-MaxTetherAcceleration,
		MaxTetherAcceleration
	);

	BodyComponent->AddForce(Direction * AccelerationMagnitude, NAME_None, true);
}

FVector UHandDragComponent::ApplyMovementAxisLock(const FVector& Location) const
{
	FVector LockedLocation = Location;
	if (!bLockedAxisValueInitialized)
	{
		return LockedLocation;
	}

	if (MovementAxisLock == EHandDragAxisLock::X)
	{
		LockedLocation.X = LockedAxisValue;
	}
	else if (MovementAxisLock == EHandDragAxisLock::Y)
	{
		LockedLocation.Y = LockedAxisValue;
	}

	return LockedLocation;
}

bool UHandDragComponent::ConstrainBodyToArmReach()
{
	// Safety net for extreme cases: avoid teleporting every frame unless the body exceeds max reach.
	if (!bUseHardReachLimit || !CanApplyBodyConstraint())
	{
		return false;
	}

	const FVector BodyLocation = BodyComponent->GetComponentLocation();
	const FVector BodyTarget = GetBodyLocationWithinArmReach(ApplyMovementAxisLock(BodyLocation), HardReachLimitSlack);
	if (BodyTarget.Equals(BodyLocation, KINDA_SMALL_NUMBER))
	{
		return false;
	}

	BodyComponent->SetWorldLocation(BodyTarget, false, nullptr, ETeleportType::TeleportPhysics);
	DampenBodyVelocityForCorrection(BodyLocation, BodyTarget);
	DampenLockedAxisVelocity();
	return true;
}

void UHandDragComponent::ConfigureBodyPhysics()
{
	if (!BodyComponent)
	{
		return;
	}

	if (bEnableBodyPhysics)
	{
		BodyComponent->SetSimulatePhysics(true);
	}

	BodyComponent->SetEnableGravity(bEnableBodyGravity);
	BodyComponent->SetAngularDamping(BodyAngularDamping);
}

void UHandDragComponent::DampenBodyVelocityForCorrection(const FVector& PreviousLocation, const FVector& CorrectedLocation)
{
	if (!bDampenBodyVelocityAtReachLimit || !BodyComponent || !BodyComponent->IsSimulatingPhysics())
	{
		return;
	}

	const FVector Correction = CorrectedLocation - PreviousLocation;
	if (Correction.IsNearlyZero())
	{
		return;
	}

	const FVector LimitDirection = Correction.GetSafeNormal();
	const FVector Velocity = BodyComponent->GetPhysicsLinearVelocity();
	const float SpeedAwayFromReach = FVector::DotProduct(Velocity, -LimitDirection);
	if (SpeedAwayFromReach > 0.0f)
	{
		// Remove only the velocity component that would immediately stretch past the corrected limit.
		BodyComponent->SetPhysicsLinearVelocity(Velocity + LimitDirection * SpeedAwayFromReach);
	}
}

void UHandDragComponent::DampenLockedAxisVelocity()
{
	if (MovementAxisLock == EHandDragAxisLock::None || !BodyComponent || !BodyComponent->IsSimulatingPhysics())
	{
		return;
	}

	FVector Velocity = BodyComponent->GetPhysicsLinearVelocity();
	if (MovementAxisLock == EHandDragAxisLock::X)
	{
		Velocity.X = 0.0f;
	}
	else if (MovementAxisLock == EHandDragAxisLock::Y)
	{
		Velocity.Y = 0.0f;
	}

	BodyComponent->SetPhysicsLinearVelocity(Velocity);
}

void UHandDragComponent::EnforceMovementAxisLock()
{
	if (!bLockedAxisValueInitialized || MovementAxisLock == EHandDragAxisLock::None)
	{
		return;
	}

	UPrimitiveComponent* ComponentsToLock[] = { LeftHandComponent.Get(), RightHandComponent.Get(), BodyComponent.Get() };
	for (UPrimitiveComponent* Component : ComponentsToLock)
	{
		if (!Component)
		{
			continue;
		}

		const FVector CurrentLocation = Component->GetComponentLocation();
		const FVector LockedLocation = ApplyMovementAxisLock(CurrentLocation);
		if (!LockedLocation.Equals(CurrentLocation, KINDA_SMALL_NUMBER))
		{
			Component->SetWorldLocation(LockedLocation, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	DampenLockedAxisVelocity();
}

void UHandDragComponent::StabilizeBodyRotation(float DeltaTime)
{
	// Align the cube to the hand-to-hand line, while trimming angular velocity that causes random spin.
	if (!bAlignBodyRotationToHands || !BodyComponent || !LeftHandComponent || !RightHandComponent)
	{
		return;
	}

	if (BodyComponent->IsSimulatingPhysics())
	{
		BodyComponent->SetAngularDamping(BodyAngularDamping);

		FVector AngularVelocity = BodyComponent->GetPhysicsAngularVelocityInDegrees();

		// Keep rotation mostly within the same plane as the climbing movement.
		if (MovementAxisLock == EHandDragAxisLock::X)
		{
			AngularVelocity.Y = 0.0f;
			AngularVelocity.Z = 0.0f;
		}
		else if (MovementAxisLock == EHandDragAxisLock::Y)
		{
			AngularVelocity.X = 0.0f;
			AngularVelocity.Z = 0.0f;
		}

		if (MaxBodyAngularSpeed > 0.0f)
		{
			AngularVelocity = AngularVelocity.GetClampedToMaxSize(MaxBodyAngularSpeed);
		}

		BodyComponent->SetPhysicsAngularVelocityInDegrees(AngularVelocity);
	}

	const FRotator CurrentRotation = BodyComponent->GetComponentRotation();
	const FRotator TargetRotation = GetDesiredBodyRotation();
	const FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, BodyRotationInterpSpeed);
	BodyComponent->SetWorldRotation(NewRotation, false, nullptr, ETeleportType::TeleportPhysics);
}

FRotator UHandDragComponent::GetDesiredBodyRotation() const
{
	// The hand direction becomes the cube's chosen local X or Y axis.
	FVector HandDirection = ApplyMovementAxisLock(RightHandComponent->GetComponentLocation())
		- ApplyMovementAxisLock(LeftHandComponent->GetComponentLocation());

	if (HandDirection.IsNearlyZero())
	{
		return BodyComponent->GetComponentRotation();
	}

	HandDirection.Normalize();

	const FVector PlaneNormal = GetPlaneNormalForAxisLock();
	FVector UpVector = FVector::CrossProduct(HandDirection, PlaneNormal);
	if (UpVector.IsNearlyZero())
	{
		UpVector = FVector::UpVector;
	}
	UpVector.Normalize();

	if (BodyHandAlignmentAxis == EBodyRotationAxis::Y)
	{
		const FVector ForwardVector = FVector::CrossProduct(UpVector, HandDirection).GetSafeNormal();
		return FRotationMatrix::MakeFromXY(ForwardVector, HandDirection).Rotator();
	}

	return FRotationMatrix::MakeFromXZ(HandDirection, UpVector).Rotator();
}

FVector UHandDragComponent::GetPlaneNormalForAxisLock() const
{
	if (MovementAxisLock == EHandDragAxisLock::X)
	{
		return FVector::ForwardVector;
	}

	if (MovementAxisLock == EHandDragAxisLock::Y)
	{
		return FVector::RightVector;
	}

	return FVector::UpVector;
}

FVector UHandDragComponent::GetBodyLocationWithinArmReach(const FVector& DesiredBodyLocation, float ExtraReachSlack) const
{
	if (!CanApplyBodyConstraint())
	{
		return ApplyMovementAxisLock(DesiredBodyLocation);
	}

	const float LeftReach = LeftArmLength + ArmLengthSlack + ExtraReachSlack;
	const float RightReach = RightArmLength + ArmLengthSlack + ExtraReachSlack;
	if (LeftReach <= 0.0f || RightReach <= 0.0f)
	{
		return ApplyMovementAxisLock(DesiredBodyLocation);
	}

	FVector BodyTarget = ApplyMovementAxisLock(DesiredBodyLocation);

	// Alternating projection into both reach spheres gives a stable point within both arm limits.
	for (int32 Iteration = 0; Iteration < 8; ++Iteration)
	{
		BodyTarget = ClampPointToRadius(BodyTarget, LeftHandComponent->GetComponentLocation(), LeftReach);
		BodyTarget = ClampPointToRadius(BodyTarget, RightHandComponent->GetComponentLocation(), RightReach);
		BodyTarget = ApplyMovementAxisLock(BodyTarget);
	}

	return BodyTarget;
}

void UHandDragComponent::SetDragPlaneForComponent(const UPrimitiveComponent* Component)
{
	// Build the drag plane at the clicked hand so cursor movement starts without a location jump.
	DragPlaneOrigin = Component ? Component->GetComponentLocation() : FVector::ZeroVector;
	DragPlaneNormal = FVector::UpVector;

	if (DragPlaneMode == EHandDragPlaneMode::CameraFacing
		&& CachedPlayerController
		&& CachedPlayerController->PlayerCameraManager)
	{
		DragPlaneNormal = CachedPlayerController->PlayerCameraManager->GetActorForwardVector();
	}

	if (DragPlaneNormal.IsNearlyZero())
	{
		DragPlaneNormal = FVector::UpVector;
	}

	DragPlaneNormal.Normalize();
}

void UHandDragComponent::BindHandComponent(FName ComponentName)
{
	UPrimitiveComponent* Component = FindPrimitiveComponentByName(ComponentName);
	if (!Component)
	{
		return;
	}

	if (bEnsureClickableCollision)
	{
		// OnClicked needs query collision that blocks visibility traces.
		const ECollisionEnabled::Type CollisionEnabled = Component->GetCollisionEnabled();
		if (CollisionEnabled == ECollisionEnabled::NoCollision)
		{
			Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}
		else if (CollisionEnabled == ECollisionEnabled::PhysicsOnly)
		{
			Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}

		Component->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}

	if (bEnsureMovable)
	{
		Component->SetMobility(EComponentMobility::Movable);
	}

	Component->OnClicked.RemoveDynamic(this, &UHandDragComponent::HandleHandClicked);
	Component->OnClicked.AddDynamic(this, &UHandDragComponent::HandleHandClicked);
}

void UHandDragComponent::CachePlayerController()
{
	if (CachedPlayerController)
	{
		return;
	}

	CachedPlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!CachedPlayerController)
	{
		return;
	}

	CachedPlayerController->bShowMouseCursor = true;
	CachedPlayerController->bEnableClickEvents = true;
	CachedPlayerController->bEnableMouseOverEvents = true;

	// Game and UI input keeps the cursor active while still allowing normal gameplay input.
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	CachedPlayerController->SetInputMode(InputMode);
}

UPrimitiveComponent* UHandDragComponent::FindPrimitiveComponentByName(FName ComponentName) const
{
	const AActor* Owner = GetOwner();
	if (!Owner || ComponentName.IsNone())
	{
		return nullptr;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Owner->GetComponents(PrimitiveComponents);

	for (UPrimitiveComponent* Component : PrimitiveComponents)
	{
		if (!Component)
		{
			continue;
		}

		const FString RequestedName = ComponentName.ToString();
		const FString ActualName = Component->GetName();
		if (Component->GetFName() == ComponentName || ActualName == RequestedName || ActualName.StartsWith(RequestedName + TEXT("_")))
		{
			return Component;
		}
	}

	return nullptr;
}

UPrimitiveComponent* UHandDragComponent::GetOtherHandComponent() const
{
	if (DraggedComponent == LeftHandComponent)
	{
		return RightHandComponent;
	}

	if (DraggedComponent == RightHandComponent)
	{
		return LeftHandComponent;
	}

	return nullptr;
}

float UHandDragComponent::GetArmLengthForComponent(const UPrimitiveComponent* Component) const
{
	if (Component == LeftHandComponent)
	{
		return LeftArmLength + ArmLengthSlack;
	}

	if (Component == RightHandComponent)
	{
		return RightArmLength + ArmLengthSlack;
	}

	return 0.0f;
}

float UHandDragComponent::GetArmRestLengthForComponent(const UPrimitiveComponent* Component) const
{
	if (Component == LeftHandComponent)
	{
		return LeftArmLength;
	}

	if (Component == RightHandComponent)
	{
		return RightArmLength;
	}

	return 0.0f;
}

bool UHandDragComponent::CanApplyBodyConstraint() const
{
	return bKeepBodyWithinArmReach
		&& BodyComponent
		&& LeftHandComponent
		&& RightHandComponent
		&& LeftArmLength > 0.0f
		&& RightArmLength > 0.0f;
}

bool UHandDragComponent::GetMousePointOnDragPlane(FVector& OutPoint) const
{
	if (!CachedPlayerController)
	{
		return false;
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	if (!CachedPlayerController->DeprojectMousePositionToWorld(WorldOrigin, WorldDirection))
	{
		return false;
	}

	if (FMath::IsNearlyZero(FVector::DotProduct(WorldDirection, DragPlaneNormal)))
	{
		return false;
	}

	const FPlane DragPlane(DragPlaneOrigin, DragPlaneNormal);
	OutPoint = FMath::RayPlaneIntersection(WorldOrigin, WorldDirection, DragPlane);
	return true;
}
