#include "HoldBase.h"

UHoldBase::UHoldBase()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHoldBase::CanGrab() const
{
	return bCanGrab;
}

FTransform UHoldBase::GetGrabTransform() const
{
	return GetComponentTransform();
}

void UHoldBase::OnGrabbed_Implementation(AActor* Grabber)
{
	bCanGrab = false;
}

void UHoldBase::OnReleased_Implementation(AActor* Grabber)
{
	bCanGrab = true;
}