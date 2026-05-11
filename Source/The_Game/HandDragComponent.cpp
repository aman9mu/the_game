#include "HandDragComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

UHandDragComponent::UHandDragComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UHandDragComponent::BeginPlay()
{
	Super::BeginPlay();

	CachePlayerController();
	BindHandComponent(LeftHandComponentName);
	BindHandComponent(RightHandComponentName);
}

void UHandDragComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!DraggedComponent)
	{
		return;
	}

	CachePlayerController();
	if (!CachedPlayerController || !CachedPlayerController->IsInputKeyDown(EKeys::LeftMouseButton))
	{
		StopDragging();
		return;
	}

	FVector MousePoint;
	if (GetMousePointOnDragPlane(MousePoint))
	{
		DraggedComponent->SetWorldLocation(MousePoint + DragOffset, false, nullptr, ETeleportType::TeleportPhysics);
	}
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

	DraggedComponent = Component;
	bDraggedComponentWasSimulatingPhysics = Component->IsSimulatingPhysics();

	if (bTemporarilyDisablePhysicsWhileDragging && bDraggedComponentWasSimulatingPhysics)
	{
		Component->SetSimulatePhysics(false);
	}

	DragPlaneZ = Component->GetComponentLocation().Z;

	FVector MousePoint;
	DragOffset = GetMousePointOnDragPlane(MousePoint)
		? Component->GetComponentLocation() - MousePoint
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

void UHandDragComponent::BindHandComponent(FName ComponentName)
{
	UPrimitiveComponent* Component = FindPrimitiveComponentByName(ComponentName);
	if (!Component)
	{
		return;
	}

	if (bEnsureClickableCollision)
	{
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
		if (Component && Component->GetFName() == ComponentName)
		{
			return Component;
		}
	}

	return nullptr;
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

	if (FMath::IsNearlyZero(FVector::DotProduct(WorldDirection, FVector::UpVector)))
	{
		return false;
	}

	const FPlane DragPlane(FVector(0.0f, 0.0f, DragPlaneZ), FVector::UpVector);
	OutPoint = FMath::RayPlaneIntersection(WorldOrigin, WorldDirection, DragPlane);
	return true;
}
