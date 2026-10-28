#include "CXAutoDownshiftComponent.h"
#include "CXDownshiftLogic.h"
#include "CXGameState.h"
#include "WheeledVehiclePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

UCXAutoDownshiftComponent::UCXAutoDownshiftComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UCXAutoDownshiftComponent::BeginPlay()
{
	Super::BeginPlay();
	Vehicle = Cast<AWheeledVehiclePawn>(GetOwner());
	if (Vehicle.IsValid()) Movement = Cast<UChaosWheeledVehicleMovementComponent>(Vehicle->GetVehicleMovement());
	if (Movement.IsValid()) AddTickPrerequisiteComponent(Movement.Get());
}

void UCXAutoDownshiftComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Vehicle.IsValid() || !Movement.IsValid() || !Vehicle->IsLocallyControlled())
	{
		ClearPendingShift();
		return;
	}
	const ACXGameState* State = GetWorld()->GetGameState<ACXGameState>();
	if (!bEnableSpeedBasedDownshift || !State || State->GetPhase() != ECXMatchPhase::Playing
		|| !Movement->GetUseAutoGears() || !Movement->TransmissionSetup.bUseAutomaticGears)
	{
		LastForwardGear = 0;
		ClearPendingShift();
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const int32 CurrentGear = Movement->GetCurrentGear();
	if (PendingGear > 0)
	{
		if (CurrentGear == PendingGear || Now >= PendingShiftDeadline || Movement->GetTargetGear() < 0)
			ClearPendingShift();
		else
		{
			// A stopped vehicle may sleep before the timed shift can finish.
			// Wake only for this short pending shift, then restore the 10 Hz tick.
			Vehicle->GetMesh()->WakeAllRigidBodies();
			return;
		}
	}
	const float Interval = FMath::Max(0.1f, MinimumShiftInterval);
	if (CurrentGear > 0)
	{
		// Detect completed upshifts, ignoring neutral during a timed gear change.
		if (LastForwardGear > 0 && CurrentGear > LastForwardGear)
			NextAllowedShiftTime = FMath::Max(NextAllowedShiftTime, Now + Interval);
		LastForwardGear = CurrentGear;
	}
	// Respect a reverse request issued by Chaos before its next physics output.
	if (CurrentGear <= 1 || Movement->GetTargetGear() < 0 || Now < NextAllowedShiftTime) return;
	if (Movement->GetHandbrakeInput())
	{
		NextAllowedShiftTime = Now + 0.25;
		return;
	}
	const FVector LocalVelocity = Vehicle->GetActorTransform().InverseTransformVectorNoScale(Vehicle->GetVelocity());
	if (LocalVelocity.X < -10.0f || FMath::Abs(LocalVelocity.Y) > FMath::Max(150.0f, FMath::Abs(LocalVelocity.X) * 0.5f)) return;
	float SmallestRadius = MAX_flt;
	for (int32 Index = 0; Index < Movement->Wheels.Num(); ++Index)
	{
		UChaosVehicleWheel* Wheel = Movement->Wheels[Index];
		if (!Wheel || Index >= Movement->GetNumWheels()) return;
		const EVehicleDifferential Differential = Movement->DifferentialSetup.DifferentialType;
		const EAxleType Axle = Wheel->GetAxleType();
		const bool bDriven = Axle == EAxleType::Undefined ? Wheel->bAffectedByEngine
			: Differential == EVehicleDifferential::AllWheelDrive
			|| (Axle == EAxleType::Front && Differential == EVehicleDifferential::FrontWheelDrive)
			|| (Axle == EAxleType::Rear && Differential == EVehicleDifferential::RearWheelDrive);
		if (!bDriven) continue;
		const FWheelStatus& WheelState = Movement->GetWheelState(Index);
		// The inherited navigation IsMovingOnGround() always returns false
		// for Chaos vehicles; use actual wheel contacts instead.
		if (!WheelState.bInContact || WheelState.bIsSlipping || WheelState.bIsSkidding)
		{
			NextAllowedShiftTime = Now + 0.25;
			return;
		}
		const float Radius = Wheel->GetWheelRadius();
		if (!FMath::IsFinite(Radius) || Radius <= 0) return;
		SmallestRadius = FMath::Min(SmallestRadius, Radius);
	}
	if (SmallestRadius == MAX_flt) return;
	FCXDownshiftInput Input;
	Input.ForwardRatios = Movement->TransmissionSetup.ForwardGearRatios;
	Input.CurrentGear = CurrentGear;
	Input.SpeedCmS = Vehicle->GetVelocity().Size2D();
	Input.WheelRadiusCm = SmallestRadius;
	Input.FinalRatio = Movement->TransmissionSetup.FinalRatio;
	Input.EngineRPM = Movement->GetEngineRotationSpeed();
	Input.IdleRPM = Movement->EngineSetup.EngineIdleRPM;
	Input.MaxRPM = Movement->EngineSetup.MaxRPM;
	Input.ChangeDownRPM = Movement->TransmissionSetup.ChangeDownRPM;
	Input.ChangeUpRPM = Movement->TransmissionSetup.ChangeUpRPM;
	Input.UpshiftMarginRPM = UpshiftMarginRPM;
	Input.RedlineFraction = RedlineFraction;
	const int32 Gear = CXDownshift::ChooseGear(Input);
	if (Gear >= 1 && Gear < CurrentGear)
	{
		Vehicle->GetMesh()->WakeAllRigidBodies();
		Movement->SetTargetGear(Gear, false);
		LastAssistedGear = Gear;
		PendingGear = Gear;
		PendingShiftDeadline = Now + FMath::Max(1.0f, Movement->TransmissionSetup.GearChangeTime + 1.0f);
		SetComponentTickInterval(0);
		NextAllowedShiftTime = Now + FMath::Max(Interval, Movement->TransmissionSetup.GearChangeTime + 0.25f);
	}
}

void UCXAutoDownshiftComponent::ClearPendingShift()
{
	if (PendingGear == 0) return;
	PendingGear = 0;
	SetComponentTickInterval(0.1f);
}
