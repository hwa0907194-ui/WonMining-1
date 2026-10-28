#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../CXPlayerState.h"
#include "../CXGameMode.h"
#include "Engine/World.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXPlayerStateTest, "CX.PlayerState.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCXPlayerStateTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ACXPlayerState* Stats = World->SpawnActor<ACXPlayerState>();
	if (!TestNotNull(TEXT("Spawn player state"), Stats)) { World->DestroyWorld(false); return false; }
	TestEqual(TEXT("Initial coins"), Stats->GetCoins(), 0);
	TestFalse(TEXT("Reject zero"), Stats->AddCoins(0));
	TestFalse(TEXT("Reject negative grant"), Stats->AddCoins(-1));
	TestFalse(TEXT("Reject insufficient funds"), Stats->SpendCoins(1));
	TestTrue(TEXT("Grant"), Stats->AddCoins(100));
	TestTrue(TEXT("Spend"), Stats->SpendCoins(40));
	TestEqual(TEXT("Balance"), Stats->GetCoins(), 60);
	TestFalse(TEXT("Reject negative spend"), Stats->SpendCoins(-1));
	TestFalse(TEXT("Reject integer overflow"), Stats->AddCoins(MAX_int32));
	TestTrue(TEXT("Reach exact coin limit"), Stats->AddCoins(ACXPlayerState::MaxCoins - 60));
	TestFalse(TEXT("Reject over coin limit"), Stats->AddCoins(1));
	TestTrue(TEXT("Spend entire balance"), Stats->SpendCoins(ACXPlayerState::MaxCoins));
	TestTrue(TEXT("Add bonus"), Stats->IncreaseBonusMaxSpeed(10));
	TestFalse(TEXT("Reject negative bonus"), Stats->IncreaseBonusMaxSpeed(-1));
	TestFalse(TEXT("Reject NaN"), Stats->IncreaseBonusMaxSpeed(std::numeric_limits<float>::quiet_NaN()));
	TestFalse(TEXT("Reject infinity"), Stats->IncreaseBonusMaxSpeed(std::numeric_limits<float>::infinity()));
	TestTrue(TEXT("Reach exact speed limit"), Stats->IncreaseBonusMaxSpeed(990));
	TestFalse(TEXT("Reject over speed limit"), Stats->IncreaseBonusMaxSpeed(1));
	ACXPlayerState* Other = World->SpawnActor<ACXPlayerState>();
	TestTrue(TEXT("Victim receives five"), Other->AddCoins(5));
	TestTrue(TEXT("Atomic theft of exactly five"), Other->TransferCoinsTo(Stats, 5));
	TestEqual(TEXT("Attacker receives five"), Stats->GetCoins(), 5);
	TestEqual(TEXT("Victim loses five"), Other->GetCoins(), 0);
	TestFalse(TEXT("Insufficient victim balance"), Other->TransferCoinsTo(Stats, 5));
	TestFalse(TEXT("Cannot transfer to self"), Stats->TransferCoinsTo(Stats, 5));
	TestFalse(TEXT("Cannot transfer negative amount"), Stats->TransferCoinsTo(Other, -5));
	TestTrue(TEXT("Recipient at limit"), Stats->AddCoins(ACXPlayerState::MaxCoins - 5));
	Other->AddCoins(5);
	TestFalse(TEXT("Recipient overflow rejects entire transfer"), Other->TransferCoinsTo(Stats, 5));
	TestEqual(TEXT("Rejected transfer keeps victim balance"), Other->GetCoins(), 5);
	TArray<APlayerState*> Players = {Stats, Other};
	TestEqual(TEXT("Single winner"), ACXGameMode::FindWinners(Players).Num(), 1);
	Stats->SpendCoins(ACXPlayerState::MaxCoins - 5);
	TestEqual(TEXT("Joint winners"), ACXGameMode::FindWinners(Players).Num(), 2);
	Other->SetIsInactive(true);
	TestEqual(TEXT("Inactive players excluded"), ACXGameMode::FindWinners(Players).Num(), 1);
	TestEqual(TEXT("Empty player list"), ACXGameMode::FindWinners({}).Num(), 0);
	World->DestroyWorld(false);
	return true;
}
#endif
