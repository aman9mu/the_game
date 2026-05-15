#include "BlockoPawn.h"

ABlockoPawn::ABlockoPawn()
{
 	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BlockA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockA"));
	BlockA->SetupAttachment(Root);

	BlockB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockB"));
	BlockB->SetupAttachment(Root);

	Connector = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Connector"));
	Connector->SetupAttachment(Root);

	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void ABlockoPawn::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;
	}
	
}

void ABlockoPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC || !bIsDragging)
    {
        return;
    }

    if (!PC->IsInputKeyDown(EKeys::LeftMouseButton))
    {
        StopDragging();
        return;
    }

    FHitResult Hit;
    const bool bHit = PC->GetHitResultUnderCursorByChannel(
        UEngineTypes::ConvertToTraceType(ECC_Visibility),
        false,
        Hit
    );

    if (bHit)
    {
        FVector NewLocation = Hit.ImpactPoint;
        NewLocation.Z = DragHeight;
        SetActorLocation(NewLocation);
    }

}

void ABlockoPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void ABlockoPawn::StartDragging()
{
    bIsDragging = true;
}

void ABlockoPawn::StopDragging()
{
    bIsDragging = false;
}

