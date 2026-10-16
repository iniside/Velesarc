// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

enum class EArcRPGAttackPath : uint8
{
	None,
	Weapon,
	Spell
};

namespace ArcRPGAttackPath
{
	ARCRPGMECHANICS_API EArcRPGAttackPath Resolve(const FGameplayTagContainer& SourceTags);
}
