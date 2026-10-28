#include "CXGameInstance.h"
#include "CXSaveGame.h"
#include "CXPlayerState.h"
#include "CXPlayerController.h"
#include "CXGameState.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace { const FName CXMap(TEXT("/Game/VehicleTemplate/Maps/VehicleAdvExampleMap")); }

bool UCXGameInstance::IsPersistenceEnabled()
{
	return !FParse::Param(FCommandLine::Get(), TEXT("CXNoSave")) && !FParse::Param(FCommandLine::Get(), TEXT("CXNetTest")) && !FParse::Param(FCommandLine::Get(), TEXT("CXGameplayTest")) && !FParse::Param(FCommandLine::Get(), TEXT("CXExtendedTest"));
}

void UCXGameInstance::Init()
{
	Super::Init();
	LoadProfiles();
	if (IOnlineSubsystem* Online = IOnlineSubsystem::Get()) Sessions = Online->GetSessionInterface();
	if (Sessions.IsValid())
	{
		CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UCXGameInstance::OnCreated));
		DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &UCXGameInstance::OnDestroyed));
		FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UCXGameInstance::OnFound));
		JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UCXGameInstance::OnJoined));
	}
	if (GEngine)
	{
		NetworkHandle = GEngine->OnNetworkFailure().AddUObject(this, &UCXGameInstance::OnNetworkFailure);
		TravelHandle = GEngine->OnTravelFailure().AddUObject(this, &UCXGameInstance::OnTravelFailure);
	}
}

void UCXGameInstance::LoadProfiles()
{
	if (IsPersistenceEnabled())
	{
		ServerSave = Cast<UCXSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("CX_ServerProfiles_v1"), 0));
		ClientSave = Cast<UCXSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("CX_ClientCredentials_v1"), 0));
	}
	if (!ServerSave || ServerSave->Version != 1) ServerSave = NewObject<UCXSaveGame>(this);
	if (!ClientSave || ClientSave->Version != 1) ClientSave = NewObject<UCXSaveGame>(this);
}

void UCXGameInstance::Shutdown()
{
	if (GetWorld() && GetWorld()->GetNetMode() != NM_Client) SaveProfiles(GetWorld());
	if (GEngine) { GEngine->OnNetworkFailure().Remove(NetworkHandle); GEngine->OnTravelFailure().Remove(TravelHandle); }
	if (Sessions.IsValid())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	}
	Super::Shutdown();
}

FString UCXGameInstance::CredentialFor(const FString& Endpoint)
{
	return ClientSave ? ClientSave->Credentials.FindRef(Endpoint) : FString();
}

void UCXGameInstance::RememberCredential(const FString& Credential)
{
	if (GetWorld() && GetWorld()->GetNetMode() == NM_Client && !GetWorld()->URL.Host.IsEmpty())
	{
		FString Endpoint;
		if (NormalizeAddress(GetWorld()->URL.Host + TEXT(":") + FString::FromInt(GetWorld()->URL.Port), Endpoint)) PendingEndpoint = Endpoint;
		Status = TEXT("방에 참가했습니다. 준비 버튼을 눌러주세요.");
	}
	FGuid Guid;
	if (!ClientSave || !FGuid::ParseExact(Credential, EGuidFormats::Digits, Guid)) return;
	ClientSave->Credentials.Add(PendingEndpoint, Credential);
	if (IsPersistenceEnabled() && !UGameplayStatics::SaveGameToSlot(ClientSave, TEXT("CX_ClientCredentials_v1"), 0)) Status = TEXT("재접속 식별자를 저장하지 못했습니다.");
}

