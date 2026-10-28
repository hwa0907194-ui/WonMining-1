#include "CXGameplaySmokeTest.h"
#include "../CXGameMode.h"
#include "../CXGameState.h"
#include "../CXPlayerState.h"
#include "../CXPlayerController.h"
#include "../CXPawn.h"
#include "../CXPickup.h"
#include "../CXPickupSpawner.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

void ScheduleCXGameplayServerTest(ACXGameMode* Mode)
{
#if !UE_BUILD_SHIPPING
	if (!FParse::Param(FCommandLine::Get(), TEXT("CXGameplayTest"))) return;
	TWeakObjectPtr<ACXGameMode> WeakMode(Mode);
	FTimerHandle WaitingOverlap;
	Mode->GetWorldTimerManager().SetTimer(WaitingOverlap, [WeakMode]()
	{
		if (!WeakMode.IsValid()) return;
		UWorld* World = WeakMode->GetWorld();
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			ACXPawn* Car = Cast<ACXPawn>(It->Get()->GetPawn());
			if (!Car) continue;
			Car->GetMesh()->SetSimulatePhysics(false);
			Car->SetActorLocation(FVector(100000, 90000, 10000), false, nullptr, ETeleportType::TeleportPhysics);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ACXPickup* Coin = World->SpawnActor<ACXCoinPickup>(ACXCoinPickup::StaticClass(), Car->GetActorLocation(), FRotator::ZeroRotator, Params);
			const bool bWaiting = World->GetGameState<ACXGameState>()->GetPhase() == ECXMatchPhase::Waiting && IsValid(Coin) && Car->GetPlayerState<ACXPlayerState>()->GetCoins() == 0;
			TWeakObjectPtr<ACXPawn> WeakCar(Car);
			TWeakObjectPtr<ACXPickup> WeakCoin(Coin);
			FTimerHandle Check;
			World->GetTimerManager().SetTimer(Check, [WeakCar, WeakCoin, bWaiting]()
			{
				if (!WeakCar.IsValid()) return;
				ACXPlayerState* PS = WeakCar->GetPlayerState<ACXPlayerState>();
				const bool bCollected = bWaiting && !WeakCoin.IsValid() && PS && PS->GetCoins() == 10;
				UE_LOG(LogTemp, Display, TEXT("CX_GAMEPLAY_WAITING_OVERLAP %s"), bCollected ? TEXT("PASS") : TEXT("FAIL"));
				// Restore the fixture for the attack/tie assertions below.
				if (PS) PS->RestoreEconomy(0, 0);
				if (WeakCoin.IsValid()) WeakCoin->RemoveForReset();
			}, 5.8f, false);
			break;
		}
	}, 0.2f, false);
	FTimerHandle Stage;
	Mode->GetWorldTimerManager().SetTimer(Stage, [WeakMode]()
	{
		if (!WeakMode.IsValid()) return;
		UWorld* World = WeakMode->GetWorld();
		TArray<ACXPawn*> Cars;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			if (ACXPawn* Car = Cast<ACXPawn>(It->Get()->GetPawn())) Cars.Add(Car);
		if (Cars.Num() != 2) { UE_LOG(LogTemp, Error, TEXT("CX_GAMEPLAY FAIL expected two cars")); return; }
		ACXPawn* Attacker = Cars[0];
		ACXPawn* Victim = Cars[1];
		for (ACXPawn* Car : Cars) Car->GetMesh()->SetSimulatePhysics(false);
		Attacker->SetActorLocationAndRotation(FVector(100000, 100000, 10000), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
		Victim->SetActorLocationAndRotation(FVector(100700, 100000, 10000), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
		ACXPlayerState* A = Attacker->GetPlayerState<ACXPlayerState>();
		ACXPlayerState* V = Victim->GetPlayerState<ACXPlayerState>();
		if (!A || !V) { UE_LOG(LogTemp, Error, TEXT("CX_GAMEPLAY FAIL player state missing")); return; }
		V->AddCoins(10);
		Attacker->Fire();
		const bool bHit = A->GetCoins() == 5 && V->GetCoins() == 5;
		Attacker->Fire();
		const bool bCooldown = A->GetCoins() == 5 && V->GetCoins() == 5;
		UE_LOG(LogTemp, Display, TEXT("CX_GAMEPLAY_TRACE %s cooldown=%d attacker=%d victim=%d"), bHit ? TEXT("PASS") : TEXT("FAIL"), bCooldown, A->GetCoins(), V->GetCoins());
		TWeakObjectPtr<ACXPawn> WeakAttacker(Attacker), WeakVictim(Victim);
		FTimerHandle SecondStage;
		World->GetTimerManager().SetTimer(SecondStage, [WeakAttacker, WeakVictim]()
		{
			if (!WeakAttacker.IsValid() || !WeakVictim.IsValid()) return;
			ACXPawn* Car = WeakAttacker.Get();
			ACXPlayerState* AS = Car->GetPlayerState<ACXPlayerState>();
			ACXPlayerState* VS = WeakVictim->GetPlayerState<ACXPlayerState>();
			VS->SpendCoins(1);
			Car->Fire();
			UE_LOG(LogTemp, Display, TEXT("CX_GAMEPLAY_UNDER_FIVE %s"), AS->GetCoins() == 5 && VS->GetCoins() == 4 ? TEXT("PASS") : TEXT("FAIL"));
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			// A genuine overlap on the authoritative vehicle awards exactly once.
			ACXCoinPickup* Coin = Car->GetWorld()->SpawnActor<ACXCoinPickup>(ACXCoinPickup::StaticClass(), Car->GetActorLocation(), FRotator::ZeroRotator, Params);
			if (IsValid(Coin)) Coin->TryCollect(Car);
			ACXSpeedPickup* Buff = Car->GetWorld()->SpawnActor<ACXSpeedPickup>(ACXSpeedPickup::StaticClass(), Car->GetActorLocation(), FRotator::ZeroRotator, Params);
			if (IsValid(Buff)) Buff->TryCollect(Car);
			UE_LOG(LogTemp, Display, TEXT("CX_GAMEPLAY_PICKUPS %s coins=%d bonus=%.1f"), AS->GetCoins() == 15 && AS->GetBonusMaxSpeed() == 10 ? TEXT("PASS") : TEXT("FAIL"), AS->GetCoins(), AS->GetBonusMaxSpeed());
			VS->AddCoins(11); // Intentional tie to exercise joint winners at round end.
			ACXPickupSpawner* Spawner = Car->GetWorld()->SpawnActor<ACXPickupSpawner>(ACXPickupSpawner::StaticClass(), Car->GetActorLocation() + FVector(0, 3000, 0), FRotator::ZeroRotator, Params);
			ACXPickup* First = Spawner ? Spawner->GetActivePickup() : nullptr;
			TWeakObjectPtr<ACXPickupSpawner> WeakSpawner(Spawner);
			TWeakObjectPtr<ACXPickup> WeakFirst(First);
			if (First) First->Destroy();
			FTimerHandle RespawnCheck;
			Car->GetWorldTimerManager().SetTimer(RespawnCheck, [WeakSpawner, WeakFirst]()
			{
				const bool bRespawned = WeakSpawner.IsValid() && IsValid(WeakSpawner->GetActivePickup()) && WeakSpawner->GetActivePickup() != WeakFirst.Get() && WeakSpawner->GetActivePickup()->GetKind() == ECXPickupKind::Coin;
				UE_LOG(LogTemp, Display, TEXT("CX_GAMEPLAY_RESPAWN %s"), bRespawned ? TEXT("PASS") : TEXT("FAIL"));
			}, 2.3f, false);
		}, 1.0f, false);
	}, 7.0f, false);
	FTimerHandle FinishCheck;
	Mode->GetWorldTimerManager().SetTimer(FinishCheck, [WeakMode]()
	{
		if (!WeakMode.IsValid()) return;
		UWorld* World = WeakMode->GetWorld();
		bool bBlocked = World->GetGameState<ACXGameState>()->GetPhase() == ECXMatchPhase::Finished;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (ACXPawn* Car = Cast<ACXPawn>(It->Get()->GetPawn()))
			{
				const ACXPlayerState* PS = Car->GetPlayerState<ACXPlayerState>();
				const int32 Before = PS->GetCoins();
				bBlocked &= !Car->CanFire();
				Car->Fire();
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				ACXCoinPickup* Coin = World->SpawnActor<ACXCoinPickup>(ACXCoinPickup::StaticClass(), Car->GetActorLocation(), FRotator::ZeroRotator, Params);
				bBlocked &= Coin && !Coin->TryCollect(Car) && PS->GetCoins() == Before;
				if (Coin) Coin->Destroy();
			}
		}
		UE_LOG(LogTemp, Display, TEXT("CX_GAMEPLAY_FINISHED_GUARDS %s"), bBlocked ? TEXT("PASS") : TEXT("FAIL"));
	}, 29.0f, false);
#endif
}

