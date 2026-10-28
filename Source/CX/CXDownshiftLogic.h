#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"

/** Physical units match Chaos: road speed in cm/s, wheel radius in cm. */
struct FCXDownshiftInput
{
	TConstArrayView<float> ForwardRatios;
	int32 CurrentGear = 0;
	float SpeedCmS = 0;
	float WheelRadiusCm = 0;
	float FinalRatio = 0;
	float EngineRPM = 0;
	float IdleRPM = 900;
	float MaxRPM = 7000;
	float ChangeDownRPM = 2000;
	float ChangeUpRPM = 6000;
	float UpshiftMarginRPM = 600;
	float RedlineFraction = 0.9f;
};

namespace CXDownshift
{
	bool PredictRPM(float SpeedCmS, float WheelRadiusCm, float GearRatio, float FinalRatio, float& OutRPM);
	/** Returns the current gear when no safe lower forward gear is appropriate. */
	int32 ChooseGear(const FCXDownshiftInput& Input);
}
