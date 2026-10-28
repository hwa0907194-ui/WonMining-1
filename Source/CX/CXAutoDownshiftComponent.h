#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CXAutoDownshiftComponent.generated.h"

class AWheeledVehiclePawn;
class UChaosWheeledVehicleMovementComponent;

/** Road-speed-aware downshift assistance alongside the Chaos automatic box. */
UCLASS(ClassGroup=Vehicle, meta=(BlueprintSpawnableComponent))
class CX_API UCXAutoDownshiftComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCXAutoDownshiftComponent();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Automatic Downshift") bool bEnableSpeedBasedDownshift = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Automatic Downshift", meta=(ClampMin=0.1)) float MinimumShiftInterval = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Automatic Downshift", meta=(ClampMin=0)) float UpshiftMarginRPM = 600.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Automatic Downshift", meta=(ClampMin=0.5, ClampMax=1)) float RedlineFraction = 0.9f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Automatic Downshift") int32 LastAssistedGear = 0;
protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
	TWeakObjectPtr<AWheeledVehiclePawn> Vehicle;
	TWeakObjectPtr<UChaosWheeledVehicleMovementComponent> Movement;
	int32 LastForwardGear = 0;
	int32 PendingGear = 0;
	double NextAllowedShiftTime = 0;
	double PendingShiftDeadline = 0;
	void ClearPendingShift();
};
