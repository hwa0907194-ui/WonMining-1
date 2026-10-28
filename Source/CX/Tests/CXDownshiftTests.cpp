#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../CXDownshiftLogic.h"
#include "../CXAutoDownshiftComponent.h"
#include "../CXPawn.h"
#include "../CXGameState.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include <limits>

class FCXDownshiftRuntimeCommand final : public IAutomationLatentCommand
{
public:
	explicit FCXDownshiftRuntimeCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		if (Stage == 0)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (!World || !World->IsGameWorld()) continue;
				for (TActorIterator<ACXPawn> It(World); It; ++It) if (It->IsLocallyControlled()) { Car = *It; break; }
			}
			if (!Car.IsValid()) { Test->AddError(TEXT("No live local vehicle for downshift test")); return true; }
			Move = Car->GetChaosVehicleMovement();
			Assist = Car->FindComponentByClass<UCXAutoDownshiftComponent>();
			State = Car->GetWorld()->GetGameState<ACXGameState>();
			if (!Move.IsValid() || !Assist.IsValid() || !State.IsValid() || Move->TransmissionSetup.ForwardGearRatios.Num() < 3)
			{ Test->AddError(TEXT("Missing vehicle setup for downshift test")); return true; }
			Original = Move->GetSnapshot();
			OriginalAuto = Move->GetUseAutoGears();
			OriginalAssist = Assist->bEnableSpeedBasedDownshift;
			OriginalHandbrake = Move->GetHandbrakeInput();
			OriginalLobby = State->IsLobbyOpen(); OriginalPhase = State->GetPhase(); OriginalTime = State->GetRemainingSeconds();
			State->SetLobby(false, State->GetLobbyLeader());
			State->SetMatchData(ECXMatchPhase::Playing, 120);
			Assist->bEnableSpeedBasedDownshift = false;
			Move->SetUseAutomaticGears(false);
			Move->SetHandbrakeInput(false);
			Move->SetThrottleInput(0);
			Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Car->GetMesh()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
			Move->SetTargetGear(Move->TransmissionSetup.ForwardGearRatios.Num(), true);
			Stage = 1; StageTime = Car->GetWorld()->GetTimeSeconds();
			return false;
		}
		if (!Car.IsValid() || !Move.IsValid() || !Assist.IsValid() || !State.IsValid())
		{ Test->AddError(TEXT("Vehicle disappeared during downshift test")); return true; }
		const double Now = Car->GetWorld()->GetTimeSeconds();
		if (Stage == 1 && Now - StageTime >= 0.4)
		{
			Test->TestEqual(TEXT("Fixture held highest gear"), Move->GetCurrentGear(), Move->TransmissionSetup.ForwardGearRatios.Num());
			Move->SetUseAutomaticGears(true);
			Assist->bEnableSpeedBasedDownshift = true;
			// Exercise the production component against the live Chaos output
			// before the next physics step can apply the built-in auto downshift.
			static_cast<UActorComponent*>(Assist.Get())->TickComponent(0.1f, LEVELTICK_All, nullptr);
			if (Assist->LastAssistedGear != 1)
			{
				Test->AddInfo(FString::Printf(TEXT("Downshift fixture auto=%d current=%d target=%d rpm=%.1f speed=%.1f final=%.3f idle=%.1f max=%.1f down=%.1f up=%.1f handbrake=%d"),
					Move->TransmissionSetup.bUseAutomaticGears, Move->GetCurrentGear(), Move->GetTargetGear(), Move->GetEngineRotationSpeed(), Car->GetVelocity().Size2D(),
					Move->TransmissionSetup.FinalRatio, Move->EngineSetup.EngineIdleRPM, Move->EngineSetup.MaxRPM, Move->TransmissionSetup.ChangeDownRPM, Move->TransmissionSetup.ChangeUpRPM, Move->GetHandbrakeInput()));
				for (float Ratio : Move->TransmissionSetup.ForwardGearRatios) Test->AddInfo(FString::Printf(TEXT("Ratio %.4f"), Ratio));
				for (int32 Index = 0; Index < Move->GetNumWheels(); ++Index)
				{
					const FWheelStatus& Wheel = Move->GetWheelState(Index);
					Test->AddInfo(FString::Printf(TEXT("Wheel %d contact=%d slip=%d skid=%d radius=%.2f"), Index, Wheel.bInContact, Wheel.bIsSlipping, Wheel.bIsSkidding, Move->Wheels[Index]->GetWheelRadius()));
				}
			}
			Test->TestEqual(TEXT("Assist requested first at standstill"), Assist->LastAssistedGear, 1);
			Stage = 2; StageTime = Now;
			return false;
		}
		if (Stage == 2 && Now - StageTime >= FMath::Max(0.8f, Move->TransmissionSetup.GearChangeTime + 0.3f))
		{
			Test->TestEqual(TEXT("Chaos completed assisted gear change"), Move->GetCurrentGear(), 1);
			Assist->bEnableSpeedBasedDownshift = OriginalAssist;
			Move->SetUseAutomaticGears(OriginalAuto);
			Move->SetSnapshot(Original);
			Move->SetHandbrakeInput(OriginalHandbrake);
			State->SetLobby(OriginalLobby, State->GetLobbyLeader());
			State->SetMatchData(OriginalPhase, OriginalTime);
			return true;
		}
		return false;
	}
