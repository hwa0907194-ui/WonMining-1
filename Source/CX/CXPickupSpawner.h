#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CXPickupSpawner.generated.h"

UCLASS()
class CX_API ACXPickupSpawner : public AActor
{
	GENERATED_BODY()
public:
	ACXPickupSpawner();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pickup") TSubclassOf<class ACXPickup> PickupClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pickup", meta=(ClampMin=0.1)) float RespawnDelay = 2.0f;
	class ACXPickup* GetActivePickup() const { return ActivePickup; }
	void ResetForRound();
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UPROPERTY(VisibleInstanceOnly, Category="Pickup") TObjectPtr<class ACXPickup> ActivePickup;
	FTimerHandle RespawnTimer;
	void SpawnPickup();
	UFUNCTION() void OnPickupDestroyed(AActor* DestroyedActor);
};
