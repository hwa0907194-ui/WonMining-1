#include "CXEngineSoundGenerator.h"

FCXEngineSoundGenerator::FCXEngineSoundGenerator(float InSampleRate, TSharedRef<FCXEngineSoundControls, ESPMode::ThreadSafe> InControls)
	: Controls(InControls)
	, SampleRate(FMath::IsFinite(InSampleRate) ? FMath::Clamp(InSampleRate, 8000.0f, 192000.0f) : 48000.0f)
	, RPMSmoothing(1.0f - FMath::Exp(-1.0f / (SampleRate * 0.08f)))
	, GainSmoothing(1.0f - FMath::Exp(-1.0f / (SampleRate * 0.025f)))
{
}

int32 FCXEngineSoundGenerator::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	if (!OutAudio || NumSamples <= 0) return 0;
	const float RawRPM = Controls->RPM.load(std::memory_order_relaxed);
	const float RawLoad = Controls->Load.load(std::memory_order_relaxed);
	const float RawVolume = Controls->Volume.load(std::memory_order_relaxed);
	const float RPM = FMath::IsFinite(RawRPM) ? FMath::Clamp(RawRPM, 400.0f, 20000.0f) : 900.0f;
	const float Load = FMath::IsFinite(RawLoad) ? FMath::Clamp(RawLoad, 0.0f, 1.0f) : 0.0f;
	const float Volume = FMath::IsFinite(RawVolume) ? FMath::Clamp(RawVolume, 0.0f, 1.0f) : 0.0f;
	const int32 Cylinders = FMath::Clamp(Controls->Cylinders.load(std::memory_order_relaxed), 2, 12);
	const float TargetGain = Volume * (0.12f + 0.17f * Load + 0.06f * FMath::Clamp(RPM / 7000.0f, 0.0f, 1.0f));
	const float NoiseAlpha = 1.0f - FMath::Exp(-2.0f * PI * 180.0f / SampleRate);
	// A darker, two-pole exhaust response removes the thin upper harmonics.
	const float ExhaustCutoff = 380.0f + 180.0f * Load + 120.0f * FMath::Clamp(RPM / 7000.0f, 0.0f, 1.0f);
	const float ExhaustAlpha = 1.0f - FMath::Exp(-2.0f * PI * ExhaustCutoff / SampleRate);
	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		SmoothedRPM += (RPM - SmoothedRPM) * RPMSmoothing;
		SmoothedGain += (TargetGain - SmoothedGain) * GainSmoothing;
		// Four-stroke firing frequency: RPM / 60 * cylinder count / 2.
		// Limit harmonics below Nyquist even with unusual editor settings.
		const float Frequency = FMath::Min(SmoothedRPM * Cylinders / 120.0f, SampleRate * 0.09f);
		Phase += 2.0f * PI * Frequency / SampleRate;
		if (Phase >= 2.0f * PI) Phase -= 2.0f * PI;
		// Independent phases keep subharmonics continuous across firing cycles.
		// The floor makes idle rumble audible rather than mostly subsonic.
		BassPhase += 2.0f * PI * FMath::Max(38.0f, Frequency * 0.25f) / SampleRate;
		BankPhase += 2.0f * PI * FMath::Max(55.0f, Frequency * 0.5f) / SampleRate;
		PulsePhase += 2.0f * PI * FMath::Min(SmoothedRPM / 120.0f, 32.0f) / SampleRate;
		if (BassPhase >= 2.0f * PI) BassPhase -= 2.0f * PI;
		if (BankPhase >= 2.0f * PI) BankPhase -= 2.0f * PI;
		if (PulsePhase >= 2.0f * PI) PulsePhase -= 2.0f * PI;
		NoiseState = NoiseState * 1664525u + 1013904223u;
		const float Noise = static_cast<float>(NoiseState >> 8) / 8388607.5f - 1.0f;
		NoiseFilter += NoiseAlpha * (Noise - NoiseFilter);
		const float Tone = 0.65f * FMath::Sin(BassPhase) + 0.28f * FMath::Sin(BankPhase + 0.2f)
			+ 0.35f * FMath::Sin(Phase) + 0.08f * FMath::Sin(2 * Phase + 0.3f)
			+ 0.025f * FMath::Sin(3 * Phase);
		const float Pulse = 0.9f + 0.1f * FMath::Sin(PulsePhase);
		const float Exhaust = Pulse * Tone + (0.06f + 0.1f * Load) * NoiseFilter;
		ExhaustFilter += ExhaustAlpha * (Exhaust - ExhaustFilter);
		ExhaustFilter2 += ExhaustAlpha * (ExhaustFilter - ExhaustFilter2);
		OutAudio[Index] = FMath::Clamp(SmoothedGain * ExhaustFilter2, -0.95f, 0.95f);
	}
	return NumSamples;
}