void ScheduleCXGameplayClientTest(ACXPlayerController* Controller)
{
#if !UE_BUILD_SHIPPING
	if (!FParse::Param(FCommandLine::Get(), TEXT("CXGameplayTest")) || Controller->HasAuthority() || !Controller->IsLocalController()) return;
	TWeakObjectPtr<ACXPlayerController> WeakPC(Controller);
	FTimerHandle Check;
	Controller->GetWorldTimerManager().SetTimer(Check, [WeakPC]()
	{
		ACXPlayerController* PC = WeakPC.Get();
		if (!PC) return;
		ACXGameState* GS = PC->GetWorld()->GetGameState<ACXGameState>();
		ACXPlayerState* Own = PC->GetPlayerState<ACXPlayerState>();
		bool bValid = GS && Own && GS->GetPhase() == ECXMatchPhase::Finished && GS->GetWinners().Num() == 2 && GS->GetWinners().Contains(Own) && Own->GetCoins() == 15;
		int32 Items = 0, Peers = 0;
		for (TActorIterator<ACXPickup> It(PC->GetWorld()); It; ++It) ++Items;
		if (GS) for (APlayerState* Entry : GS->PlayerArray)
		{
			if (const ACXPlayerState* Peer = Cast<ACXPlayerState>(Entry); Peer && Peer != Own)
			{
				++Peers;
				bValid &= Peer->GetCoins() == 0 && Peer->GetBonusMaxSpeed() == 0;
			}
		}
		bValid &= Items == 22 && Peers == 1; // 21 map items + one test spawner, no client duplicates.
		UE_LOG(LogTemp, Display, TEXT("CX_GAMEPLAY_CLIENT %s items=%d peers=%d winners=%d coins=%d"), bValid ? TEXT("PASS") : TEXT("FAIL"), Items, Peers, GS ? GS->GetWinners().Num() : -1, Own ? Own->GetCoins() : -1);
	}, 30.0f, false);
#endif
}
