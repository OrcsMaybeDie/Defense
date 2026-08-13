#pragma once

#include "Engine/EngineTypes.h"

namespace DefenseCollisionChannels
{
	inline constexpr ECollisionChannel Enemy = ECC_GameTraceChannel1;
	inline constexpr ECollisionChannel FootIK = ECC_GameTraceChannel2;
	inline constexpr ECollisionChannel Barricade = ECC_GameTraceChannel3;
	inline constexpr ECollisionChannel MedusaSight = ECC_GameTraceChannel4;
}
