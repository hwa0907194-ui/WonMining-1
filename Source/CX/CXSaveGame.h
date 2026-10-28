#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CXSaveGame.generated.h"

USTRUCT()
struct FCXSavedProfile
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) int32 Coins = 0;
	UPROPERTY(SaveGame) float BonusSpeed = 0;
};

UCLASS()
class CX_API UCXSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) int32 Version = 1;
	UPROPERTY(SaveGame) TMap<FString, FCXSavedProfile> Profiles;
	// Client file stores only opaque reconnect credentials, never economy values.
	UPROPERTY(SaveGame) TMap<FString, FString> Credentials;
};
