#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "CXEngineAudioComponent.generated.h"

struct FCXEngineSoundControls;

UCLASS(ClassGroup=Audio, meta=(BlueprintSpawnableComponent))
class CX_API UCXEngineAudioComponent : public USynthComponent
{
	GENERATED_BODY()
public:
	UCXEngineAudioComponent(const FObjectInitializer& ObjectInitializer);
	void SetEngineParameters(float RPM, float Load);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engine Sound") bool bEngineSoundEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engine Sound", meta=(ClampMin=0, ClampMax=1)) float EngineVolume = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engine Sound", meta=(ClampMin=2, ClampMax=12)) int32 CylinderCount = 4;
protected:
	virtual void BeginPlay() override;
	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;
private:
	TSharedPtr<FCXEngineSoundControls, ESPMode::ThreadSafe> Controls;
};
