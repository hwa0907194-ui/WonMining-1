#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "CXGameState.generated.h"

UENUM(BlueprintType)
enum class ECXMatchPhase : uint8 { Waiting, Playing, Finished };

UCLASS()
class CX_API ACXGameState : public AGameStateBase
{
	GENERATED_BODY()
public:
	ACXGameState();
	UFUNCTION(BlueprintPure) ECXMatchPhase GetPhase() const { return Phase; }
	UFUNCTION(BlueprintPure) int32 GetRemainingSeconds() const { return RemainingSeconds; }
	const TArray<TObjectPtr<APlayerState>>& GetWinners() const { return Winners; }
	bool IsLobbyOpen() const { return bLobbyOpen; }
	APlayerState* GetLobbyLeader() const { return LobbyLeader; }
	void SetLobby(bool bOpen, APlayerState* Leader);
	void SetMatchData(ECXMatchPhase NewPhase, int32 Seconds);
	void SetWinners(const TArray<APlayerState*>& NewWinners);
	UFUNCTION(NetMulticast, Reliable) void MulticastAttack(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit);
	UFUNCTION(NetMulticast, Reliable) void MulticastPickupEffect(FVector_NetQuantize Location);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
	UPROPERTY(Replicated) bool bLobbyOpen = true;
	UPROPERTY(Replicated) TObjectPtr<APlayerState> LobbyLeader;
	UPROPERTY(ReplicatedUsing=OnRep_Match, BlueprintReadOnly, meta=(AllowPrivateAccess=true)) ECXMatchPhase Phase = ECXMatchPhase::Waiting;
	UPROPERTY(ReplicatedUsing=OnRep_Match, BlueprintReadOnly, meta=(AllowPrivateAccess=true)) int32 RemainingSeconds = 0;
	UPROPERTY(ReplicatedUsing=OnRep_Match, BlueprintReadOnly, meta=(AllowPrivateAccess=true)) TArray<TObjectPtr<APlayerState>> Winners;
	UPROPERTY() TObjectPtr<class UParticleSystem> HitParticles;
	UPROPERTY() TObjectPtr<class UParticleSystem> PickupParticles;
	UPROPERTY() TObjectPtr<class USoundBase> PickupSound;
	UFUNCTION() void OnRep_Match();
};
