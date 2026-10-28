#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableTextBox.h"
#include "CXChatWidget.generated.h"

class UTextBlock;
class UScrollBox;

/** Native layout also works as the parent of an optional Widget Blueprint. */
UCLASS()
class CX_API UCXChatWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void AddMessage(const FString& Sender, const FString& Message);
	void UpdateStats(int32 Coins, float BonusMaxSpeed);
	void UpdateMatch(const class ACXGameState* State, const class APlayerState* LocalPlayer);
	void OpenChat();
	void CloseChat();
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
private:
	void BuildInterface();
	void Submit();
	UFUNCTION() void HandleCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void HandleSendClicked();
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> ChatInput;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> ChatHistory;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatsText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MatchText;
	FString LastMatchText;
	int32 LastCoins = INDEX_NONE;
	float LastBonusMaxSpeed = -1.0f;
	bool bChatOpen = false;
};
