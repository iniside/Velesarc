// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcEffectComponent.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "NativeGameplayTags.h"
#include "ArcCombatTestTypes.generated.h"

class UArcEffectDefinition;

USTRUCT(meta = (Hidden))
struct ARCMASSMECHANICSTEST_API FArcMechTest_CombatSet : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute HealMeta;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMechTest_CombatSet, HealMeta)
};

namespace ArcCombatTestTags
{
	ARCMASSMECHANICSTEST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamagePhysical);
	ARCMASSMECHANICSTEST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageMagical);
	ARCMASSMECHANICSTEST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageSlash);
	ARCMASSMECHANICSTEST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageFire);
	ARCMASSMECHANICSTEST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttackMelee);
	ARCMASSMECHANICSTEST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttackMeleeHeavy);
	ARCMASSMECHANICSTEST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttackOther);
	ARCMASSMECHANICSTEST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Primer);
	ARCMASSMECHANICSTEST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Reaction);
}

template <>
struct TMassFragmentTraits<FArcMechTest_CombatSet> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
