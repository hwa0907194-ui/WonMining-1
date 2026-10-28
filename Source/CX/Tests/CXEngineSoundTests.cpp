#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../CXEngineSoundGenerator.h"
#include "../CXEngineAudioComponent.h"
#include "../CXPawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Audio.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXEngineSoundTest, "CX.Audio.EngineSynthesis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCXEngineSoundTest::RunTest(const FString& Parameters)
{
	constexpr int32 Rate = 48000;
	auto Controls = MakeShared<FCXEngineSoundControls, ESPMode::ThreadSafe>();
	auto Measure = [this, Controls](float RPM, float Load)
	{
		Controls->RPM.store(RPM); Controls->Load.store(Load); Controls->Volume.store(0.35f);
		FCXEngineSoundGenerator Generator(Rate, Controls);
		TArray<float> Buffer; Buffer.SetNumUninitialized(Rate * 2);
		Generator.OnGenerateAudio(Buffer.GetData(), Buffer.Num());
		double Energy = 0;
		bool bFinite = true, bBounded = true;
		for (int32 Index = Rate; Index < Buffer.Num(); ++Index)
		{
			bFinite &= FMath::IsFinite(Buffer[Index]);
			bBounded &= FMath::Abs(Buffer[Index]) <= 0.95f;
			Energy += Buffer[Index] * Buffer[Index];
		}
		TestTrue(TEXT("Finite output"), bFinite);
		TestTrue(TEXT("No clipping"), bBounded);
		TestTrue(TEXT("Audible nonzero signal"), Energy / Rate > 0.00001);
		return Buffer;
	};
	const TArray<float> Idle = Measure(900, 0);
	const TArray<float> High = Measure(6000, 1);
	auto SpectralEnergy = [](const TArray<float>& Audio, float Hertz)
	{
		double Real = 0, Imaginary = 0;
		for (int32 Index = Rate; Index < Audio.Num(); ++Index)
		{
			const double Angle = 2.0 * PI * Hertz * (Index - Rate) / Rate;
			Real += Audio[Index] * FMath::Cos(Angle);
			Imaginary += Audio[Index] * FMath::Sin(Angle);
		}
		return (Real * Real + Imaginary * Imaginary) / (static_cast<double>(Rate) * Rate);
	};
	TestTrue(TEXT("High RPM retains the 200 Hz four-cylinder firing tone"), SpectralEnergy(High, 200) > SpectralEnergy(Idle, 200) * 10);
	TestTrue(TEXT("Low exhaust body dominates upper harmonic"), SpectralEnergy(High, 50) > SpectralEnergy(High, 400) * 8);
	FCXEngineSoundGenerator Generator(Rate, Controls);
	TArray<float> Buffer; Buffer.SetNumUninitialized(Rate);
	Generator.OnGenerateAudio(Buffer.GetData(), Buffer.Num());
	Controls->Volume.store(0);
	Generator.OnGenerateAudio(Buffer.GetData(), Buffer.Num());
	bool bMuted = true;
	for (int32 Index = Rate / 2; Index < Rate; ++Index) bMuted &= FMath::Abs(Buffer[Index]) < 0.00001f;
	TestTrue(TEXT("Mute fades to silence"), bMuted);
	Controls->RPM.store(std::numeric_limits<float>::quiet_NaN());
	Controls->Load.store(std::numeric_limits<float>::infinity());
	Controls->Volume.store(1);
	Generator.OnGenerateAudio(Buffer.GetData(), Buffer.Num());
	bool bSafe = true;
	for (float Sample : Buffer) bSafe &= FMath::IsFinite(Sample) && FMath::Abs(Sample) <= 0.95f;
	TestTrue(TEXT("Bad parameters do not poison audio"), bSafe);
	TestEqual(TEXT("Empty buffer is safe"), Generator.OnGenerateAudio(nullptr, 0), 0);
	if (FParse::Param(FCommandLine::Get(), TEXT("CXAudioDeviceTest")))
	{
		bool bPlaying = false;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || !World->IsGameWorld()) continue;
			for (TActorIterator<ACXPawn> It(World); It; ++It)
			{
				const UCXEngineAudioComponent* Audio = It->FindComponentByClass<UCXEngineAudioComponent>();
				bPlaying |= Audio && Audio->IsPlaying();
			}
		}
		TestTrue(TEXT("Live vehicle synth started on audio device"), bPlaying);
	}

	FString PreviewPath;
	if (FParse::Value(FCommandLine::Get(), TEXT("CXAudioPreview="), PreviewPath))
	{
		Controls->Cylinders.store(4); Controls->Volume.store(0.6f);
		FCXEngineSoundGenerator Preview(Rate, Controls);
		TArray<int16> PCM;
		constexpr int32 Block = 480;
		TArray<float> Audio; Audio.SetNumUninitialized(Block);
		for (int32 Frame = 0; Frame < Rate * 8; Frame += Block)
		{
			const float Time = static_cast<float>(Frame) / Rate;
			float RPM = 900;
			if (Time >= 1 && Time < 4) RPM = FMath::Lerp(900.0f, 6500.0f, (Time - 1) / 3);
			else if (Time >= 4 && Time < 6) RPM = FMath::Lerp(3800.0f, 6500.0f, (Time - 4) / 2);
			else if (Time >= 6) RPM = FMath::Lerp(6500.0f, 900.0f, (Time - 6) / 2);
			Controls->RPM.store(RPM); Controls->Load.store(Time >= 1 && Time < 6 ? 1.0f : 0.0f);
			Preview.OnGenerateAudio(Audio.GetData(), Block);
			for (float Sample : Audio) PCM.Add(static_cast<int16>(Sample * 32767));
		}
		TArray<uint8> Wave;
		SerializeWaveFile(Wave, reinterpret_cast<const uint8*>(PCM.GetData()), PCM.Num() * sizeof(int16), 1, Rate);
		TestTrue(TEXT("Write preview WAV"), FFileHelper::SaveArrayToFile(Wave, *PreviewPath));
	}
	return true;
}
#endif
