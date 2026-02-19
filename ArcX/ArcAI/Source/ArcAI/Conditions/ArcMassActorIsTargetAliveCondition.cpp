// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassActorIsTargetAliveCondition.h"

#include "ArcHealthComponent.h"
#include "StateTreeExecutionContext.h"

bool FArcMassActorIsTargetAliveConditionCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	
	if (InstanceData.TargetInput == nullptr)
	{
		return false;
	}
	
	if (UArcHealthComponent* HealthComp = InstanceData.TargetInput->FindComponentByClass<UArcHealthComponent>())
	{
		if (HealthComp->GetDeathState() == EArcDeathState::NotDead)
		{
			return true;
		}
		
		return false;
	}
	
	if (InstanceData.TargetInput)
	{
		return true;
	}
	
	return false;
}
