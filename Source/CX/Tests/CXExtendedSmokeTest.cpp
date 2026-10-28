#include "CXExtendedSmokeTest.h"
#include "../CXGameMode.h"
#include "../CXGameState.h"
#include "../CXPlayerState.h"
#include "../CXPlayerController.h"
#include "../CXPawn.h"
#include "../CXPickup.h"
#include "../CXGameInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

void ScheduleCXExtendedServerTest(ACXGameMode* Mode)
{
#if !UE_BUILD_SHIPPING
	if (!FParse::Param(FCommandLine::Get(), TEXT("CXExtendedTest"))) return;
	TWeakObjectPtr<ACXGameMode> WeakMode(Mode);
	FTimerHandle Start;
	Mode->GetWorldTimerManager().SetTimer(Start, [WeakMode]()
	{
		if (!WeakMode.IsValid()) return;
		ACXGameState* GS = WeakMode->GetGameState<ACXGameState>();
		APlayerController* Leader = GS && GS->GetLobbyLeader() ? Cast<APlayerController>(GS->GetLobbyLeader()->GetOwner()) : nullptr;
		UE_LOG(LogTemp, Display, TEXT("CX_EXT_UNREADY %s"), Leader && !WeakMode->TryStartMatch(Leader) ? TEXT("PASS") : TEXT("FAIL"));
	}, 1.0f, false);
	FTimerHandle Ready;
	Mode->GetWorldTimerManager().SetTimer(Ready, [WeakMode]()
	{
		if (!WeakMode.IsValid()) return;
		ACXGameState* GS = WeakMode->GetGameState<ACXGameState>();
		ACXPlayerController* Leader = Cast<ACXPlayerController>(GS->GetLobbyLeader()->GetOwner());
		APlayerController* Other = nullptr;
		for (FConstPlayerControllerIterator It = WeakMode->GetWorld()->GetPlayerControllerIterator(); It; ++It) if (It->Get() != Leader) Other = It->Get();
		const bool bPermission = Other && !WeakMode->TryStartMatch(Other);
		Leader->GetPlayerState<ACXPlayerState>()->RestoreEconomy(50, 0);
		Leader->ServerBuySpeed();
		const bool bOutsideBlocked = Leader->GetPlayerState<ACXPlayerState>()->GetCoins() == 50;
		const bool bStarted = WeakMode->TryStartMatch(Leader);
		UE_LOG(LogTemp, Display, TEXT("CX_EXT_READY_START %s permission=%d shop_guard=%d"), bStarted ? TEXT("PASS") : TEXT("FAIL"), bPermission, bOutsideBlocked);
		TWeakObjectPtr<ACXPlayerController> WeakLeader(Leader);
		FTimerHandle Shop;
		WeakMode->GetWorldTimerManager().SetTimer(Shop, [WeakLeader]()
		{
			if (!WeakLeader.IsValid()) return;
			ACXPlayerState* PS = WeakLeader->GetPlayerState<ACXPlayerState>();
			ACXPawn* Car = Cast<ACXPawn>(WeakLeader->GetPawn());
			float Before = -1, After = -1;
			if (Car) { Car->GetMesh()->SetPhysicsLinearVelocity(FVector(6000, 0, 0)); Car->Tick(0.016f); Before = Car->GetMesh()->GetPhysicsLinearVelocity().Size2D() * 0.036f; }
			WeakLeader->ServerBuySpeed();
			WeakLeader->ServerBuySpeed();
			if (Car) { Car->GetMesh()->SetPhysicsLinearVelocity(FVector(6000, 0, 0)); Car->Tick(0.016f); After = Car->GetMesh()->GetPhysicsLinearVelocity().Size2D() * 0.036f; Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector); }
			const bool bShop = PS && PS->GetCoins() == 30 && PS->GetBonusMaxSpeed() == 10;
			UE_LOG(LogTemp, Display, TEXT("CX_EXT_SHOP %s"), bShop ? TEXT("PASS") : TEXT("FAIL"));
			UE_LOG(LogTemp, Display, TEXT("CX_EXT_SPEED_LIMIT %s before=%.1f after=%.1f"), FMath::IsNearlyEqual(Before, 120.0f, 0.5f) && FMath::IsNearlyEqual(After, 130.0f, 0.5f) ? TEXT("PASS") : TEXT("FAIL"), Before, After);
		}, 3.0f, false);
	}, 5.0f, false);
	FTimerHandle Restart;
	Mode->GetWorldTimerManager().SetTimer(Restart, [WeakMode]()
	{
		if (!WeakMode.IsValid()) return;
		ACXGameState* GS = WeakMode->GetGameState<ACXGameState>();
		bool bValid = GS && GS->IsLobbyOpen() && GS->GetPhase() == ECXMatchPhase::Waiting && GS->GetWinners().IsEmpty();
		int32 Items = 0;
		for (TActorIterator<ACXPickup> It(WeakMode->GetWorld()); It; ++It) ++Items;
		bValid &= Items == 21;
		ACXPlayerController* Leader = nullptr;
		for (FConstPlayerControllerIterator It = WeakMode->GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (ACXPlayerController* PC = Cast<ACXPlayerController>(It->Get()))
			{
				bValid &= !PC->GetPlayerState<ACXPlayerState>()->IsReady();
				PC->ServerToggleReady();
				if (PC->PlayerState == GS->GetLobbyLeader()) Leader = PC;
			}
		}
		bValid &= Leader && Leader->GetPlayerState<ACXPlayerState>()->GetCoins() == 30 && WeakMode->TryStartMatch(Leader);
		UE_LOG(LogTemp, Display, TEXT("CX_EXT_REMATCH %s items=%d"), bValid ? TEXT("PASS") : TEXT("FAIL"), Items);
	}, 18.0f, false);
