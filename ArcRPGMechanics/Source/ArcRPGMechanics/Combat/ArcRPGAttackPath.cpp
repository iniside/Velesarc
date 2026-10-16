// Copyright Lukasz Baran. All Rights Reserved.

#include "Combat/ArcRPGAttackPath.h"

#include "ArcRPGMechanicsTags.h"

namespace ArcRPGAttackPath
{
	EArcRPGAttackPath Resolve(const FGameplayTagContainer& SourceTags)
	{
		const bool bSpell = SourceTags.HasTag(ArcRPGMechanicsTags::AttackClass_Spell);
		const bool bWeapon = SourceTags.HasTag(ArcRPGMechanicsTags::AttackClass_Weapon);
		if (bSpell == bWeapon)
		{
			return EArcRPGAttackPath::None;
		}
		return bSpell ? EArcRPGAttackPath::Spell : EArcRPGAttackPath::Weapon;
	}
}
