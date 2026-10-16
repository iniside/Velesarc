// Copyright Lukasz Baran. All Rights Reserved.

#include "Items/Fragments/ArcItemFragment_Rarity.h"

#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_InstanceTags.h"

namespace ArcMassItems
{
	FGameplayTag GetSpecRarity(const FArcItemSpec& Spec)
	{
		const FArcItemFragment_Rarity* Fragment = Spec.FindFragment<FArcItemFragment_Rarity>();
		return Fragment ? Fragment->Rarity : FGameplayTag();
	}

	bool IsSameStackKey(const FArcItemSpec& A, const FArcItemSpec& B)
	{
		return A.GetItemDefinitionId() == B.GetItemDefinitionId()
			&& GetSpecRarity(A) == GetSpecRarity(B)
			&& GetSpecInstanceTags(A) == GetSpecInstanceTags(B);
	}
}
