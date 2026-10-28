#include "CXDownshiftLogic.h"

bool CXDownshift::PredictRPM(float SpeedCmS, float WheelRadiusCm, float GearRatio, float FinalRatio, float& OutRPM)
{
	OutRPM = 0;
	if (!FMath::IsFinite(SpeedCmS) || SpeedCmS < 0 || !FMath::IsFinite(WheelRadiusCm) || WheelRadiusCm <= 0
		|| !FMath::IsFinite(GearRatio) || GearRatio <= 0 || !FMath::IsFinite(FinalRatio) || FinalRatio <= 0) return false;
	const double RPM = static_cast<double>(SpeedCmS) * 60.0 / (2.0 * PI * WheelRadiusCm) * GearRatio * FinalRatio;
	if (!FMath::IsFinite(RPM) || RPM > MAX_flt) return false;
	OutRPM = static_cast<float>(RPM);
	return true;
}

int32 CXDownshift::ChooseGear(const FCXDownshiftInput& Input)
{
	const int32 Current = Input.CurrentGear;
	if (Current <= 1 || Current > Input.ForwardRatios.Num()
		|| !FMath::IsFinite(Input.EngineRPM) || Input.EngineRPM < 0
		|| !FMath::IsFinite(Input.IdleRPM) || Input.IdleRPM <= 0
		|| !FMath::IsFinite(Input.MaxRPM) || Input.MaxRPM <= Input.IdleRPM
		|| !FMath::IsFinite(Input.ChangeDownRPM) || Input.ChangeDownRPM <= 0
		|| !FMath::IsFinite(Input.ChangeUpRPM) || Input.ChangeUpRPM <= Input.ChangeDownRPM
		|| !FMath::IsFinite(Input.UpshiftMarginRPM) || Input.UpshiftMarginRPM < 0
		|| !FMath::IsFinite(Input.RedlineFraction) || Input.RedlineFraction <= 0 || Input.RedlineFraction > 1) return Current;
	float PreviousRatio = MAX_flt;
	for (float Ratio : Input.ForwardRatios)
	{
		if (!FMath::IsFinite(Ratio) || Ratio <= 0 || Ratio >= PreviousRatio) return Current;
		PreviousRatio = Ratio;
	}
	float CurrentRoadRPM;
	if (!PredictRPM(Input.SpeedCmS, Input.WheelRadiusCm, Input.ForwardRatios[Current - 1], Input.FinalRatio, CurrentRoadRPM)) return Current;
	if (CurrentRoadRPM > Input.ChangeDownRPM && Input.EngineRPM > Input.ChangeDownRPM) return Current;
	// Leave room below the upshift point to avoid immediately shifting back up.
	const float UpperRPM = FMath::Min(Input.MaxRPM * Input.RedlineFraction,
		Input.ChangeUpRPM - FMath::Max(Input.UpshiftMarginRPM, Input.ChangeUpRPM * 0.1f));
	if (UpperRPM <= Input.ChangeDownRPM || Input.EngineRPM >= UpperRPM) return Current;
	for (int32 Gear = 1; Gear < Current; ++Gear)
	{
		float CandidateRPM;
		if (!PredictRPM(Input.SpeedCmS, Input.WheelRadiusCm, Input.ForwardRatios[Gear - 1], Input.FinalRatio, CandidateRPM)) return Current;
		// Road-speed prediction handles braking and locked/slowing wheels. When
		// coupled above idle, actual RPM provides an additional over-rev check.
		if (CurrentRoadRPM >= Input.IdleRPM)
			CandidateRPM = FMath::Max(CandidateRPM, Input.EngineRPM * Input.ForwardRatios[Gear - 1] / Input.ForwardRatios[Current - 1]);
		if (FMath::IsFinite(CandidateRPM) && CandidateRPM <= UpperRPM) return Gear;
	}
	return Current;
}
