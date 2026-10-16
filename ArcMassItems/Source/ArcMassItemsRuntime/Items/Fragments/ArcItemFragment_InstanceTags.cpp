// Copyright Lukasz Baran. All Rights Reserved.

#include "Items/Fragments/ArcItemFragment_InstanceTags.h"

#include "Items/ArcItemSpec.h"

namespace ArcMassItems
{
	const FGameplayTagContainer& GetSpecInstanceTags(const FArcItemSpec& Spec)
	{
		const FArcItemFragment_InstanceTags* Fragment = Spec.FindFragment<FArcItemFragment_InstanceTags>();
		return Fragment ? Fragment->Tags : FGameplayTagContainer::EmptyContainer;
	}
}
