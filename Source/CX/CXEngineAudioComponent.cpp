#include "CXEngineAudioComponent.h"
#include "CXEngineSoundGenerator.h"
#include "Engine/World.h"

UCXEngineAudioComponent::UCXEngineAudioComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), Controls(MakeShared<FCXEngineSoundControls, ESPMode::ThreadSafe>())
{
	NumChannels = 1;
	bAutoActivate = false;
	bAutoDestroy = false;
	bStopWhenOwnerDestroyed = true;
	bAllowSpatialization = true;
	bOverrideAttenuation = true;
	AttenuationOverrides.bAttenuate = true;
	AttenuationOverrides.bSpatialize = true;
	AttenuationOverrides.AttenuationShapeExtents = FVector(800.0f);
	AttenuationOverrides.FalloffDistance = 3500.0f;
	PrimaryComponentTick.bCanEverTick = false;
}

void UCXEngineAudioComponent::BeginPlay()
{
	Super::BeginPlay();
	SetEngineParameters(900.0f, 0.0f);
	if (GetNetMode() != NM_DedicatedServer) Start();
}

void UCXEngineAudioComponent::SetEngineParameters(float RPM, float Load)
{
	Controls->RPM.store(RPM, std::memory_order_relaxed);
	Controls->Load.store(Load, std::memory_order_relaxed);
	Controls->Volume.store(bEngineSoundEnabled ? EngineVolume : 0.0f, std::memory_order_relaxed);
	Controls->Cylinders.store(CylinderCount, std::memory_order_relaxed);
}

ISoundGeneratorPtr UCXEngineAudioComponent::CreateSoundGenerator(const FSoundGeneratorInitParams& InParams)
{
	return MakeShared<FCXEngineSoundGenerator, ESPMode::ThreadSafe>(InParams.SampleRate, Controls.ToSharedRef());
}
