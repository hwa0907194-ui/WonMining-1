#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundGenerator.h"
#include <atomic>

/** Shared numeric controls only: the audio renderer never accesses a UObject. */
struct FCXEngineSoundControls
{
	std::atomic<float> RPM { 900.0f };
	std::atomic<float> Load { 0.0f };
	std::atomic<float> Volume { 0.35f };
	std::atomic<int32> Cylinders { 4 };
};

/** A lightweight, original synthesized engine tone; no recorded sample required. */
class FCXEngineSoundGenerator final : public ISoundGenerator
{
public:
	FCXEngineSoundGenerator(float InSampleRate, TSharedRef<FCXEngineSoundControls, ESPMode::ThreadSafe> InControls);
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;
private:
	TSharedRef<FCXEngineSoundControls, ESPMode::ThreadSafe> Controls;
	float SampleRate;
	float RPMSmoothing;
	float GainSmoothing;
	float SmoothedRPM = 900.0f;
	float SmoothedGain = 0.0f;
	float Phase = 0.0f;
	float BassPhase = 0.0f;
	float BankPhase = 0.0f;
	float PulsePhase = 0.0f;
	float NoiseFilter = 0.0f;
	float ExhaustFilter = 0.0f;
	float ExhaustFilter2 = 0.0f;
	uint32 NoiseState = 0x4358454e;
};
