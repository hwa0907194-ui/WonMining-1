#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CXMenuWidget.generated.h"

UCLASS()
class CX_API UCXMenuWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Refresh(const class ACXGameState* State, const class ACXPlayerState* Player);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
	void BuildMenu();
	UFUNCTION() void Host();
	UFUNCTION() void Quick();
	UFUNCTION() void Join();
	UFUNCTION() void Leave();
	UFUNCTION() void Ready();
	UFUNCTION() void Start();
	UFUNCTION() void Next();
	UFUNCTION() void Buy();
	UFUNCTION() void Close();
	UPROPERTY() TObjectPtr<class UEditableTextBox> Address;
	UPROPERTY() TObjectPtr<class UTextBlock> StatusText;
	UPROPERTY() TObjectPtr<class UTextBlock> PlayersText;
	UPROPERTY() TObjectPtr<class UTextBlock> BalanceText;
	UPROPERTY() TObjectPtr<class UTextBlock> ReadyText;
	UPROPERTY() TObjectPtr<class UButton> ReadyButton;
	UPROPERTY() TObjectPtr<class UButton> StartButton;
	UPROPERTY() TObjectPtr<class UButton> NextButton;
	UPROPERTY() TObjectPtr<class UButton> BuyButton;
	TWeakObjectPtr<class UButton> CloseButton;
};
