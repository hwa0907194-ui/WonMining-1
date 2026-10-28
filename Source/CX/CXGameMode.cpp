// CXGameMode.cpp

#include "CXGameMode.h"
#include "CXPlayerController.h"
#include "CXPlayerState.h"
#include "CXGameState.h"
#include "CXGameInstance.h"
#include "CXPawn.h"
#include "CXPickupSpawner.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Tests/CXGameplaySmokeTest.h"
#include "Tests/CXExtendedSmokeTest.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"

ACXGameMode::ACXGameMode()
{
	PlayerControllerClass = ACXPlayerController::StaticClass();
	PlayerStateClass = ACXPlayerState::StaticClass();
	GameStateClass = ACXGameState::StaticClass();
}

void ACXGameMode::BeginPlay()
{
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING
	// Short rounds only when the integration test explicitly opts in.
	if (FParse::Param(FCommandLine::Get(), TEXT("CXGameplayTest"))) { WaitingDuration = 5; PlayingDuration = 22; bUseLobby = false; }
	if (FParse::Param(FCommandLine::Get(), TEXT("CXNetTest"))) bUseLobby = false;
	if (FParse::Param(FCommandLine::Get(), TEXT("CXExtendedTest"))) { WaitingDuration = 2; PlayingDuration = 6; NextRoundDelay = 4; }
#endif
	if (ACXGameState* GS = GetGameState<ACXGameState>())
	{
		GS->SetLobby(bUseLobby, GS->GetLobbyLeader());
		GS->SetMatchData(ECXMatchPhase::Waiting, WaitingDuration);
	}
	if (!bUseLobby && GetNumPlayers() > 0) StartCountdown();
	// Listen-server PostLogin may run before BeginPlay. Preserve the countdown.
}

void ACXGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	ACXPlayerState* PS = NewPlayer->GetPlayerState<ACXPlayerState>();
	if (UCXGameInstance* GI = GetGameInstance<UCXGameInstance>(); GI && PS)
	{
		const FString Credential = GI->RestoreProfile(PS, LoginCredentials.FindRef(NewPlayer));
		if (ACXPlayerController* PC = Cast<ACXPlayerController>(NewPlayer)) PC->ClientRememberProfile(Credential);
		PS->OnStatsChanged.AddDynamic(this, &ACXGameMode::QueueSave);
		QueueSave();
	}
	LoginCredentials.Remove(NewPlayer);
	ScheduleCXSaveProbe(Cast<ACXPlayerController>(NewPlayer));
	if (ACXGameState* GS = GetGameState<ACXGameState>(); GS && !GS->GetLobbyLeader()) GS->SetLobby(bUseLobby, PS);
	if (!bGameplayTestScheduled && GetNumPlayers() >= 2)
	{
		bGameplayTestScheduled = true;
		ScheduleCXGameplayServerTest(this);
		ScheduleCXExtendedServerTest(this);
	}
	if (!bUseLobby) StartCountdown();
}

void ACXGameMode::StartCountdown()
{
	if (bCountdownStarted) return;
	bCountdownStarted = true;
	GetWorldTimerManager().SetTimer(MatchClock, this, &ACXGameMode::TickMatchClock, 1.0f, true);
}

void ACXGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty()) return;
	if (GetNumPlayers() >= 3) { ErrorMessage = TEXT("Room is full (3 players)."); return; }
	const FString Token = UGameplayStatics::ParseOption(Options, TEXT("CXProfile"));
	if (!Token.IsEmpty())
	{
		FGuid Guid;
		if (!FGuid::ParseExact(Token, EGuidFormats::Digits, Guid)) { ErrorMessage = TEXT("Invalid reconnect credential."); return; }
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			if (const ACXPlayerState* PS = It->Get()->GetPlayerState<ACXPlayerState>(); PS && PS->ProfileCredential == Token) { ErrorMessage = TEXT("Profile is already connected."); return; }
	}
}

FString ACXGameMode::InitNewPlayer(APlayerController* PC, const FUniqueNetIdRepl& Id, const FString& Options, const FString& Portal)
{
	const FString Error = Super::InitNewPlayer(PC, Id, Options, Portal);
	if (Error.IsEmpty()) LoginCredentials.Add(PC, UGameplayStatics::ParseOption(Options, TEXT("CXProfile")));
	return Error;
}

bool ACXGameMode::TryStartMatch(APlayerController* Requester)
{
	ACXGameState* GS = GetGameState<ACXGameState>();
	if (!GS || !Requester || !GS->IsLobbyOpen() || GS->GetLobbyLeader() != Requester->PlayerState) return false;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const ACXPlayerState* PS = It->Get()->GetPlayerState<ACXPlayerState>();
		if (!PS || !PS->IsReady()) return false;
	}
	GS->SetLobby(false, GS->GetLobbyLeader());
	StartCountdown();
	return true;
}

void ACXGameMode::QueueSave()
{
	if (!GetWorldTimerManager().IsTimerActive(SaveTimer)) GetWorldTimerManager().SetTimer(SaveTimer, this, &ACXGameMode::SaveNow, 0.5f, false);
}

void ACXGameMode::SaveNow()
{
	if (UCXGameInstance* GI = GetGameInstance<UCXGameInstance>()) GI->SaveProfiles(GetWorld());
}

