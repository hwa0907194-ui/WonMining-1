#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CXPickup.generated.h"

UENUM(BlueprintType)
enum class ECXPickupKind : uint8 { Coin, Speed };

UCLASS()
class CX_API ACXPickup : public AActor
{
	GENERATED_BODY()
public:
	ACXPickup();
	bool TryCollect(class ACXPawn* Vehicle);
	void RemoveForReset() { bSilentDestruction = true; Destroy(); }
	ECXPickupKind GetKind() const { return Kind; }
	virtual void Tick(float DeltaSeconds) override;
	virtual void Destroyed() override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) ECXPickupKind Kind = ECXPickupKind::Coin;
private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<class USphereComponent> Trigger;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Visual;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UTextRenderComponent> Label;
	UPROPERTY() TObjectPtr<class UStaticMesh> CoinMesh;
	UPROPERTY() TObjectPtr<class UStaticMesh> SpeedMesh;
	bool bConsumed = false;
	bool bSilentDestruction = false;
	FTimerHandle CollectionRetryTimer;
	void RetryCollection();
	UFUNCTION() void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION() void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};

UCLASS()
class CX_API ACXCoinPickup : public ACXPickup
{
	GENERATED_BODY()
};

UCLASS()
class CX_API ACXSpeedPickup : public ACXPickup
{
	GENERATED_BODY()
public:
	ACXSpeedPickup();
};
