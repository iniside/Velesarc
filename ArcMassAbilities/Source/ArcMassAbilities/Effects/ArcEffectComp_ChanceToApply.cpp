// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcEffectComp_ChanceToApply.h"

bool FArcEffectComp_ChanceToApply::OnPreApplication(const FArcEffectContext& Context) const
{
	return FMath::FRand() < Chance;
}
