// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBurningConditionSet.h"

#include "Engine/World.h"

#include "ArcConditionEffectsSubsystem.h"
#include "MassAgentComponent.h"

UArcBurningConditionSet::UArcBurningConditionSet()
{
}

void UArcBurningConditionSet::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	if (Data.EvaluatedData.Attribute == GetBurningAddAttribute())
	{
		const float BurningToAdd = GetBurningAdd();
		SetBurningAdd(0);
		
		UArcConditionEffectsSubsystem* ConditionEffectsSubsystem = GetWorld()->GetSubsystem<UArcConditionEffectsSubsystem>();
		UMassAgentComponent* MassAgentComponent = GetOwningAbilitySystemComponent()->GetAvatarActor()->FindComponentByClass<UMassAgentComponent>();
		if (!MassAgentComponent)
		{
			return;	
		}
		
		FMassEntityHandle EntityHandle = MassAgentComponent->GetEntityHandle();
		if (EntityHandle.IsValid())
		{
			ConditionEffectsSubsystem->ApplyCondition(EntityHandle, EArcConditionType::Burning, BurningToAdd);
		}
		
		return;
	}
	
	if (Data.EvaluatedData.Attribute == GetBurningRemoveAttribute())
	{
		const float BurningToRemove = GetBurningRemove();
		SetBurningRemove(0);
		
		UArcConditionEffectsSubsystem* ConditionEffectsSubsystem = GetWorld()->GetSubsystem<UArcConditionEffectsSubsystem>();
		UMassAgentComponent* MassAgentComponent = GetOwningAbilitySystemComponent()->GetAvatarActor()->FindComponentByClass<UMassAgentComponent>();
		if (!MassAgentComponent)
		{
			return;	
		}
		
		FMassEntityHandle EntityHandle = MassAgentComponent->GetEntityHandle();
		if (EntityHandle.IsValid())
		{
			ConditionEffectsSubsystem->ApplyCondition(EntityHandle, EArcConditionType::Burning, -BurningToRemove);
		}
		
		return;
	}
}
