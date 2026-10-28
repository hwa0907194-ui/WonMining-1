#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "CXPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCXStatsChanged);

/** Private player economy. Only authoritative gameplay code can mutate it. */
UCLASS()
class CX_API ACXPlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	static constexpr int32 MaxCoins = 1000000000;
	static constexpr float MaxBonusSpeed = 1000.0f;

	UFUNCTION(BlueprintPure, Category="Mining") int32 GetCoins() const { return Coins; }
	/** Additional maximum speed, expressed in km/h. */
	UFUNCTION(BlueprintPure, Category="Mining") float GetBonusMaxSpeed() const { return BonusMaxSpeed; }
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mining") bool AddCoins(int32 Amount);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mining") bool SpendCoins(int32 Amount);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mining") bool IncreaseBonusMaxSpeed(float Amount);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mining") bool TransferCoinsTo(ACXPlayerState* Recipient, int32 Amount);
	bool BuySpeedBoost();
	bool RestoreEconomy(int32 SavedCoins, float SavedBonus);
	bool IsReady() const { return bReady; }
	void SetReady(bool bNewReady);
	// Server-only bearer credential; never replicated or displayed.
	FString ProfileCredential;
	UPROPERTY(BlueprintAssignable, Category="Mining") FCXStatsChanged OnStatsChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;
private:
	UPROPERTY(Replicated) bool bReady = false;
	UPROPERTY(ReplicatedUsing=OnRep_Stats) int32 Coins = 0;
	UPROPERTY(ReplicatedUsing=OnRep_Stats) float BonusMaxSpeed = 0.0f;
	UFUNCTION() void OnRep_Stats();
	void NotifyChanged();
};
