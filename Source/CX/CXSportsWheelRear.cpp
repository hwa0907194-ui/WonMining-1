// Copyright Epic Games, Inc. All Rights Reserved.


#include "CXSportsWheelRear.h"

UCXSportsWheelRear::UCXSportsWheelRear()
{
	WheelRadius = 40.f;
	WheelWidth = 40.0f;
	// Retain normal driving grip, but let rear-wheel lock initiate a slide.
	FrictionForceMultiplier = 4.0f;
	SideSlipModifier = 0.7f;
	SlipThreshold = 100.0f;
	SkidThreshold = 100.0f;
	MaxSteerAngle = 0.0f;
	MaxHandBrakeTorque = 15000.0f;
}
