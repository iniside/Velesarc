// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassDamageToHealthHandler.h"

#include "ArcMassHealthStatsFragment.h"
#include "MassEntityView.h"

bool FArcMassDamageToHealthHandler::PreChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float& NewValue)
{
	if (NewValue < 0.f)
	{
		NewValue = 0.f;
	}
	return true;
}

void FArcMassDamageToHealthHandler::PostExecute(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue)
{
	if (NewValue <= 0.f)
	{
		return;
	}

	FMassEntityView EntityView(Context.EntityManager, Context.Entity);
	FArcMassHealthStatsFragment* HealthFrag = EntityView.GetFragmentDataPtr<FArcMassHealthStatsFragment>();
	if (HealthFrag)
	{
		HealthFrag->SetHealth(HealthFrag->GetHealth() - NewValue);
	}

	Attribute.SetBaseValue(0.f);
}
