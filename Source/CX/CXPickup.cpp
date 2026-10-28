#include "CXPickup.h"
#include "CXPawn.h"
#include "CXPlayerState.h"
#include "CXGameState.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ACXPickup::ACXPickup()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(1.0f);
	PrimaryActorTick.bCanEverTick = true;
	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("PickupTrigger"));
	SetRootComponent(Trigger);
	Trigger->SetSphereRadius(140);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupVisual"));
	Visual->SetupAttachment(Trigger);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PickupLabel"));
	Label->SetupAttachment(Trigger);
	Label->SetRelativeLocation(FVector(0, 0, 100));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(35);
	Label->SetRelativeRotation(FRotator(0, 180, 0));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	CoinMesh = Sphere.Object;
	SpeedMesh = Cube.Object;
}

ACXSpeedPickup::ACXSpeedPickup() { Kind = ECXPickupKind::Speed; }

void ACXPickup::BeginPlay()
{
	Super::BeginPlay();
	const bool bCoin = Kind == ECXPickupKind::Coin;
	Visual->SetStaticMesh(bCoin ? CoinMesh : SpeedMesh);
	Visual->SetRelativeScale3D(bCoin ? FVector(0.85f, 0.2f, 0.85f) : FVector(0.6f));
	Label->SetText(FText::FromString(bCoin ? TEXT("+10 WON") : TEXT("+10 KM/H")));
	Label->SetTextRenderColor(bCoin ? FColor::Yellow : FColor::Cyan);
	if (HasAuthority())
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &ACXPickup::OnOverlap);
		Trigger->OnComponentEndOverlap.AddDynamic(this, &ACXPickup::OnEndOverlap);
		// Handle a vehicle already inside a newly spawned pickup.
		RetryCollection();
	}
	else Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (GetNetMode() == NM_DedicatedServer) SetActorTickEnabled(false);
}

void ACXPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Visual->AddLocalRotation(FRotator(0, 60 * DeltaSeconds, 0));
}

void ACXPickup::OnOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (Cast<ACXPawn>(OtherActor)) RetryCollection();
}

void ACXPickup::OnEndOverlap(UPrimitiveComponent*, AActor*, UPrimitiveComponent*, int32)
{
	RetryCollection();
}

void ACXPickup::RetryCollection()
{
	if (!HasAuthority() || bConsumed || IsActorBeingDestroyed()) return;
	TArray<AActor*> Vehicles;
	Trigger->GetOverlappingActors(Vehicles, ACXPawn::StaticClass());
	if (Vehicles.IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(CollectionRetryTimer);
		return;
	}
	for (AActor* Vehicle : Vehicles) if (TryCollect(Cast<ACXPawn>(Vehicle))) return;
	// Retry only occupied triggers: waiting vehicles and capped balances may
	// become eligible without a new overlap event (match start / shop purchase).
	if (!GetWorldTimerManager().IsTimerActive(CollectionRetryTimer))
		GetWorldTimerManager().SetTimer(CollectionRetryTimer, this, &ACXPickup::RetryCollection, 0.1f, true);
}

void ACXPickup::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(CollectionRetryTimer);
	Super::EndPlay(Reason);
}

bool ACXPickup::TryCollect(ACXPawn* Vehicle)
{
	if (!HasAuthority() || bConsumed || IsActorBeingDestroyed() || !IsValid(Vehicle)) return false;
	const ACXGameState* GS = GetWorld()->GetGameState<ACXGameState>();
	if (!GS || GS->GetPhase() != ECXMatchPhase::Playing) return false;
	// Also defend direct C++ calls: only a vehicle within the trigger may collect.
	if (FVector::DistSquared(Vehicle->GetActorLocation(), GetActorLocation()) > FMath::Square(400.0f)) return false;
	ACXPlayerState* Stats = Vehicle->GetPlayerState<ACXPlayerState>();
	if (!Stats) return false;
	const bool bAwarded = Kind == ECXPickupKind::Coin ? Stats->AddCoins(10) : Stats->IncreaseBonusMaxSpeed(10);
	if (!bAwarded) return false;
	bConsumed = true;
	Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Destroy();
	return true;
}

void ACXPickup::Destroyed()
{
	if (!bSilentDestruction && HasAuthority() && GetWorld() && !GetWorld()->bIsTearingDown)
	{
		if (ACXGameState* GS = GetWorld()->GetGameState<ACXGameState>()) GS->MulticastPickupEffect(GetActorLocation());
	}
	Super::Destroyed();
}