void ACXGameMode::Logout(AController* Exiting)
{
	SaveNow();
	if (ACXPlayerState* PS = Exiting->GetPlayerState<ACXPlayerState>()) PS->OnStatsChanged.RemoveDynamic(this, &ACXGameMode::QueueSave);
	ACXGameState* GS = GetGameState<ACXGameState>();
	if (GS && GS->GetLobbyLeader() == Exiting->PlayerState)
	{
		APlayerState* Next = nullptr;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It) if (It->Get() != Exiting) { Next = It->Get()->PlayerState; break; }
		GS->SetLobby(GS->IsLobbyOpen(), Next);
	}
	Super::Logout(Exiting);
}

bool ACXGameMode::ReturnToLobby(APlayerController* Requester)
{
	ACXGameState* GS = GetGameState<ACXGameState>();
	if (!GS || GS->GetPhase() != ECXMatchPhase::Finished || (Requester && GS->GetLobbyLeader() != Requester->PlayerState)) return false;
	SaveNow();
	GetWorldTimerManager().ClearTimer(MatchClock);
	bCountdownStarted = false;
	GS->SetWinners({});
	GS->SetLobby(true, GS->GetLobbyLeader());
	GS->SetMatchData(ECXMatchPhase::Waiting, WaitingDuration);
	TArray<APlayerStart*> Starts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It) Starts.Add(*It);
	int32 Index = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ACXPlayerState* PS = It->Get()->GetPlayerState<ACXPlayerState>()) PS->SetReady(false);
		if (ACXPawn* Car = Cast<ACXPawn>(It->Get()->GetPawn()); Car && Starts.Num()) Car->MulticastResetForRound(Starts[Index++ % Starts.Num()]->GetActorTransform());
	}
	// Reset pickups and their respawn timers so every new round starts complete.
	for (TActorIterator<class ACXPickupSpawner> It(GetWorld()); It; ++It) It->ResetForRound();
	return true;
}

TArray<APlayerState*> ACXGameMode::FindWinners(const TArray<APlayerState*>& Players)
{
	TArray<APlayerState*> Result;
	int32 BestCoins = -1;
	for (APlayerState* Entry : Players)
	{
		const ACXPlayerState* Stats = Cast<ACXPlayerState>(Entry);
		if (!IsValid(Stats) || Stats->IsInactive()) continue;
		const int32 Coins = Stats->GetCoins();
		if (Coins > BestCoins) { BestCoins = Coins; Result.Reset(); }
		if (Coins == BestCoins) Result.Add(Entry);
	}
	return Result;
}

void ACXGameMode::TickMatchClock()
{
	ACXGameState* GS = GetGameState<ACXGameState>();
	if (!GS) return;
	if (GS->GetPhase() == ECXMatchPhase::Finished)
	{
		if (GS->GetRemainingSeconds() <= 1) ReturnToLobby();
		else GS->SetMatchData(ECXMatchPhase::Finished, GS->GetRemainingSeconds() - 1);
		return;
	}
	const int32 Seconds = GS->GetRemainingSeconds() - 1;
	if (Seconds > 0) { GS->SetMatchData(GS->GetPhase(), Seconds); return; }
	if (GS->GetPhase() == ECXMatchPhase::Waiting)
	{
		GS->SetMatchData(ECXMatchPhase::Playing, PlayingDuration);
	}
	else
	{
		TArray<APlayerState*> Players;
		for (APlayerState* Entry : GS->PlayerArray) Players.Add(Entry);
		GS->SetWinners(FindWinners(Players));
		GS->SetMatchData(ECXMatchPhase::Finished, NextRoundDelay);
		SaveNow();
	}
}

void ACXGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(MatchClock);
	GetWorldTimerManager().ClearTimer(SaveTimer);
	SaveNow();
	Super::EndPlay(Reason);
}

void ACXGameMode::BroadcastChat(ACXPlayerController* Sender, const FString& Message)
{
	if (!HasAuthority() || !IsValid(Sender)) return;
	const APlayerState* State = Sender->GetPlayerState<APlayerState>();
	const FString Name = State ? State->GetPlayerName().Left(32) : TEXT("Player");
	// PlayerControllers are only relevant to their owner. Send a client RPC to
	// each controller instead of multicasting on a single player's controller.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ACXPlayerController* Recipient = Cast<ACXPlayerController>(It->Get()))
			Recipient->ClientReceiveChat(Name, Message);
	}
}

AActor* ACXGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	TArray<APlayerStart*> AvailablePlayerStarts;

	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		APlayerStart* PlayerStart = *It;

		if (IsPlayerStartAvailable(PlayerStart))
		{
			AvailablePlayerStarts.Add(PlayerStart);
		}
	}

	if (AvailablePlayerStarts.IsEmpty()) return Super::ChoosePlayerStart_Implementation(Player);

	const int32 RandomIndex = FMath::RandHelper(AvailablePlayerStarts.Num());
	return Cast<AActor>(AvailablePlayerStarts[RandomIndex]);
}

bool ACXGameMode::IsPlayerStartAvailable(const APlayerStart* PlayerStart) const
{
	if (IsValid(PlayerStart) == false || IsValid(GetWorld()) == false)
	{
		return false;
	}

	const FTransform PlayerStartTransform = PlayerStart->GetActorTransform();
	const FVector CheckLocation = PlayerStartTransform.TransformPosition(VehicleSpawnCheckOffset);
	const FQuat CheckRotation = PlayerStartTransform.GetRotation();
	const FCollisionShape CheckShape = FCollisionShape::MakeBox(VehicleSpawnCheckHalfExtent);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Vehicle);

	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VehiclePlayerStartCheck), false);

	const bool bVehicleExists =
		GetWorld()->OverlapAnyTestByObjectType(
			CheckLocation,
			CheckRotation,
			ObjectQueryParams,
			CheckShape,
			QueryParams
		);

	return !bVehicleExists;
}
