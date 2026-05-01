// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectTypes.h"
#include "Attributes/ArcAggregator.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"
#include "ScalableFloat.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TagTest_Fire, "Test.Fire");

TEST_CLASS(ArcOwnedTags, "ArcMassAbilities.Tags")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();
	}

	TEST_METHOD(GrantTags_AddAndRemove)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FMassEntityView EntityView(*EntityManager, Entity);

		FArcOwnedTagsFragment* Tags = EntityView.GetFragmentDataPtr<FArcOwnedTagsFragment>();
		FGameplayTag FireTag = TAG_TagTest_Fire;

		Tags->Tags.AddTag(FireTag);
		ASSERT_THAT(IsTrue(Tags->Tags.HasTag(FireTag)));

		Tags->Tags.RemoveTag(FireTag);
		ASSERT_THAT(IsFalse(Tags->Tags.HasTag(FireTag)));
	}

	TEST_METHOD(EffectGrantedTags_AffectAggregatorQualification)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FMassEntityView EntityView(*EntityManager, Entity);

		FArcAggregatorFragment* AggFrag = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();

		FGameplayTagRequirements TargetReqs;
		TargetReqs.RequireTags.AddTag(TAG_TagTest_Fire);

		FArcAttributeRef HealthRef = FArcTestStatsFragment::GetHealthAttribute();
		FArcAggregator& Agg = AggFrag->FindOrAddAggregator(HealthRef);
		Agg.AddMod(EArcModifierOp::Add, 50.f, FMassEntityHandle(), nullptr, &TargetReqs);

		FArcOwnedTagsFragment* Tags = EntityView.GetFragmentDataPtr<FArcOwnedTagsFragment>();
		FArcAggregationContext CtxBefore{FMassEntityHandle(), Tags->Tags.GetTagContainer(), *EntityManager, 100.f};
		float ResultBefore = Agg.Evaluate(CtxBefore);
		ASSERT_THAT(IsNear(100.f, ResultBefore, 0.001f));

		UArcEffectDefinition* TagEffect = NewObject<UArcEffectDefinition>();
		TagEffect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;

		FArcEffectComp_GrantTags GrantTags;
		GrantTags.GrantedTags.AddTag(TAG_TagTest_Fire);
		TagEffect->Components.Add(FInstancedStruct::Make(GrantTags));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, TagEffect, FMassEntityHandle());

		FArcAggregationContext CtxAfter{FMassEntityHandle(), Tags->Tags.GetTagContainer(), *EntityManager, 100.f};
		float ResultAfter = Agg.Evaluate(CtxAfter);
		ASSERT_THAT(IsNear(150.f, ResultAfter, 0.001f));
	}

	TEST_METHOD(EffectRemoval_RevokesTag_DisqualifiesMod)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FMassEntityView EntityView(*EntityManager, Entity);

		FArcAggregatorFragment* AggFrag = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();

		FGameplayTagRequirements TargetReqs;
		TargetReqs.RequireTags.AddTag(TAG_TagTest_Fire);

		FArcAttributeRef HealthRef = FArcTestStatsFragment::GetHealthAttribute();
		FArcAggregator& Agg = AggFrag->FindOrAddAggregator(HealthRef);
		Agg.AddMod(EArcModifierOp::Add, 50.f, FMassEntityHandle(), nullptr, &TargetReqs);

		UArcEffectDefinition* TagEffect = NewObject<UArcEffectDefinition>();
		TagEffect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;

		FArcEffectComp_GrantTags GrantTags;
		GrantTags.GrantedTags.AddTag(TAG_TagTest_Fire);
		TagEffect->Components.Add(FInstancedStruct::Make(GrantTags));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, TagEffect, FMassEntityHandle());

		FArcOwnedTagsFragment* Tags = EntityView.GetFragmentDataPtr<FArcOwnedTagsFragment>();
		FArcAggregationContext Ctx{FMassEntityHandle(), Tags->Tags.GetTagContainer(), *EntityManager, 100.f};
		float ResultWithTag = Agg.Evaluate(Ctx);
		ASSERT_THAT(IsNear(150.f, ResultWithTag, 0.001f));

		ArcEffects::RemoveEffect(*EntityManager, Entity, TagEffect);

		float ResultAfterRemoval = Agg.Evaluate(Ctx);
		ASSERT_THAT(IsNear(100.f, ResultAfterRemoval, 0.001f));
	}
};
