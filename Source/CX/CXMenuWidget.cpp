#include "CXMenuWidget.h"
#include "CXGameInstance.h"
#include "CXGameState.h"
#include "CXPlayerState.h"
#include "CXPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "InputCoreTypes.h"

namespace
{
	void SetTextIfChanged(UTextBlock* Label, const FString& Text)
	{
		if (Label->GetText().ToString() != Text) Label->SetText(FText::FromString(Text));
	}
}

TSharedRef<SWidget> UCXMenuWidget::RebuildWidget()
{
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this);
	if (!WidgetTree->RootWidget) BuildMenu();
	SetIsFocusable(true);
	return Super::RebuildWidget();
}

void UCXMenuWidget::BuildMenu()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.015f, 0.025f, 0.05f, 0.98f));
	Panel->SetPadding(FMargin(24));
	UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f)); PanelSlot->SetAlignment(FVector2D(0.5f)); PanelSlot->SetAutoSize(true);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(); Panel->SetContent(Column);
	auto Text = [this, Column](const FString& Value)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(Value));
		Label->SetWrapTextAt(700);
		Column->AddChildToVerticalBox(Label)->SetPadding(FMargin(0, 5));
		return Label;
	};
	Text(TEXT("CX · 원화 채굴 로비 / 상점"))->SetColorAndOpacity(FLinearColor(1, 0.8f, 0.2f));
	Text(TEXT("최대 3명 · LAN 빠른 매칭 또는 IPv4 직접 참가\n모두 준비 → 방장 시작 → 대기 10초 → 경기 120초"));
	StatusText = Text(TEXT("방을 만들거나 참가해주세요. 혼자서도 준비 후 시작할 수 있습니다."));
	PlayersText = Text(TEXT("플레이어 목록"));
	BalanceText = Text(TEXT(""));
	Address = WidgetTree->ConstructWidget<UEditableTextBox>();
	Address->SetText(FText::FromString(TEXT("127.0.0.1:7777")));
	Address->WidgetStyle.ForegroundColor = FLinearColor::Black;
	Address->WidgetStyle.FocusedForegroundColor = FLinearColor::Black;
	Address->WidgetStyle.TextStyle.SetColorAndOpacity(FLinearColor::Black);
	Column->AddChildToVerticalBox(Address)->SetPadding(FMargin(0, 8));
	auto Row = [this, Column]() { UHorizontalBox* Box = WidgetTree->ConstructWidget<UHorizontalBox>(); Column->AddChildToVerticalBox(Box)->SetPadding(FMargin(0, 5)); return Box; };
	auto Button = [this](UHorizontalBox* Box, const FString& Caption)
	{
		UButton* NewButton = WidgetTree->ConstructWidget<UButton>();
		NewButton->SetBackgroundColor(FLinearColor(0.06f, 0.23f, 0.5f));
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(); Label->SetText(FText::FromString(Caption)); NewButton->SetContent(Label);
		Box->AddChildToHorizontalBox(NewButton)->SetPadding(FMargin(4));
		return NewButton;
	};
	UHorizontalBox* NetworkRow = Row();
	Button(NetworkRow, TEXT(" 방 만들기 "))->OnClicked.AddDynamic(this, &UCXMenuWidget::Host);
	Button(NetworkRow, TEXT(" LAN 빠른 매칭 "))->OnClicked.AddDynamic(this, &UCXMenuWidget::Quick);
	Button(NetworkRow, TEXT(" IP 참가 "))->OnClicked.AddDynamic(this, &UCXMenuWidget::Join);
	UHorizontalBox* LobbyRow = Row();
	ReadyButton = Button(LobbyRow, TEXT(" 준비 ")); ReadyText = Cast<UTextBlock>(ReadyButton->GetContent()); ReadyButton->OnClicked.AddDynamic(this, &UCXMenuWidget::Ready);
	StartButton = Button(LobbyRow, TEXT(" 방장: 경기 시작 ")); StartButton->OnClicked.AddDynamic(this, &UCXMenuWidget::Start);
	NextButton = Button(LobbyRow, TEXT(" 방장: 다음 로비 ")); NextButton->OnClicked.AddDynamic(this, &UCXMenuWidget::Next);
	UHorizontalBox* ShopRow = Row();
	BuyButton = Button(ShopRow, TEXT(" 20원 → +10 km/h ")); BuyButton->OnClicked.AddDynamic(this, &UCXMenuWidget::Buy);
	CloseButton = Button(ShopRow, TEXT(" 닫기 (Tab / Esc) "));
	CloseButton->OnClicked.AddDynamic(this, &UCXMenuWidget::Close);
	Button(ShopRow, TEXT(" 방 나가기 "))->OnClicked.AddDynamic(this, &UCXMenuWidget::Leave);
	Text(TEXT("경기 중 B: 즉시 구매 · Tab: 메뉴 · Enter: 채팅\n보유 원화·버프는 서버에 자동 저장되며 재경기에도 유지됩니다."));
}

