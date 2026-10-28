#include "CXNetworkSmokeTest.h"
#include "../CXPlayerController.h"
#include "../CXPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

void ScheduleCXNetworkSmokeTest(ACXPlayerController* Controller)
{
#if !UE_BUILD_SHIPPING
	if (!FParse::Param(FCommandLine::Get(), TEXT("CXNetTest"))) return;
	TWeakObjectPtr<ACXPlayerController> WeakController(Controller);
	FTimerHandle Handle;
	if (Controller->HasAuthority())
	{
		Controller->GetWorldTimerManager().SetTimer(Handle, [WeakController]()
		{
			if (ACXPlayerController* PC = WeakController.Get())
			{
				if (ACXPlayerState* PS = PC->GetPlayerState<ACXPlayerState>())
				{
					const bool bValid = PS->AddCoins(100) && PS->IncreaseBonusMaxSpeed(10);
					UE_LOG(LogTemp, Display, TEXT("CX_NET_SERVER_GRANT %s"), bValid ? TEXT("PASS") : TEXT("FAIL"));
				}
			}
		}, 1.0f, false);
	}
	else if (Controller->IsLocalController())
	{
		Controller->GetWorldTimerManager().SetTimer(Handle, [WeakController]()
		{
			if (ACXPlayerController* PC = WeakController.Get())
			{
				int32 Index = 0;
				FParse::Value(FCommandLine::Get(), TEXT("CXTestIndex="), Index);
				PC->SendChatMessage(FString::Printf(TEXT("CX_NET_HELLO_%d"), Index));
			}
		}, 8.0f, false);
		FTimerHandle VerificationHandle;
		Controller->GetWorldTimerManager().SetTimer(VerificationHandle, [WeakController]()
		{
			ACXPlayerController* PC = WeakController.Get();
			if (!PC) return;
			ACXPlayerState* Own = PC->GetPlayerState<ACXPlayerState>();
			bool bPassed = Own && Own->GetCoins() == 100 && Own->GetBonusMaxSpeed() == 10.0f;
			if (Own)
			{
				bPassed &= !Own->AddCoins(1) && !Own->SpendCoins(1) && !Own->IncreaseBonusMaxSpeed(1);
			}
			int32 Peers = 0;
			if (const AGameStateBase* GS = PC->GetWorld()->GetGameState())
			{
				for (APlayerState* Entry : GS->PlayerArray)
				{
					if (const ACXPlayerState* Peer = Cast<ACXPlayerState>(Entry); Peer && Peer != Own)
					{
						++Peers;
						bPassed &= Peer->GetCoins() == 0 && Peer->GetBonusMaxSpeed() == 0.0f;
					}
				}
			}
			bPassed &= Peers >= 1;
			UE_LOG(LogTemp, Display, TEXT("CX_NET_OWNER_ONLY %s peers=%d coins=%d bonus=%.1f"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Peers, Own ? Own->GetCoins() : -1, Own ? Own->GetBonusMaxSpeed() : -1);
		}, 14.0f, false);
	}
#endif
}
