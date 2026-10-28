#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "CXGameInstance.generated.h"

UCLASS()
class CX_API UCXGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	virtual void Init() override;
	virtual void Shutdown() override;
	void HostRoom();
	void QuickMatch();
	void JoinAddress(const FString& Address);
	void LeaveRoom();
	void RememberCredential(const FString& Credential);
	FString RestoreProfile(class ACXPlayerState* State, const FString& RequestedCredential);
	bool SaveProfiles(UWorld* World);
	const FString& GetStatus() const { return Status; }
	static bool NormalizeAddress(const FString& Input, FString& Output);
	static bool IsPersistenceEnabled();
private:
	FString CredentialFor(const FString& Endpoint);
	void ConnectTo(const FString& Endpoint);
	void OnCreated(FName Name, bool bSuccess);
	void OnDestroyed(FName Name, bool bSuccess);
	void OnFound(bool bSuccess);
	void OnJoined(FName Name, EOnJoinSessionCompleteResult::Type Result);
	void OnNetworkFailure(UWorld* World, class UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error);
	void OnTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error);
	void LoadProfiles();
	IOnlineSessionPtr Sessions;
	TSharedPtr<class FOnlineSessionSearch> Search;
	FDelegateHandle CreateHandle, DestroyHandle, FindHandle, JoinHandle, NetworkHandle, TravelHandle;
	UPROPERTY() TObjectPtr<class UCXSaveGame> ServerSave;
	UPROPERTY() TObjectPtr<class UCXSaveGame> ClientSave;
	FString PendingEndpoint = TEXT("LOCAL_HOST");
	FString Status;
	bool bBusy = false;
	bool bHostAfterDestroy = false;
	bool bServerSaveDirty = false;
};