void UCXMenuWidget::Refresh(const ACXGameState* State, const ACXPlayerState* Player)
{
	if (!State || !StatusText) return;
	if (const UCXGameInstance* GI = GetGameInstance<UCXGameInstance>()) SetTextIfChanged(StatusText, GI->GetStatus());
	FString Names;
	bool bAllReady = true;
	for (APlayerState* Entry : State->PlayerArray)
	{
		if (const ACXPlayerState* PS = Cast<ACXPlayerState>(Entry); IsValid(PS) && !PS->IsInactive())
		{
			Names += FString::Printf(TEXT("%s%s · %s\n"), State->GetLobbyLeader() == PS ? TEXT("[방장] ") : TEXT(""), *PS->GetPlayerName().Left(24), PS->IsReady() ? TEXT("준비 완료") : TEXT("준비 중"));
			bAllReady &= PS->IsReady();
		}
	}
	SetTextIfChanged(PlayersText, Names);
	if (Player) SetTextIfChanged(BalanceText, FString::Printf(TEXT("내 원화 %d · 추가 속도 %.1f km/h"), Player->GetCoins(), Player->GetBonusMaxSpeed()));
	SetTextIfChanged(ReadyText, Player && Player->IsReady() ? TEXT(" 준비 취소 ") : TEXT(" 준비 "));
	ReadyButton->SetIsEnabled(State->IsLobbyOpen());
	if (CloseButton.IsValid()) CloseButton->SetIsEnabled(!State->IsLobbyOpen() && State->GetPhase() != ECXMatchPhase::Finished);
	StartButton->SetIsEnabled(Player && State->GetLobbyLeader() == Player && State->IsLobbyOpen() && bAllReady);
	NextButton->SetIsEnabled(Player && State->GetLobbyLeader() == Player && State->GetPhase() == ECXMatchPhase::Finished);
	BuyButton->SetIsEnabled(Player && State->GetPhase() == ECXMatchPhase::Playing && Player->GetCoins() >= 20 && Player->GetBonusMaxSpeed() <= ACXPlayerState::MaxBonusSpeed - 10);
}

void UCXMenuWidget::Host() { if (UCXGameInstance* GI = GetGameInstance<UCXGameInstance>()) GI->HostRoom(); }
void UCXMenuWidget::Quick() { if (UCXGameInstance* GI = GetGameInstance<UCXGameInstance>()) GI->QuickMatch(); }
void UCXMenuWidget::Join() { if (UCXGameInstance* GI = GetGameInstance<UCXGameInstance>()) GI->JoinAddress(Address->GetText().ToString()); }
void UCXMenuWidget::Leave() { if (UCXGameInstance* GI = GetGameInstance<UCXGameInstance>()) GI->LeaveRoom(); }
void UCXMenuWidget::Ready() { if (ACXPlayerController* PC = GetOwningPlayer<ACXPlayerController>()) PC->ServerToggleReady(); }
void UCXMenuWidget::Start() { if (ACXPlayerController* PC = GetOwningPlayer<ACXPlayerController>()) PC->ServerStartMatch(); }
void UCXMenuWidget::Next() { if (ACXPlayerController* PC = GetOwningPlayer<ACXPlayerController>()) PC->ServerNextRound(); }
void UCXMenuWidget::Buy() { if (ACXPlayerController* PC = GetOwningPlayer<ACXPlayerController>()) PC->ServerBuySpeed(); }
void UCXMenuWidget::Close() { if (ACXPlayerController* PC = GetOwningPlayer<ACXPlayerController>()) PC->CloseMenu(); }

FReply UCXMenuWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Tab || Event.GetKey() == EKeys::Escape) { Close(); return FReply::Handled(); }
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}
