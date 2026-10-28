// CXGameMode.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CXGameMode.generated.h"

class APlayerStart;

UCLASS(MinimalAPI)
class ACXGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACXGameMode();
	void BroadcastChat(class ACXPlayerController* Sender, const FString& Message);
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
	bool TryStartMatch(APlayerController* Requester);
	bool ReturnToLobby(APlayerController* Requester = nullptr);
	static TArray<APlayerState*> FindWinners(const TArray<APlayerState*>& Players);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UPROPERTY(EditDefaultsOnly, Category="Match", meta=(ClampMin=1)) int32 WaitingDuration = 10;
	UPROPERTY(EditDefaultsOnly, Category="Match", meta=(ClampMin=1)) int32 PlayingDuration = 120;
	UPROPERTY(EditDefaultsOnly, Category="Match", meta=(ClampMin=1)) int32 NextRoundDelay = 15;
	UPROPERTY(EditDefaultsOnly, Category="Match") bool bUseLobby = true;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

private:
	bool IsPlayerStartAvailable(const APlayerStart* PlayerStart) const;
	void TickMatchClock();
	bool bCountdownStarted = false;
	bool bGameplayTestScheduled = false;
	FTimerHandle MatchClock;
	FTimerHandle SaveTimer;
	TMap<TWeakObjectPtr<APlayerController>, FString> LoginCredentials;
	UFUNCTION() void QueueSave();
	void SaveNow();
	void StartCountdown();

private:
	UPROPERTY(EditDefaultsOnly)
	FVector VehicleSpawnCheckHalfExtent = FVector(250.0, 120.0, 80.0);

	UPROPERTY(EditDefaultsOnly)
	FVector VehicleSpawnCheckOffset = FVector(0.0, 0.0, 80.0);

};



