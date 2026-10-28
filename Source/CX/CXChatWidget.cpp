#include "CXChatWidget.h"
#include "CXPlayerController.h"
#include "CXGameState.h"
#include "GameFramework/PlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"

TSharedRef<SWidget> UCXChatWidget::RebuildWidget()
{
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	if (!WidgetTree->RootWidget) BuildInterface();
	return Super::RebuildWidget();
}

void UCXChatWidget::BuildInterface()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	StatsText = WidgetTree->ConstructWidget<UTextBlock>();
	StatsText->SetColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.2f));
	UCanvasPanelSlot* StatsSlot = Canvas->AddChildToCanvas(StatsText);
	StatsSlot->SetPosition(FVector2D(24, 24));
	StatsSlot->SetAutoSize(true);
	LastCoins = INDEX_NONE;
	UpdateStats(0, 0);
	MatchText = WidgetTree->ConstructWidget<UTextBlock>();
	MatchText->SetJustification(ETextJustify::Center);
	MatchText->SetColorAndOpacity(FLinearColor::White);
	UCanvasPanelSlot* MatchSlot = Canvas->AddChildToCanvas(MatchText);
	MatchSlot->SetAnchors(FAnchors(0.5f, 0));
	MatchSlot->SetAlignment(FVector2D(0.5f, 0));
	MatchSlot->SetPosition(FVector2D(0, 75));
	MatchSlot->SetAutoSize(true);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.05f, 0.85f));
	Panel->SetPadding(FMargin(12));
	UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(1, 1));
	PanelSlot->SetAlignment(FVector2D(1, 1));
	PanelSlot->SetPosition(FVector2D(-24, -24));
	PanelSlot->SetSize(FVector2D(520, 260));
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Column);
	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>();
	Hint->SetText(FText::FromString(TEXT("채팅 · Enter 입력 / Esc 닫기")));
	Column->AddChildToVerticalBox(Hint);
	ChatHistory = WidgetTree->ConstructWidget<UScrollBox>();
	Column->AddChildToVerticalBox(ChatHistory)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(Row);
	ChatInput = WidgetTree->ConstructWidget<UEditableTextBox>();
	ChatInput->SetHintText(FText::FromString(TEXT("메시지를 입력하세요 (최대 200자)")));
	ChatInput->SetClearKeyboardFocusOnCommit(false);
	ChatInput->SetForegroundColor(FLinearColor(0.06f, 0.07f, 0.09f));
	ChatInput->WidgetStyle.SetFocusedForegroundColor(FSlateColor(FLinearColor(0.06f, 0.07f, 0.09f)));
	ChatInput->WidgetStyle.TextStyle.SetColorAndOpacity(FSlateColor(FLinearColor(0.06f, 0.07f, 0.09f)));
	Row->AddChildToHorizontalBox(ChatInput)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UButton* Send = WidgetTree->ConstructWidget<UButton>();
	Send->SetBackgroundColor(FLinearColor(0.05f, 0.25f, 0.6f));
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
	Label->SetText(FText::FromString(TEXT(" 보내기 ")));
	Send->SetContent(Label);
	Row->AddChildToHorizontalBox(Send);
	Send->OnClicked.AddDynamic(this, &UCXChatWidget::HandleSendClicked);
	ChatInput->OnTextCommitted.AddDynamic(this, &UCXChatWidget::HandleCommitted);
}

void UCXChatWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
}

void UCXChatWidget::UpdateStats(int32 Coins, float BonusMaxSpeed)
{
	if (!StatsText || (Coins == LastCoins && BonusMaxSpeed == LastBonusMaxSpeed)) return;
	LastCoins = Coins;
	LastBonusMaxSpeed = BonusMaxSpeed;
	StatsText->SetText(FText::FromString(FString::Printf(TEXT("원화: %d   |   추가 최대 속도: %.1f km/h"), Coins, BonusMaxSpeed)));
}

void UCXChatWidget::AddMessage(const FString& Sender, const FString& Message)
{
	if (!ChatHistory) return;
	// Keep widget count and memory bounded over long sessions.
	while (ChatHistory->GetChildrenCount() >= 100) ChatHistory->RemoveChildAt(0);
	UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>();
	Line->SetAutoWrapText(true);
	Line->SetText(FText::FromString(Sender.IsEmpty() ? Message : FString::Printf(TEXT("%s: %s"), *Sender, *Message)));
	ChatHistory->AddChild(Line);
	ChatHistory->ScrollToEnd();
}

void UCXChatWidget::UpdateMatch(const ACXGameState* State, const APlayerState* LocalPlayer)
{
	if (!State || !MatchText) return;
	FString Text;
	if (State->IsLobbyOpen()) Text = TEXT("로비 · 모두 준비 후 방장이 시작해주세요");
	else if (State->GetPhase() == ECXMatchPhase::Waiting)
		Text = FString::Printf(TEXT("대기 · 시작까지 %d초\nWASD 운전 / 좌클릭 공격 / Enter 채팅"), State->GetRemainingSeconds());
	else if (State->GetPhase() == ECXMatchPhase::Playing)
		Text = FString::Printf(TEXT("원화 채굴 진행 중 · %02d:%02d\n좌클릭 약탈 / B: 20원으로 속도 구매 / Tab: 메뉴"), State->GetRemainingSeconds() / 60, State->GetRemainingSeconds() % 60);
	else
	{
		const auto& Winners = State->GetWinners();
		const bool bWinner = LocalPlayer && Winners.Contains(LocalPlayer);
		FString Names;
		for (const APlayerState* Winner : Winners)
		{
			if (!IsValid(Winner)) continue;
			if (!Names.IsEmpty()) Names += TEXT(", ");
			Names += Winner->GetPlayerName().Left(32);
		}
		Text = FString::Printf(TEXT("경기 종료 · %s\n우승자: %s\n%d초 후 다음 로비"), bWinner ? (Winners.Num() > 1 ? TEXT("공동 승리!") : TEXT("승리!")) : TEXT("패배"), *Names, State->GetRemainingSeconds());
	}
	if (Text != LastMatchText)
	{
		LastMatchText = Text;
		MatchText->SetText(FText::FromString(Text));
		if (State->GetPhase() == ECXMatchPhase::Finished) UE_LOG(LogTemp, Display, TEXT("CX_RESULT %s"), *Text.Replace(TEXT("\n"), TEXT(" ")));
	}
}

void UCXChatWidget::OpenChat()
{
	if (bChatOpen) return;
	if (APlayerController* PC = GetOwningPlayer(); PC && ChatInput)
	{
		bChatOpen = true;
		// Stop the previous throttle/brake input before ignoring driving actions.
		PC->FlushPressedKeys();
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(ChatInput->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
		ChatInput->SetKeyboardFocus();
	}
}

void UCXChatWidget::CloseChat()
{
	if (!bChatOpen) return;
	bChatOpen = false;
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
}

void UCXChatWidget::Submit()
{
	if (ChatInput)
	{
		if (ACXPlayerController* PC = GetOwningPlayer<ACXPlayerController>()) PC->SendChatMessage(ChatInput->GetText().ToString());
		ChatInput->SetText(FText::GetEmpty());
	}
	CloseChat();
}

void UCXChatWidget::HandleCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter) Submit();
}

void UCXChatWidget::HandleSendClicked() { Submit(); }

FReply UCXChatWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseChat();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}
