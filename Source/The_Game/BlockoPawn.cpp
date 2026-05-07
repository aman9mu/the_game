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
}

void ABlockoPawn::BeginPlay()
{
	Super::BeginPlay();
	
}

void ABlockoPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ABlockoPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