FString UCXGameInstance::RestoreProfile(ACXPlayerState* State, const FString& RequestedCredential)
{
	if (!State || !State->HasAuthority() || !ServerSave) return FString();
	FGuid Guid;
	const APlayerController* OwnerPC = Cast<APlayerController>(State->GetOwner());
	const FString Requested = RequestedCredential.IsEmpty() && OwnerPC && OwnerPC->IsLocalController() ? CredentialFor(TEXT("LOCAL_HOST")) : RequestedCredential;
	const bool bKnown = FGuid::ParseExact(Requested, EGuidFormats::Digits, Guid) && ServerSave->Profiles.Contains(Requested);
	const FString Credential = bKnown ? Requested : FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FCXSavedProfile Record = bKnown ? ServerSave->Profiles.FindChecked(Credential) : FCXSavedProfile();
	State->RestoreEconomy(Record.Coins, Record.BonusSpeed);
	State->ProfileCredential = Credential;
	if (!bKnown)
	{
		ServerSave->Profiles.Add(Credential, FCXSavedProfile());
		bServerSaveDirty = true;
	}
	return Credential;
}

bool UCXGameInstance::SaveProfiles(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client || !ServerSave) return false;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (const ACXPlayerState* PS = It->Get()->GetPlayerState<ACXPlayerState>(); PS && !PS->ProfileCredential.IsEmpty())
		{
			bServerSaveDirty |= !ServerSave->Profiles.Contains(PS->ProfileCredential);
			FCXSavedProfile& Record = ServerSave->Profiles.FindOrAdd(PS->ProfileCredential);
			bServerSaveDirty |= Record.Coins != PS->GetCoins() || Record.BonusSpeed != PS->GetBonusMaxSpeed();
			Record.Coins = PS->GetCoins();
			Record.BonusSpeed = PS->GetBonusMaxSpeed();
		}
	}
	if (!bServerSaveDirty) return true;
	const bool bOK = !IsPersistenceEnabled() || UGameplayStatics::SaveGameToSlot(ServerSave, TEXT("CX_ServerProfiles_v1"), 0);
	if (bOK) bServerSaveDirty = false;
	if (!bOK) Status = TEXT("서버 저장에 실패했습니다. 저장 공간을 확인해주세요.");
	return bOK;
}

bool UCXGameInstance::NormalizeAddress(const FString& Input, FString& Output)
{
	FString Host, PortText;
	const FString Trimmed = Input.TrimStartAndEnd();
	if (!Trimmed.Split(TEXT(":"), &Host, &PortText)) { Host = Trimmed; PortText = TEXT("7777"); }
	FIPv4Address IP;
	if (!FIPv4Address::Parse(Host, IP) || PortText.IsEmpty() || PortText.Len() > 5) return false;
	for (TCHAR Digit : PortText) if (!FChar::IsDigit(Digit)) return false;
	const int32 Port = FCString::Atoi(*PortText);
	if (Port < 1 || Port > 65535) return false;
	Output = IP.ToString() + TEXT(":") + FString::FromInt(Port);
	return true;
}

void UCXGameInstance::ConnectTo(const FString& Endpoint)
{
	PendingEndpoint = Endpoint;
	FString URL = Endpoint;
	const FString Token = CredentialFor(Endpoint);
	if (!Token.IsEmpty()) URL += TEXT("?CXProfile=") + Token;
	if (APlayerController* PC = GetFirstLocalPlayerController()) PC->ClientTravel(URL, TRAVEL_Absolute);
}

void UCXGameInstance::JoinAddress(const FString& Address)
{
	if (bBusy) return;
	FString Normalized;
	if (!NormalizeAddress(Address, Normalized)) { Status = TEXT("IPv4 주소와 포트를 입력해주세요. 예: 192.168.0.10:7777"); return; }
	Status = TEXT("방에 연결하는 중...");
	ConnectTo(Normalized);
}

void UCXGameInstance::HostRoom()
{
	if (bBusy) return;
	if (!Sessions.IsValid()) { Status = TEXT("LAN 세션 기능을 사용할 수 없습니다."); return; }
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		bBusy = true; bHostAfterDestroy = true;
		if (!Sessions->DestroySession(NAME_GameSession)) { bBusy = false; bHostAfterDestroy = false; Status = TEXT("기존 방을 정리하지 못했습니다."); }
		return;
	}
	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = true;
	Settings.NumPublicConnections = 3;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bUsesPresence = false;
	Settings.Set(FName(TEXT("CX_ROOM")), FString(TEXT("CX_v1")), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	bBusy = true;
	Status = TEXT("LAN 방을 만드는 중...");
	if (!Sessions->CreateSession(0, NAME_GameSession, Settings)) { bBusy = false; Status = TEXT("방을 만들지 못했습니다."); }
}

