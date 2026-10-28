#include "CXGameState.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

ACXGameState::ACXGameState()
{
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(2.0f);
	static ConstructorHelpers::FObjectFinder<UParticleSystem> Hit(TEXT("/Game/StarterContent/Particles/P_Sparks.P_Sparks"));
	static ConstructorHelpers::FObjectFinder<UParticleSystem> Pickup(TEXT("/Game/StarterContent/Particles/P_Explosion.P_Explosion"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Sound(TEXT("/Game/StarterContent/Audio/Explosion_Cue.Explosion_Cue"));
	HitParticles = Hit.Object;
	PickupParticles = Pickup.Object;
	PickupSound = Sound.Object;
}

void ACXGameState::SetMatchData(ECXMatchPhase NewPhase, int32 Seconds)
{
	if (!HasAuthority()) return;
	Phase = NewPhase;
	RemainingSeconds = FMath::Max(0, Seconds);
	ForceNetUpdate();
	OnRep_Match();
}

void ACXGameState::SetLobby(bool bOpen, APlayerState* Leader)
{
	if (!HasAuthority()) return;
	bLobbyOpen = bOpen;
	LobbyLeader = Leader;
	ForceNetUpdate();
}

void ACXGameState::SetWinners(const TArray<APlayerState*>& NewWinners)
{
	if (!HasAuthority()) return;
	Winners.Reset();
	for (APlayerState* Winner : NewWinners) if (IsValid(Winner)) Winners.Add(Winner);
	ForceNetUpdate();
}

void ACXGameState::OnRep_Match()
{
	UE_LOG(LogTemp, Display, TEXT("CX_MATCH phase=%d time=%d winners=%d"), static_cast<int32>(Phase), RemainingSeconds, Winners.Num());
}

void ACXGameState::MulticastAttack_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Red : FColor::Green, false, 0.5f, 0, 3.0f);
	if (bHit && HitParticles) UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), HitParticles, End, FRotator::ZeroRotator, FVector(0.4f));
}

void ACXGameState::MulticastPickupEffect_Implementation(FVector_NetQuantize Location)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (PickupParticles) UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), PickupParticles, Location, FRotator::ZeroRotator, FVector(0.15f));
	if (PickupSound) UGameplayStatics::PlaySoundAtLocation(this, PickupSound, Location, 0.15f, 1.6f);
}

void ACXGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACXGameState, bLobbyOpen);
	DOREPLIFETIME(ACXGameState, LobbyLeader);
	DOREPLIFETIME(ACXGameState, Phase);
	DOREPLIFETIME(ACXGameState, RemainingSeconds);
	DOREPLIFETIME(ACXGameState, Winners);
}
