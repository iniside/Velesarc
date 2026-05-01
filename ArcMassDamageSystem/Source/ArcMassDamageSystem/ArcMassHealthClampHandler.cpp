// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassHealthClampHandler.h"

#include "ArcMassHealthStatsFragment.h"

bool FArcMassHealthClampHandler::PreChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float& NewValue)
{
	FArcMassHealthStatsFragment* HealthFrag = static_cast<FArcMassHealthStatsFragment*>(static_cast<void*>(Context.OwningFragmentMemory));
	float MaxHealth = HealthFrag ? HealthFrag->GetMaxHealth() : 0.f;
	NewValue = FMath::Clamp(NewValue, 0.f, MaxHealth);
	return true;
}
