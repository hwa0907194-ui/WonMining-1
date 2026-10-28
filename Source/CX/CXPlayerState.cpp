#include "CXPlayerState.h"
#include "Net/UnrealNetwork.h"

bool ACXPlayerState::AddCoins(int32 Amount)
{
	if (!HasAuthority() || Amount <= 0 || Amount > MaxCoins - Coins) return false;
	Coins += Amount;
	NotifyChanged();
	return true;
}

bool ACXPlayerState::SpendCoins(int32 Amount)
{
	if (!HasAuthority() || Amount <= 0 || Amount > Coins) return false;
	Coins -= Amount;
	NotifyChanged();
	return true;
}

bool ACXPlayerState::IncreaseBonusMaxSpeed(float Amount)
{
	if (!HasAuthority() || !FMath::IsFinite(Amount) || Amount <= 0.0f || Amount > MaxBonusSpeed - BonusMaxSpeed) return false;
	BonusMaxSpeed += Amount;
	NotifyChanged();
	return true;
}

void ACXPlayerState::NotifyChanged()
{
	// Listen-server owners do not receive their own RepNotify callbacks.
	OnStatsChanged.Broadcast();
	ForceNetUpdate();
}

bool ACXPlayerState::BuySpeedBoost()
{
	if (!HasAuthority() || Coins < 20 || BonusMaxSpeed > MaxBonusSpeed - 10) return false;
	Coins -= 20;
	BonusMaxSpeed += 10;
	NotifyChanged();
	return true;
}

bool ACXPlayerState::RestoreEconomy(int32 SavedCoins, float SavedBonus)
{
	if (!HasAuthority() || SavedCoins < 0 || SavedCoins > MaxCoins || !FMath::IsFinite(SavedBonus) || SavedBonus < 0 || SavedBonus > MaxBonusSpeed) return false;
	Coins = SavedCoins;
	BonusMaxSpeed = SavedBonus;
	NotifyChanged();
	return true;
}

void ACXPlayerState::SetReady(bool bNewReady)
{
	if (HasAuthority() && bReady != bNewReady) { bReady = bNewReady; ForceNetUpdate(); }
}

bool ACXPlayerState::TransferCoinsTo(ACXPlayerState* Recipient, int32 Amount)
{
	if (!HasAuthority() || !IsValid(Recipient) || !Recipient->HasAuthority() || Recipient == this ||
		Recipient->GetWorld() != GetWorld() || Amount <= 0 || Coins < Amount || Amount > MaxCoins - Recipient->Coins) return false;
	// Validate both accounts before mutating either balance.
	Coins -= Amount;
	Recipient->Coins += Amount;
	NotifyChanged();
	Recipient->NotifyChanged();
	return true;
}

void ACXPlayerState::OnRep_Stats()
{
	OnStatsChanged.Broadcast();
}

void ACXPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACXPlayerState, bReady);
	DOREPLIFETIME_CONDITION(ACXPlayerState, Coins, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ACXPlayerState, BonusMaxSpeed, COND_OwnerOnly);
}

void ACXPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);
	if (HasAuthority())
	{
		if (ACXPlayerState* Target = Cast<ACXPlayerState>(PlayerState))
		{
			Target->Coins = Coins;
			Target->BonusMaxSpeed = BonusMaxSpeed;
			Target->NotifyChanged();
		}
	}
}
