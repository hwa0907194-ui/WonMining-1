// CXPlayerController.cpp


#include "CXPlayerController.h"
#include "CXPawn.h"
#include "CXUI.h"
#include "CXChatWidget.h"
#include "CXPlayerState.h"
#include "CXGameMode.h"
#include "CXGameState.h"
#include "CXGameInstance.h"
#include "CXMenuWidget.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Tests/CXNetworkSmokeTest.h"
#include "Tests/CXGameplaySmokeTest.h"
#include "Tests/CXExtendedSmokeTest.h"
#include "EnhancedInputSubsystems.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Net/UnrealNetwork.h"

void ACXPlayerController::BeginPlay()
{
	Super::BeginPlay();
	ScheduleCXNetworkSmokeTest(this);
	ScheduleCXGameplayClientTest(this);
	ScheduleCXExtendedClientTest(this);

	if (IsLocalController() == false)
	{
		return;
	}
	
	// spawn the UI widget and add it to the viewport
	if (VehicleUIClass)
	{
		VehicleUI = CreateWidget<UCXUI>(this, VehicleUIClass);
		if (VehicleUI) VehicleUI->AddToViewport();
	}
	ChatWidget = CreateWidget<UCXChatWidget>(this, ChatWidgetClass ? ChatWidgetClass.Get() : UCXChatWidget::StaticClass());
	if (ChatWidget)
	{
		ChatWidget->AddToViewport(10);
		for (const auto& Entry : PendingChat) ChatWidget->AddMessage(Entry.Key, Entry.Value);
		PendingChat.Empty();
	}
	MenuWidget = CreateWidget<UCXMenuWidget>(this, UCXMenuWidget::StaticClass());
	if (MenuWidget) { MenuWidget->AddToViewport(20); MenuWidget->SetVisibility(ESlateVisibility::Collapsed); }
}

void ACXPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!IsLocalController()) return;
	if (InputComponent) InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ACXPlayerController::OpenChat);
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ACXPlayerController::ToggleMenu);
		InputComponent->BindKey(EKeys::B, IE_Pressed, this, &ACXPlayerController::BuySpeed);
	}
	
	// get the enhanced input subsystem
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		// add the mapping context so we get controls
		if (InputMappingContext) Subsystem->AddMappingContext(InputMappingContext, 0);

		// optionally add the steering wheel context
		if (bUseSteeringWheelControls && SteeringWheelInputMappingContext)
		{
			Subsystem->AddMappingContext(SteeringWheelInputMappingContext, 1);
		}
	}
}

void ACXPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, VehiclePawn);
}

void ACXPlayerController::Tick(float Delta)
{
	Super::Tick(Delta);
	if (!IsLocalController()) return;
	if (MenuWidget)
	{
		const ACXGameState* GS = GetWorld()->GetGameState<ACXGameState>();
		const bool bShow = bMenuRequested || (GS && (GS->IsLobbyOpen() || GS->GetPhase() == ECXMatchPhase::Finished));
		if (bShow != bMenuVisible)
		{
			bMenuVisible = bShow;
			MenuWidget->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			FlushPressedKeys();
			if (bShow)
			{
				if (ChatWidget) ChatWidget->CloseChat();
				MenuWidget->Refresh(GS, GetPlayerState<ACXPlayerState>());
				SetInputMode(FInputModeUIOnly());
				bShowMouseCursor = true;
				MenuWidget->SetKeyboardFocus();
			}
			else { SetInputMode(FInputModeGameOnly()); bShowMouseCursor = false; }
		}
	}
	// Keep driving instruments smooth; slower-changing HUD data only needs 10 Hz.
	HUDRefreshElapsed += Delta;
	if (HUDRefreshElapsed >= 0.1f)
	{
		HUDRefreshElapsed = 0.0f;
		const ACXGameState* GS = GetWorld()->GetGameState<ACXGameState>();
		const ACXPlayerState* Stats = GetPlayerState<ACXPlayerState>();
		if (bMenuVisible && MenuWidget) MenuWidget->Refresh(GS, Stats);
		if (ChatWidget)
		{
			ChatWidget->UpdateMatch(GS, PlayerState);
			if (Stats) ChatWidget->UpdateStats(Stats->GetCoins(), Stats->GetBonusMaxSpeed());
		}
	}

	if (IsValid(VehiclePawn) && IsValid(VehicleUI))
	{
		VehicleUI->UpdateSpeed(VehiclePawn->GetChaosVehicleMovement()->GetForwardSpeed());
		VehicleUI->UpdateGear(VehiclePawn->GetChaosVehicleMovement()->GetCurrentGear());
	}
}

void ACXPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// get a pointer to the controlled pawn
	VehiclePawn = Cast<ACXPawn>(InPawn);
}

void ACXPlayerController::OpenChat()
{
	if (!bMenuVisible && ChatWidget) ChatWidget->OpenChat();
}

void ACXPlayerController::ToggleMenu() { bMenuRequested = !bMenuVisible; }
void ACXPlayerController::CloseMenu() { bMenuRequested = false; }
void ACXPlayerController::BuySpeed() { ServerBuySpeed(); }

void ACXPlayerController::ServerBuySpeed_Implementation()
{
	const ACXGameState* GS = GetWorld()->GetGameState<ACXGameState>();
	ACXPlayerState* PS = GetPlayerState<ACXPlayerState>();
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastPurchaseRequest < 0.25) return;
	LastPurchaseRequest = Now;
	const bool bOK = GS && GS->GetPhase() == ECXMatchPhase::Playing && PS && PS->BuySpeedBoost();
	ClientReceiveChat(TEXT("상점"), bOK ? TEXT("20원으로 추가 최대 속도 +10 km/h를 구매했습니다.") : TEXT("경기 중에만 구매할 수 있습니다. 잔액 20원과 속도 상한을 확인해주세요."));
}

void ACXPlayerController::ServerToggleReady_Implementation()
{
	const ACXGameState* GS = GetWorld()->GetGameState<ACXGameState>();
	if (ACXPlayerState* PS = GetPlayerState<ACXPlayerState>(); GS && GS->IsLobbyOpen() && PS) PS->SetReady(!PS->IsReady());
}

void ACXPlayerController::ServerStartMatch_Implementation()
{
	ACXGameMode* GM = GetWorld()->GetAuthGameMode<ACXGameMode>();
	if (!GM || !GM->TryStartMatch(this)) ClientReceiveChat(TEXT("로비"), TEXT("방장만 시작할 수 있습니다. 모든 플레이어가 준비해야 합니다."));
}

void ACXPlayerController::ServerNextRound_Implementation()
{
	if (ACXGameMode* GM = GetWorld()->GetAuthGameMode<ACXGameMode>()) GM->ReturnToLobby(this);
}

void ACXPlayerController::ClientRememberProfile_Implementation(const FString& Credential)
{
	if (UCXGameInstance* GI = GetGameInstance<UCXGameInstance>()) GI->RememberCredential(Credential);
}

void ACXPlayerController::SendChatMessage(const FString& Message)
{
	if (!IsLocalController()) return;
	const FString Trimmed = Message.TrimStartAndEnd();
	if (Trimmed.IsEmpty() || Trimmed.Len() > 200)
	{
		ClientReceiveChat_Implementation(TEXT(""), TEXT("메시지는 1~200자로 입력해주세요."));
		return;
	}
	ServerSubmitChat(Trimmed);
}

void ACXPlayerController::ServerSubmitChat_Implementation(const FString& Message)
{
	// Validate again on the authority; a modified client can bypass local checks.
	if (Message.Len() > 200) return;
	const FString Trimmed = Message.TrimStartAndEnd();
	if (Trimmed.IsEmpty()) return;
	for (TCHAR CodePoint : Trimmed)
	{
		if (FChar::IsControl(CodePoint)) return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastChatTime < 0.75)
	{
		ClientReceiveChat(TEXT(""), TEXT("잠시 후 다시 전송해주세요."));
		return;
	}
	LastChatTime = Now;
	if (ACXGameMode* Rules = GetWorld()->GetAuthGameMode<ACXGameMode>()) Rules->BroadcastChat(this, Trimmed);
}

void ACXPlayerController::ClientReceiveChat_Implementation(const FString& Sender, const FString& Message)
{
	if (!IsLocalController()) return;
	UE_LOG(LogTemp, Display, TEXT("CX_CHAT [%s] %s"), *Sender, *Message);
	if (ChatWidget) ChatWidget->AddMessage(Sender, Message);
	else
	{
		if (PendingChat.Num() >= 100) PendingChat.RemoveAt(0);
		PendingChat.Emplace(Sender, Message);
	}
}
