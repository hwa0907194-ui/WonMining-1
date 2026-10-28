#include "CXPickupSpawner.h"
#include "CXPickup.h"
#include "CXGameState.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ACXPickupSpawner::ACXPickupSpawner()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(1.0f);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SpawnPoint")));
	PickupClass = ACXCoinPickup::StaticClass();
}

void ACXPickupSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_Client && HasAuthority()) SpawnPickup();
}

void ACXPickupSpawner::SpawnPickup()
{
	if (GetNetMode() == NM_Client || !HasAuthority() || !PickupClass || IsValid(ActivePickup)) return;
	const ACXGameState* GS = GetWorld()->GetGameState<ACXGameState>();
	if (GS && GS->GetPhase() == ECXMatchPhase::Finished) return;
	ActivePickup = GetWorld()->SpawnActorDeferred<ACXPickup>(PickupClass, GetActorTransform(), this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (ActivePickup)
	{
		ActivePickup->OnDestroyed.AddDynamic(this, &ACXPickupSpawner::OnPickupDestroyed);
		UGameplayStatics::FinishSpawningActor(ActivePickup, GetActorTransform());
	}
}

void ACXPickupSpawner::OnPickupDestroyed(AActor* DestroyedActor)
{
	if (!HasAuthority() || DestroyedActor != ActivePickup) return;
	ActivePickup = nullptr;
	if (!IsActorBeingDestroyed()) GetWorldTimerManager().SetTimer(RespawnTimer, this, &ACXPickupSpawner::SpawnPickup, FMath::Max(0.1f, RespawnDelay), false);
}

void ACXPickupSpawner::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	if (IsValid(ActivePickup))
	{
		ActivePickup->OnDestroyed.RemoveDynamic(this, &ACXPickupSpawner::OnPickupDestroyed);
		if (HasAuthority()) ActivePickup->Destroy();
	}
	Super::EndPlay(Reason);
}

void ACXPickupSpawner::ResetForRound()
{
	if (!HasAuthority() || GetNetMode() == NM_Client) return;
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	if (IsValid(ActivePickup))
	{
		ActivePickup->OnDestroyed.RemoveDynamic(this, &ACXPickupSpawner::OnPickupDestroyed);
		ActivePickup->RemoveForReset();
	}
	ActivePickup = nullptr;
	SpawnPickup();
}