#endif
}

void ScheduleCXExtendedClientTest(ACXPlayerController* PC)
{
#if !UE_BUILD_SHIPPING
	if (!FParse::Param(FCommandLine::Get(), TEXT("CXExtendedTest")) || PC->HasAuthority() || !PC->IsLocalController()) return;
	TWeakObjectPtr<ACXPlayerController> WeakPC(PC);
	FTimerHandle Ready;
	PC->GetWorldTimerManager().SetTimer(Ready, [WeakPC]() { if (WeakPC.IsValid()) WeakPC->ServerToggleReady(); }, 3.0f, false);
	FTimerHandle Verify;
	PC->GetWorldTimerManager().SetTimer(Verify, [WeakPC]()
	{
		if (!WeakPC.IsValid()) return;
		const ACXGameState* GS = WeakPC->GetWorld()->GetGameState<ACXGameState>();
		const ACXPlayerState* Own = WeakPC->GetPlayerState<ACXPlayerState>();
		bool bValid = GS && Own && !GS->IsLobbyOpen() && GS->GetPhase() == ECXMatchPhase::Playing;
		if (GS) for (APlayerState* Entry : GS->PlayerArray) if (const ACXPlayerState* Peer = Cast<ACXPlayerState>(Entry); Peer && Peer != Own) bValid &= Peer->GetCoins() == 0 && Peer->GetBonusMaxSpeed() == 0;
		UE_LOG(LogTemp, Display, TEXT("CX_EXT_CLIENT %s phase=%d"), bValid ? TEXT("PASS") : TEXT("FAIL"), GS ? static_cast<int32>(GS->GetPhase()) : -1);
	}, 22.0f, false);
#endif
}

void ScheduleCXSaveProbe(ACXPlayerController* PC)
{
#if !UE_BUILD_SHIPPING
	FString Stage;
	if (!PC || !PC->HasAuthority() || !FParse::Value(FCommandLine::Get(), TEXT("CXSaveTest="), Stage)) return;
	TWeakObjectPtr<ACXPlayerController> WeakPC(PC);
	FTimerHandle Probe;
	PC->GetWorldTimerManager().SetTimer(Probe, [WeakPC, Stage]()
	{
		if (!WeakPC.IsValid()) return;
		ACXPlayerState* PS = WeakPC->GetPlayerState<ACXPlayerState>();
		UCXGameInstance* GI = WeakPC->GetGameInstance<UCXGameInstance>();
		if (!PS || !GI) return;
		if (Stage == TEXT("Seed"))
		{
			PS->RestoreEconomy(73, 20);
			const bool bSaved = GI->SaveProfiles(WeakPC->GetWorld());
			const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation");
			IFileManager::Get().MakeDirectory(*Directory, true);
			const bool bCredential = FFileHelper::SaveStringToFile(PS->ProfileCredential, *(Directory / TEXT("CXReconnectCredential.txt")));
			UE_LOG(LogTemp, Display, TEXT("CX_SAVE_SEED %s"), bSaved && bCredential ? TEXT("PASS") : TEXT("FAIL"));
		}
		else if (Stage == TEXT("Restore"))
		{
			UE_LOG(LogTemp, Display, TEXT("CX_SAVE_RECONNECT %s coins=%d bonus=%.1f"), PS->GetCoins() == 73 && PS->GetBonusMaxSpeed() == 20 ? TEXT("PASS") : TEXT("FAIL"), PS->GetCoins(), PS->GetBonusMaxSpeed());
		}
	}, 1.0f, false);
#endif
}
