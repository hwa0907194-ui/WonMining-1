#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../CXPlayerState.h"
#include "../CXGameInstance.h"
#include "../CXSaveGame.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXExtendedTest, "CX.Extended.ShopPersistenceAddress", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXExtendedTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ACXPlayerState* PS = World->SpawnActor<ACXPlayerState>();
	if (!PS) { World->DestroyWorld(false); return false; }
	TestTrue(TEXT("Restore valid profile"), PS->RestoreEconomy(100, 10));
	TestTrue(TEXT("Buy a speed boost"), PS->BuySpeedBoost());
	TestEqual(TEXT("Atomic shop debit"), PS->GetCoins(), 80);
	TestEqual(TEXT("Atomic shop upgrade"), PS->GetBonusMaxSpeed(), 20.0f);
	TestFalse(TEXT("Reject corrupted negative save"), PS->RestoreEconomy(-1, 0));
	TestFalse(TEXT("Reject corrupted NaN save"), PS->RestoreEconomy(0, std::numeric_limits<float>::quiet_NaN()));
	TestEqual(TEXT("Invalid restore keeps balance"), PS->GetCoins(), 80);
	PS->RestoreEconomy(19, 0);
	TestFalse(TEXT("Insufficient shop balance"), PS->BuySpeedBoost());
	TestEqual(TEXT("Failed purchase keeps balance"), PS->GetCoins(), 19);
	PS->RestoreEconomy(100, ACXPlayerState::MaxBonusSpeed);
	TestFalse(TEXT("At speed cap purchase rejected"), PS->BuySpeedBoost());
	TestEqual(TEXT("Cap rejection keeps balance"), PS->GetCoins(), 100);
	UCXSaveGame* Save = NewObject<UCXSaveGame>();
	FCXSavedProfile& Record = Save->Profiles.Add(TEXT("test-profile")); Record.Coins = 80; Record.BonusSpeed = 20;
	const FString SlotName = TEXT("CX_Automation_ProfileRoundTrip");
	TestTrue(TEXT("Write save file"), UGameplayStatics::SaveGameToSlot(Save, SlotName, 0));
	const UCXSaveGame* Loaded = Cast<UCXSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	TestNotNull(TEXT("Read save file"), Loaded);
	if (Loaded)
	{
		const FCXSavedProfile* Restored = Loaded->Profiles.Find(TEXT("test-profile"));
		TestNotNull(TEXT("Profile survives serialization"), Restored);
		if (Restored) { TestEqual(TEXT("Saved coins"), Restored->Coins, 80); TestEqual(TEXT("Saved speed"), Restored->BonusSpeed, 20.0f); }
	}
	UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	FString Address;
	TestTrue(TEXT("Valid IPv4 with port"), UCXGameInstance::NormalizeAddress(TEXT(" 127.0.0.1:7777 "), Address));
	TestEqual(TEXT("Canonical endpoint"), Address, FString(TEXT("127.0.0.1:7777")));
	TestTrue(TEXT("Default port"), UCXGameInstance::NormalizeAddress(TEXT("192.168.0.2"), Address));
	TestFalse(TEXT("Reject URL option injection"), UCXGameInstance::NormalizeAddress(TEXT("127.0.0.1:7777?listen"), Address));
	TestFalse(TEXT("Reject invalid port"), UCXGameInstance::NormalizeAddress(TEXT("127.0.0.1:65536"), Address));
	World->DestroyWorld(false);
	return true;
}
#endif
