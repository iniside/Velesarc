// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Attributes/ArcAttribute.h"
#include "GameplayTagContainer.h"

class UArcAbilityDefinition;

struct FArcPendingAttributeEvent
{
	FMassEntityHandle Entity;
	FArcAttributeRef Attribute;
	float OldValue;
	float NewValue;
};

struct FArcPendingTagEvent
{
	FMassEntityHandle Entity;
	FGameplayTag Tag;
	int32 OldCount;
	int32 NewCount;
};

struct FArcPendingAbilityEvent
{
	FMassEntityHandle Entity;
	UArcAbilityDefinition* Ability;
};