private:
	FAutomationTestBase* Test;
	TWeakObjectPtr<ACXPawn> Car;
	TWeakObjectPtr<UChaosWheeledVehicleMovementComponent> Move;
	TWeakObjectPtr<UCXAutoDownshiftComponent> Assist;
	TWeakObjectPtr<ACXGameState> State;
	FWheeledSnaphotData Original;
	bool OriginalAuto = true, OriginalAssist = true, OriginalHandbrake = false, OriginalLobby = true;
	ECXMatchPhase OriginalPhase = ECXMatchPhase::Waiting;
	int32 OriginalTime = 0, Stage = 0;
	double StageTime = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXDownshiftTest, "CX.Vehicle.Downshift",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCXDownshiftTest::RunTest(const FString& Parameters)
{
	const TArray<float> Ratios {4.25f, 2.52f, 1.66f, 1.22f, 1.0f};
	FCXDownshiftInput Input;
	Input.ForwardRatios = Ratios; Input.CurrentGear = 5; Input.WheelRadiusCm = 40; Input.FinalRatio = 2.81f;
	float RPM = 0;
	TestTrue(TEXT("Predict physical RPM"), CXDownshift::PredictRPM(60 / 0.036f, 40, 4.25f, 2.81f, RPM));
	TestTrue(TEXT("60 km/h in first is about 4752 RPM"), FMath::IsNearlyEqual(RPM, 4751.8f, 2.0f));
	Input.SpeedCmS = 0; Input.EngineRPM = 900;
	TestEqual(TEXT("Standstill recovers first gear"), CXDownshift::ChooseGear(Input), 1);
	Input.SpeedCmS = 70 / 0.036f; Input.EngineRPM = 1300;
	TestEqual(TEXT("Skip down to safe second gear"), CXDownshift::ChooseGear(Input), 2);
	Input.SpeedCmS = 180 / 0.036f; Input.EngineRPM = 3354;
	TestEqual(TEXT("Hold gear at appropriate cruising RPM"), CXDownshift::ChooseGear(Input), 5);
	Input.CurrentGear = 2; Input.SpeedCmS = 80 / 0.036f; Input.EngineRPM = 1000;
	TestEqual(TEXT("Road speed prevents unsafe first gear"), CXDownshift::ChooseGear(Input), 2);
	Input.CurrentGear = 5; Input.SpeedCmS = 60 / 0.036f; Input.EngineRPM = 3000;
	TestEqual(TEXT("Actual RPM supplies additional over-rev guard"), CXDownshift::ChooseGear(Input), 3);
	Input.EngineRPM = 1118; Input.MaxRPM = 5000;
	TestEqual(TEXT("Lower redline changes safe choice"), CXDownshift::ChooseGear(Input), 2);
	Input.MaxRPM = 7000;
	for (float SpeedKmh : {0.0f, 10.0f, 30.0f, 60.0f, 90.0f, 120.0f, 180.0f, 240.0f})
	{
		for (int32 Gear = 2; Gear <= Ratios.Num(); ++Gear)
		{
			Input.CurrentGear = Gear; Input.SpeedCmS = SpeedKmh / 0.036f;
			CXDownshift::PredictRPM(Input.SpeedCmS, Input.WheelRadiusCm, Ratios[Gear - 1], Input.FinalRatio, RPM);
			Input.EngineRPM = FMath::Max(Input.IdleRPM, RPM);
			const int32 Chosen = CXDownshift::ChooseGear(Input);
			TestTrue(TEXT("Selector only holds or downshifts forward gears"), Chosen >= 1 && Chosen <= Gear);
			if (Chosen < Gear)
			{
				CXDownshift::PredictRPM(Input.SpeedCmS, Input.WheelRadiusCm, Ratios[Chosen - 1], Input.FinalRatio, RPM);
				TestTrue(TEXT("Selected gear has safe modeled RPM"), RPM <= 5400.1f);
			}
		}
	}
	Input.CurrentGear = 5; Input.SpeedCmS = 0; Input.EngineRPM = 900;
	Input.WheelRadiusCm = 0;
	TestEqual(TEXT("Reject zero wheel radius"), CXDownshift::ChooseGear(Input), 5);
	Input.WheelRadiusCm = 40; Input.FinalRatio = 0;
	TestEqual(TEXT("Reject zero final ratio"), CXDownshift::ChooseGear(Input), 5);
	Input.FinalRatio = 2.81f; Input.EngineRPM = std::numeric_limits<float>::quiet_NaN();
	TestEqual(TEXT("Reject NaN RPM"), CXDownshift::ChooseGear(Input), 5);
	Input.EngineRPM = 900; Input.SpeedCmS = -10;
	TestEqual(TEXT("Reject reverse road speed"), CXDownshift::ChooseGear(Input), 5);
	Input.SpeedCmS = 0; Input.CurrentGear = -1;
	TestEqual(TEXT("Do not replace reverse"), CXDownshift::ChooseGear(Input), -1);
	Input.CurrentGear = 0;
	TestEqual(TEXT("Do not interrupt neutral/shift"), CXDownshift::ChooseGear(Input), 0);
	const TArray<float> BadRatios {4.0f, 0.0f, 2.0f};
	Input.CurrentGear = 3; Input.ForwardRatios = BadRatios;
	TestEqual(TEXT("Reject invalid gear table"), CXDownshift::ChooseGear(Input), 3);
	const TArray<float> GR86Ratios {3.626f, 2.189f, 1.541f, 1.213f, 1.0f, 0.767f};
	Input.ForwardRatios = GR86Ratios; Input.CurrentGear = 6; Input.FinalRatio = 4.1f;
	Input.SpeedCmS = 60 / 0.036f; Input.WheelRadiusCm = 34; Input.EngineRPM = 1472;
	TestEqual(TEXT("Custom six-speed ratios and final drive are respected"), CXDownshift::ChooseGear(Input), 2);
	if (FParse::Param(FCommandLine::Get(), TEXT("CXDownshiftRuntimeTest")))
		FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCXDownshiftRuntimeCommand>(this));
	return true;
}
#endif
