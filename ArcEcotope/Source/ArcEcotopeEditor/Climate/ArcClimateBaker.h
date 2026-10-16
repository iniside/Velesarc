// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AArcClimateVolume;

class ARCECOTOPEEDITOR_API FArcClimateBaker
{
public:
	static bool Bake(AArcClimateVolume& Volume, FText& OutError);
};
