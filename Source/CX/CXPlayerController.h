// CXPlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CXPlayerController.generated.h"

class UInputMappingContext;
class ACXPawn;
class UCXUI;
class UCXChatWidget;
class UCXMenuWidget;

/**
 *  Vehicle Player Controller class
 *  Handles input mapping and user interface
 */
UCLASS(abstract)
class CX_API ACXPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	/** Input Mapping Context to be used for player input */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputMappingContext* InputMappingContext;

	/** If true, the optional steering wheel input mapping context will be registered */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	bool bUseSteeringWheelControls = false;

	/** Optional Input Mapping Context to be used for steering wheel input.
	 *  This is added alongside the default Input Mapping Context and does not block other forms of input.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta=(EditCondition="bUseSteeringWheelControls"))
	UInputMappingContext* SteeringWheelInputMappingContext;

	/** Pointer to the controlled vehicle pawn */
	UPROPERTY(Replicated)
	TObjectPtr<ACXPawn> VehiclePawn;

	/** Type of the UI to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = UI)
	TSubclassOf<UCXUI> VehicleUIClass;

	/** Pointer to the UI widget */
	UPROPERTY(Transient)
	TObjectPtr<UCXUI> VehicleUI;

	UPROPERTY(EditDefaultsOnly, Category="Chat")
	TSubclassOf<UCXChatWidget> ChatWidgetClass;
	UPROPERTY(Transient)
	TObjectPtr<UCXChatWidget> ChatWidget;
	UPROPERTY(Transient) TObjectPtr<UCXMenuWidget> MenuWidget;
	bool bMenuRequested = false;
	bool bMenuVisible = false;
	float HUDRefreshElapsed = 0.0f;
	void ToggleMenu();
	void BuySpeed();
	double LastPurchaseRequest = -100;
	TArray<TPair<FString, FString>> PendingChat;
	double LastChatTime = -1.0;
	void OpenChat();
	UFUNCTION(Server, Reliable)
	void ServerSubmitChat(const FString& Message);

	

	// Begin Actor interface
protected:

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const;

public:
	void CloseMenu();
	UFUNCTION(Server, Reliable) void ServerToggleReady();
	UFUNCTION(Server, Reliable) void ServerStartMatch();
	UFUNCTION(Server, Reliable) void ServerNextRound();
	UFUNCTION(Server, Reliable) void ServerBuySpeed();
	UFUNCTION(Client, Reliable) void ClientRememberProfile(const FString& Credential);
	UFUNCTION(BlueprintCallable, Category="Chat")
	void SendChatMessage(const FString& Message);
	UFUNCTION(Client, Reliable)
	void ClientReceiveChat(const FString& Sender, const FString& Message);

	virtual void Tick(float Delta) override;

	// End Actor interface

	// Begin PlayerController interface
protected:

	virtual void OnPossess(APawn* InPawn) override;

	// End PlayerController interface
};