void UCXGameInstance::OnCreated(FName, bool bSuccess)
{
	bBusy = false;
	if (!bSuccess) { Status = TEXT("방 생성 실패. 다른 포트가 사용 중인지 확인해주세요."); return; }
	PendingEndpoint = TEXT("LOCAL_HOST");
	FString Options = TEXT("listen");
	const FString Token = CredentialFor(PendingEndpoint);
	if (!Token.IsEmpty()) Options += TEXT("?CXProfile=") + Token;
	Status = TEXT("방을 만들었습니다. 모두 준비한 뒤 방장이 시작해주세요.");
	UGameplayStatics::OpenLevel(this, CXMap, true, Options);
}

void UCXGameInstance::OnDestroyed(FName, bool bSuccess)
{
	bBusy = false;
	const bool bRehost = bHostAfterDestroy;
	bHostAfterDestroy = false;
	if (!bSuccess) { Status = TEXT("세션 정리에 실패했습니다."); return; }
	if (bRehost) HostRoom();
}

void UCXGameInstance::QuickMatch()
{
	if (bBusy || !Sessions.IsValid()) return;
	Search = MakeShared<FOnlineSessionSearch>();
	Search->bIsLanQuery = true;
	Search->MaxSearchResults = 30;
	bBusy = true;
	Status = TEXT("같은 LAN의 방을 찾는 중...");
	if (!Sessions->FindSessions(0, Search.ToSharedRef())) { bBusy = false; Status = TEXT("방 검색을 시작하지 못했습니다."); }
}

void UCXGameInstance::OnFound(bool bSuccess)
{
	bBusy = false;
	if (!bSuccess || !Search.IsValid()) { Status = TEXT("방 검색 실패. IP 직접 참가도 사용할 수 있습니다."); return; }
	for (const FOnlineSessionSearchResult& Result : Search->SearchResults)
	{
		FString Room;
		if (Result.Session.NumOpenPublicConnections > 0 && Result.Session.SessionSettings.Get(FName(TEXT("CX_ROOM")), Room) && Room == TEXT("CX_v1"))
		{
			bBusy = true;
			if (!Sessions->JoinSession(0, NAME_GameSession, Result)) { bBusy = false; Status = TEXT("방 참가를 시작하지 못했습니다."); }
			return;
		}
	}
	Status = TEXT("참가할 방이 없어 새 방을 만듭니다.");
	HostRoom();
}

void UCXGameInstance::OnJoined(FName Name, EOnJoinSessionCompleteResult::Type Result)
{
	bBusy = false;
	FString Address;
	if (Result != EOnJoinSessionCompleteResult::Success || !Sessions->GetResolvedConnectString(Name, Address)) { Status = TEXT("방 참가 실패. IP 직접 참가를 시도해주세요."); return; }
	Status = TEXT("방에 참가하는 중...");
	ConnectTo(Address);
}

void UCXGameInstance::LeaveRoom()
{
	if (GetWorld() && GetWorld()->GetNetMode() != NM_Client) SaveProfiles(GetWorld());
	if (Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession)) Sessions->DestroySession(NAME_GameSession);
	PendingEndpoint = TEXT("LOCAL_HOST");
	Status = TEXT("로컬 로비로 돌아왔습니다.");
	const FString Token = CredentialFor(PendingEndpoint);
	UGameplayStatics::OpenLevel(this, CXMap, true, Token.IsEmpty() ? TEXT("") : TEXT("CXProfile=") + Token);
}

void UCXGameInstance::OnNetworkFailure(UWorld*, UNetDriver*, ENetworkFailure::Type, const FString&)
{
	bBusy = false;
	Status = TEXT("연결이 끊겼거나 참가하지 못했습니다. 주소·포트·방 상태를 확인해주세요.");
}

void UCXGameInstance::OnTravelFailure(UWorld*, ETravelFailure::Type, const FString&)
{
	bBusy = false;
	Status = TEXT("맵 이동에 실패했습니다. 다시 참가해주세요.");
}
