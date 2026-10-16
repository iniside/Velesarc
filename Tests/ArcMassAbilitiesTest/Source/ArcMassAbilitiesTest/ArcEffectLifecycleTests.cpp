// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcEffectLifecycleFixture.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Effects/ArcEffectComp_GrantImmunity.h"
#include "Events/ArcGameEventTags.h"
#include "NativeGameplayTags.h"

namespace LC = ArcEffectLifecycleTest;

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Lifecycle_Eff, "Test.ArcLifecycle.Eff");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Lifecycle_Fire, "Test.ArcLifecycle.Eff.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Lifecycle_Ice, "Test.ArcLifecycle.Eff.Ice");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Lifecycle_Plain, "Test.ArcLifecycle.Eff.Plain");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Lifecycle_FrostVeto, "Test.ArcLifecycle.Veto.Frost");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Lifecycle_Seed, "Test.ArcLifecycle.Seed");

// ============================================================================
// Removal reasons and hooks (plan step 3)
// ============================================================================

TEST_CLASS(ArcEffectLifecycle_RemovalReasons, "ArcMassAbilities.Effects.Lifecycle.RemovalReasons")
{
	FArcEffectLifecycleFixture Fix;

	BEFORE_EACH()
	{
		Fix.Setup();
	}

	TEST_METHOD(Removed_QueryRemoval_ReportsRemoved_TasksBeforeComponents)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 10.f);
		Fix.AddRecorder(Definition, TEXT("A"));
		Fix.AddTaskRecorder(Definition, TEXT("A"));

		const FArcEffectApplyResult Applied = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		ASSERT_THAT(IsTrue(Applied.Handle.IsValid()));
		ASSERT_THAT(AreEqual(1, Fix.CountMods(Target, FArcAttributeSet_TestStats::GetArmorAttribute())));

		FArcEffectRemovalQuery Query;
		Query.Handle = Applied.Handle;
		ASSERT_THAT(AreEqual(1, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, Query)));

		const LC::FHookRecord* ComponentRecord = Fix.FindHook(LC::EHookKind::Removed, TEXT("A"));
		const LC::FHookRecord* TaskRecord = Fix.FindHook(LC::EHookKind::TaskRemoved, TEXT("A"));
		ASSERT_THAT(IsNotNull(ComponentRecord));
		ASSERT_THAT(IsNotNull(TaskRecord));
		ASSERT_THAT(IsTrue(ComponentRecord->Reason == EArcEffectRemovalReason::Removed));
		ASSERT_THAT(IsTrue(TaskRecord->Reason == EArcEffectRemovalReason::Removed));
		ASSERT_THAT(IsTrue(ComponentRecord->Handle == Applied.Handle));
		ASSERT_THAT(IsTrue(Fix.IndexOfHook(LC::EHookKind::TaskRemoved, TEXT("A")) < Fix.IndexOfHook(LC::EHookKind::Removed, TEXT("A"))));
		ASSERT_THAT(AreEqual(0, Fix.NumActive(Target)));
		ASSERT_THAT(AreEqual(0, Fix.CountMods(Target, FArcAttributeSet_TestStats::GetArmorAttribute())));
	}

	TEST_METHOD(Removed_QueryReason_IsForwardedToHooks)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 10.f);
		Fix.AddRecorder(Definition, TEXT("A"));
		Fix.AddTaskRecorder(Definition, TEXT("A"));
		ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());

		FArcEffectRemovalQuery Query;
		Query.Definition = Definition;
		Query.Reason = EArcEffectRemovalReason::EntityDestroyed;
		ASSERT_THAT(AreEqual(1, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, Query)));

		const LC::FHookRecord* ComponentRecord = Fix.FindHook(LC::EHookKind::Removed, TEXT("A"));
		const LC::FHookRecord* TaskRecord = Fix.FindHook(LC::EHookKind::TaskRemoved, TEXT("A"));
		ASSERT_THAT(IsNotNull(ComponentRecord));
		ASSERT_THAT(IsNotNull(TaskRecord));
		ASSERT_THAT(IsTrue(ComponentRecord->Reason == EArcEffectRemovalReason::EntityDestroyed));
		ASSERT_THAT(IsTrue(TaskRecord->Reason == EArcEffectRemovalReason::EntityDestroyed));
	}

	TEST_METHOD(Expired_DurationProcessor_ReportsExpired_RunsTaskAndComponentHooks)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Duration, 5.f, 10.f);
		Fix.AddRecorder(Definition, TEXT("A"));
		Fix.AddTaskRecorder(Definition, TEXT("A"));

		const FArcEffectApplyResult Applied = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		ASSERT_THAT(IsTrue(Applied.Handle.IsValid()));

		Fix.TickDuration(2.f);
		ASSERT_THAT(AreEqual(1, Fix.NumActive(Target)));
		ASSERT_THAT(AreEqual(0, Fix.CountHooks(LC::EHookKind::Removed, TEXT("A"))));

		Fix.TickDuration(4.f);
		ASSERT_THAT(AreEqual(0, Fix.NumActive(Target)));

		const LC::FHookRecord* ComponentRecord = Fix.FindHook(LC::EHookKind::Removed, TEXT("A"));
		const LC::FHookRecord* TaskRecord = Fix.FindHook(LC::EHookKind::TaskRemoved, TEXT("A"));
		ASSERT_THAT(IsNotNull(ComponentRecord));
		ASSERT_THAT(IsNotNull(TaskRecord));
		ASSERT_THAT(IsTrue(ComponentRecord->Reason == EArcEffectRemovalReason::Expired));
		ASSERT_THAT(IsTrue(TaskRecord->Reason == EArcEffectRemovalReason::Expired));
		ASSERT_THAT(IsTrue(ComponentRecord->Handle == Applied.Handle));
		ASSERT_THAT(IsTrue(Fix.IndexOfHook(LC::EHookKind::TaskRemoved, TEXT("A")) < Fix.IndexOfHook(LC::EHookKind::Removed, TEXT("A"))));
		ASSERT_THAT(AreEqual(0, Fix.CountMods(Target, FArcAttributeSet_TestStats::GetArmorAttribute())));
	}

	TEST_METHOD(Replaced_ReplacePolicy_ReportsReplacedForOldEffect)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Duration, 5.f, 10.f);
		Definition->StackingPolicy.StackPolicy = EArcStackPolicy::Replace;
		Fix.AddRecorder(Definition, TEXT("A"));
		Fix.AddTaskRecorder(Definition, TEXT("A"));

		const FArcEffectApplyResult First = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		const FArcEffectApplyResult Second = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		ASSERT_THAT(IsTrue(First.Handle.IsValid()));
		ASSERT_THAT(IsTrue(Second.Handle.IsValid()));
		ASSERT_THAT(IsTrue(First.Handle != Second.Handle));

		ASSERT_THAT(AreEqual(1, Fix.NumActive(Target)));
		ASSERT_THAT(IsNull(Fix.FindActive(Target, First.Handle)));
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, Second.Handle)));

		ASSERT_THAT(AreEqual(1, Fix.CountHooks(LC::EHookKind::Removed, TEXT("A"))));
		ASSERT_THAT(AreEqual(1, Fix.CountHooks(LC::EHookKind::TaskRemoved, TEXT("A"))));
		const LC::FHookRecord* ComponentRecord = Fix.FindHook(LC::EHookKind::Removed, TEXT("A"));
		const LC::FHookRecord* TaskRecord = Fix.FindHook(LC::EHookKind::TaskRemoved, TEXT("A"));
		ASSERT_THAT(IsTrue(ComponentRecord->Reason == EArcEffectRemovalReason::Replaced));
		ASSERT_THAT(IsTrue(TaskRecord->Reason == EArcEffectRemovalReason::Replaced));
		ASSERT_THAT(IsTrue(ComponentRecord->Handle == First.Handle));
	}

	TEST_METHOD(Overflow_RemoveOldestPolicy_ReportsOverflowForOldest)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 10.f);
		Definition->StackingPolicy.StackType = EArcStackType::Instance;
		Definition->StackingPolicy.MaxStacks = 2;
		Definition->StackingPolicy.OverflowPolicy = EArcStackOverflowPolicy::RemoveOldest;
		Fix.AddRecorder(Definition, TEXT("A"));

		const FArcEffectApplyResult First = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		const FArcEffectApplyResult Second = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		const FArcEffectApplyResult Third = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		ASSERT_THAT(IsTrue(Third.Handle.IsValid()));

		ASSERT_THAT(AreEqual(2, Fix.NumActive(Target)));
		ASSERT_THAT(IsNull(Fix.FindActive(Target, First.Handle)));
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, Second.Handle)));
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, Third.Handle)));

		ASSERT_THAT(AreEqual(1, Fix.CountHooks(LC::EHookKind::Removed, TEXT("A"))));
		const LC::FHookRecord* ComponentRecord = Fix.FindHook(LC::EHookKind::Removed, TEXT("A"));
		ASSERT_THAT(IsTrue(ComponentRecord->Reason == EArcEffectRemovalReason::Overflow));
		ASSERT_THAT(IsTrue(ComponentRecord->Handle == First.Handle));
	}

	TEST_METHOD(Invalid_NullSpecStrip_RunsTaskHooksAndLeavesLiveEffects)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Duration, 100.f, 10.f);
		Fix.AddRecorder(Definition, TEXT("Live"));
		const FArcEffectApplyResult Live = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		ASSERT_THAT(IsTrue(Live.Handle.IsValid()));

		FArcTestTask_RemovalRecorder NullTask;
		NullTask.Label = TEXT("Null");
		const FArcActiveEffectHandle NullHandle = FArcActiveEffectHandle::Generate();
		FArcEffectStackFragment* Stack = Fix.GetStack(Target);
		FArcActiveEffect& NullEffect = Stack->ActiveEffects.AddDefaulted_GetRef();
		NullEffect.Handle = NullHandle;
		NullEffect.OwnerEntity = Target;
		NullEffect.Tasks.Add(FInstancedStruct::Make(NullTask));
		ASSERT_THAT(AreEqual(2, Fix.NumActive(Target)));

		Fix.TickDuration(0.1f);

		ASSERT_THAT(AreEqual(1, Fix.NumActive(Target)));
		ASSERT_THAT(IsNull(Fix.FindActive(Target, NullHandle)));
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, Live.Handle)));
		ASSERT_THAT(AreEqual(1, Fix.CountHooks(LC::EHookKind::TaskRemoved, TEXT("Null"))));
		const LC::FHookRecord* TaskRecord = Fix.FindHook(LC::EHookKind::TaskRemoved, TEXT("Null"));
		ASSERT_THAT(IsTrue(TaskRecord->Reason == EArcEffectRemovalReason::Invalid));
		ASSERT_THAT(IsTrue(TaskRecord->Handle == NullHandle));
		ASSERT_THAT(AreEqual(0, Fix.CountHooks(LC::EHookKind::Removed, TEXT("Live"))));
	}

	TEST_METHOD(RemovalContext_CarriesSourceAbility_OnQueryRemovalAndExpiry)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcAbilityDefinition* Ability = NewObject<UArcAbilityDefinition>();
		const FArcAbilityHandle AbilityHandle = FArcAbilityHandle::Make(3, 7);

		FArcEffectContext Context;
		Context.SourceAbility = Ability;
		Context.SourceAbilityHandle = AbilityHandle;

		UArcEffectDefinition* Queried = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 10.f);
		Fix.AddRecorder(Queried, TEXT("Queried"));
		UArcEffectDefinition* Expiring = Fix.MakeDefinition(EArcEffectDuration::Duration, 1.f, 10.f);
		Fix.AddRecorder(Expiring, TEXT("Expiring"));

		const FArcEffectApplyResult QueriedResult = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Queried, Context);
		const FArcEffectApplyResult ExpiringResult = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Expiring, Context);
		ASSERT_THAT(IsTrue(QueriedResult.Handle.IsValid()));
		ASSERT_THAT(IsTrue(ExpiringResult.Handle.IsValid()));

		FArcEffectRemovalQuery Query;
		Query.Handle = QueriedResult.Handle;
		ArcEffects::RemoveEffects(*Fix.EntityManager, Target, Query);
		Fix.TickDuration(2.f);

		const LC::FHookRecord* QueriedRecord = Fix.FindHook(LC::EHookKind::Removed, TEXT("Queried"));
		const LC::FHookRecord* ExpiredRecord = Fix.FindHook(LC::EHookKind::Removed, TEXT("Expiring"));
		ASSERT_THAT(IsNotNull(QueriedRecord));
		ASSERT_THAT(IsNotNull(ExpiredRecord));
		ASSERT_THAT(IsTrue(QueriedRecord->SourceAbility == Ability));
		ASSERT_THAT(IsTrue(QueriedRecord->SourceAbilityHandle == AbilityHandle));
		ASSERT_THAT(IsTrue(ExpiredRecord->SourceAbility == Ability));
		ASSERT_THAT(IsTrue(ExpiredRecord->SourceAbilityHandle == AbilityHandle));
	}

	TEST_METHOD(Overflow_RemoveOldest_UsesApplicationOrder_AfterEarlierRemovalAndReorder)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 10.f);
		Definition->StackingPolicy.StackType = EArcStackType::Instance;
		Definition->StackingPolicy.MaxStacks = 3;
		Definition->StackingPolicy.OverflowPolicy = EArcStackOverflowPolicy::RemoveOldest;
		Fix.AddRecorder(Definition, TEXT("O"));

		const FArcEffectApplyResult One = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		const FArcEffectApplyResult Two = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		const FArcEffectApplyResult Three = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());

		FArcEffectRemovalQuery EarlierRemoval;
		EarlierRemoval.Handle = One.Handle;
		ASSERT_THAT(AreEqual(1, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, EarlierRemoval)));

		const FArcEffectApplyResult Four = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		ASSERT_THAT(AreEqual(3, Fix.NumActive(Target)));

		Fix.GetStack(Target)->ActiveEffects.Swap(0, 2);
		ASSERT_THAT(IsTrue(Fix.GetStack(Target)->ActiveEffects[0].Handle == Four.Handle));

		const FArcEffectApplyResult Five = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		ASSERT_THAT(IsTrue(Five.Handle.IsValid()));

		ASSERT_THAT(AreEqual(3, Fix.NumActive(Target)));
		ASSERT_THAT(IsNull(Fix.FindActive(Target, Two.Handle)));
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, Three.Handle)));
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, Four.Handle)));
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, Five.Handle)));

		int32 OverflowRecords = 0;
		for (const LC::FHookRecord& Record : LC::GetHookLog())
		{
			if (Record.Kind == LC::EHookKind::Removed && Record.Reason == EArcEffectRemovalReason::Overflow)
			{
				++OverflowRecords;
				ASSERT_THAT(IsTrue(Record.Handle == Two.Handle));
			}
		}
		ASSERT_THAT(AreEqual(1, OverflowRecords));
	}

	TEST_METHOD(RemoveEffects_DoesNotRemoveItself)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();

		UArcEffectDefinition* Other = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 10.f);
		Fix.AddIdentity(Other, TAG_Lifecycle_Fire);
		Fix.AddRecorder(Other, TEXT("Other"));

		UArcEffectDefinition* Remover = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 5.f);
		Fix.AddIdentity(Remover, TAG_Lifecycle_Fire);
		FArcEffectComp_RemoveEffects RemoveByTag;
		RemoveByTag.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Lifecycle_Fire));
		Fix.AddComponent(Remover, RemoveByTag);

		const FArcEffectApplyResult OtherResult = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Other, FMassEntityHandle());
		ASSERT_THAT(IsTrue(OtherResult.Handle.IsValid()));

		const FArcEffectApplyResult RemoverResult = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Remover, FMassEntityHandle());
		ASSERT_THAT(IsTrue(RemoverResult.bApplied));
		ASSERT_THAT(IsTrue(RemoverResult.Handle.IsValid()));

		ASSERT_THAT(AreEqual(1, Fix.NumActive(Target)));
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, RemoverResult.Handle)));
		ASSERT_THAT(IsNull(Fix.FindActive(Target, OtherResult.Handle)));
		ASSERT_THAT(AreEqual(1, Fix.CountHooks(LC::EHookKind::Removed, TEXT("Other"))));
	}
};

// ============================================================================
// RemoveEffects predicates, MaxCount and order (plan step 3)
// ============================================================================

TEST_CLASS(ArcEffectLifecycle_RemovalQuery, "ArcMassAbilities.Effects.Lifecycle.RemovalQuery")
{
	FArcEffectLifecycleFixture Fix;
	FMassEntityHandle Target;
	FMassEntityHandle SourceA;
	FMassEntityHandle SourceB;
	UArcEffectDefinition* FireDefinition = nullptr;
	UArcEffectDefinition* IceDefinition = nullptr;
	FArcActiveEffectHandle FireFromA1;
	FArcActiveEffectHandle FireFromB;
	FArcActiveEffectHandle IceFromA;
	FArcActiveEffectHandle FireFromA2;

	BEFORE_EACH()
	{
		Fix.Setup();
		Target = Fix.MakeEntity();
		SourceA = Fix.MakeEntity();
		SourceB = Fix.MakeEntity();

		FireDefinition = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 10.f);
		FireDefinition->StackingPolicy.StackType = EArcStackType::Instance;
		FireDefinition->StackingPolicy.MaxStacks = 4;
		Fix.AddIdentity(FireDefinition, TAG_Lifecycle_Fire);
		Fix.AddRecorder(FireDefinition, TEXT("Fire"));

		IceDefinition = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 10.f);
		Fix.AddIdentity(IceDefinition, TAG_Lifecycle_Ice);

		FireFromA1 = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, FireDefinition, SourceA).Handle;
		FireFromB = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, FireDefinition, SourceB).Handle;
		IceFromA = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, IceDefinition, SourceA).Handle;
		FireFromA2 = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, FireDefinition, SourceA).Handle;
	}

	bool Populated()
	{
		return FireFromA1.IsValid() && FireFromB.IsValid() && IceFromA.IsValid() && FireFromA2.IsValid()
			&& Fix.NumActive(Target) == 4;
	}

	TEST_METHOD(Definition_RemovesOnlyThatDefinition)
	{
		ASSERT_THAT(IsTrue(Populated()));

		FArcEffectRemovalQuery Query;
		Query.Definition = FireDefinition;
		ASSERT_THAT(AreEqual(3, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, Query)));

		ASSERT_THAT(AreEqual(1, Fix.NumActive(Target)));
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, IceFromA)));
	}

	TEST_METHOD(Source_RemovesOnlyThatSource)
	{
		ASSERT_THAT(IsTrue(Populated()));

		FArcEffectRemovalQuery Query;
		Query.Source = SourceA;
		ASSERT_THAT(AreEqual(3, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, Query)));

		ASSERT_THAT(AreEqual(1, Fix.NumActive(Target)));
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, FireFromB)));
	}

	TEST_METHOD(Handle_RemovesOnlyThatHandle)
	{
		ASSERT_THAT(IsTrue(Populated()));

		FArcEffectRemovalQuery Query;
		Query.Handle = FireFromB;
		ASSERT_THAT(AreEqual(1, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, Query)));

		ASSERT_THAT(AreEqual(3, Fix.NumActive(Target)));
		ASSERT_THAT(IsNull(Fix.FindActive(Target, FireFromB)));
	}

	TEST_METHOD(TagQuery_MatchesExactIdentityTag)
	{
		ASSERT_THAT(IsTrue(Populated()));

		FArcEffectRemovalQuery Query;
		Query.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Lifecycle_Ice));
		ASSERT_THAT(AreEqual(1, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, Query)));

		ASSERT_THAT(AreEqual(3, Fix.NumActive(Target)));
		ASSERT_THAT(IsNull(Fix.FindActive(Target, IceFromA)));
	}

	TEST_METHOD(TagQuery_MatchesParentTagOfIdentityTags)
	{
		ASSERT_THAT(IsTrue(Populated()));

		FArcEffectRemovalQuery Query;
		Query.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Lifecycle_Eff));
		ASSERT_THAT(AreEqual(4, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, Query)));
		ASSERT_THAT(AreEqual(0, Fix.NumActive(Target)));
	}

	TEST_METHOD(TagQuery_NonMatchingTag_RemovesNothing)
	{
		ASSERT_THAT(IsTrue(Populated()));

		FArcEffectRemovalQuery Query;
		Query.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Lifecycle_Plain));
		ASSERT_THAT(AreEqual(0, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, Query)));
		ASSERT_THAT(AreEqual(4, Fix.NumActive(Target)));
	}

	TEST_METHOD(Predicates_AreConjunctive)
	{
		ASSERT_THAT(IsTrue(Populated()));

		FArcEffectRemovalQuery WrongSource;
		WrongSource.Definition = IceDefinition;
		WrongSource.Source = SourceB;
		ASSERT_THAT(AreEqual(0, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, WrongSource)));

		FArcEffectRemovalQuery WrongDefinition;
		WrongDefinition.Definition = IceDefinition;
		WrongDefinition.Handle = FireFromA1;
		ASSERT_THAT(AreEqual(0, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, WrongDefinition)));

		FArcEffectRemovalQuery WrongTag;
		WrongTag.Source = SourceB;
		WrongTag.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Lifecycle_Ice));
		ASSERT_THAT(AreEqual(0, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, WrongTag)));
		ASSERT_THAT(AreEqual(4, Fix.NumActive(Target)));

		FArcEffectRemovalQuery Matching;
		Matching.Definition = FireDefinition;
		Matching.Source = SourceA;
		ASSERT_THAT(AreEqual(2, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, Matching)));
		ASSERT_THAT(AreEqual(2, Fix.NumActive(Target)));
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, FireFromB)));
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, IceFromA)));
	}

	TEST_METHOD(All_RemovesEveryEffect)
	{
		ASSERT_THAT(IsTrue(Populated()));

		FArcEffectRemovalQuery Query;
		Query.bAll = true;
		ASSERT_THAT(AreEqual(4, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, Query)));
		ASSERT_THAT(AreEqual(0, Fix.NumActive(Target)));
		ASSERT_THAT(AreEqual(0, Fix.CountMods(Target, FArcAttributeSet_TestStats::GetArmorAttribute())));
	}

	TEST_METHOD(AllWithExcludeHandle_SparesTheExcludedEffect)
	{
		ASSERT_THAT(IsTrue(Populated()));

		FArcEffectRemovalQuery Query;
		Query.bAll = true;
		Query.ExcludeHandle = IceFromA;
		ASSERT_THAT(AreEqual(3, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, Query)));
		ASSERT_THAT(AreEqual(1, Fix.NumActive(Target)));
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, IceFromA)));
	}

	TEST_METHOD(MaxCount_LimitsNumberOfRemovals)
	{
		ASSERT_THAT(IsTrue(Populated()));

		FArcEffectRemovalQuery Query;
		Query.Definition = FireDefinition;
		Query.MaxCount = 2;
		ASSERT_THAT(AreEqual(2, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, Query)));
		ASSERT_THAT(AreEqual(2, Fix.NumActive(Target)));
		ASSERT_THAT(AreEqual(2, Fix.CountHooks(LC::EHookKind::Removed, TEXT("Fire"))));
	}

	TEST_METHOD(Order_OldestFirst_RemovesOldestMatch)
	{
		ASSERT_THAT(IsTrue(Populated()));

		FArcEffectRemovalQuery Query;
		Query.Definition = FireDefinition;
		Query.MaxCount = 1;
		Query.Order = EArcEffectRemovalOrder::OldestFirst;
		ASSERT_THAT(AreEqual(1, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, Query)));

		ASSERT_THAT(IsNull(Fix.FindActive(Target, FireFromA1)));
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, FireFromB)));
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, FireFromA2)));
	}

	TEST_METHOD(Order_NewestFirst_RemovesNewestMatch)
	{
		ASSERT_THAT(IsTrue(Populated()));

		FArcEffectRemovalQuery Query;
		Query.Definition = FireDefinition;
		Query.MaxCount = 1;
		Query.Order = EArcEffectRemovalOrder::NewestFirst;
		ASSERT_THAT(AreEqual(1, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, Query)));

		ASSERT_THAT(IsNull(Fix.FindActive(Target, FireFromA2)));
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, FireFromA1)));
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, FireFromB)));
	}

	TEST_METHOD(Order_FollowsApplicationOrder_NotArrayPosition)
	{
		ASSERT_THAT(IsTrue(Populated()));

		Fix.GetStack(Target)->ActiveEffects.Swap(0, 3);
		ASSERT_THAT(IsTrue(Fix.GetStack(Target)->ActiveEffects[0].Handle == FireFromA2));

		FArcEffectRemovalQuery Oldest;
		Oldest.Definition = FireDefinition;
		Oldest.MaxCount = 1;
		Oldest.Order = EArcEffectRemovalOrder::OldestFirst;
		ASSERT_THAT(AreEqual(1, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, Oldest)));
		ASSERT_THAT(IsNull(Fix.FindActive(Target, FireFromA1)));
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, FireFromA2)));

		FArcEffectRemovalQuery Newest;
		Newest.Definition = FireDefinition;
		Newest.MaxCount = 1;
		Newest.Order = EArcEffectRemovalOrder::NewestFirst;
		ASSERT_THAT(AreEqual(1, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, Newest)));
		ASSERT_THAT(IsNull(Fix.FindActive(Target, FireFromA2)));
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, FireFromB)));
	}

	TEST_METHOD(NothingMatching_ReturnsZero)
	{
		ASSERT_THAT(IsTrue(Populated()));

		UArcEffectDefinition* NeverApplied = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 10.f);
		FArcEffectRemovalQuery Query;
		Query.Definition = NeverApplied;
		ASSERT_THAT(AreEqual(0, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, Query)));
		ASSERT_THAT(AreEqual(4, Fix.NumActive(Target)));
	}
};

// ============================================================================
// Pre-application gate (plan step 4)
// ============================================================================

TEST_CLASS(ArcEffectLifecycle_Gate, "ArcMassAbilities.Effects.Lifecycle.Gate")
{
	FArcEffectLifecycleFixture Fix;

	BEFORE_EACH()
	{
		Fix.Setup();
	}

	UArcEffectDefinition* MakeTagged(FGameplayTag Tag)
	{
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 5.f);
		Fix.AddIdentity(Definition, Tag);
		return Definition;
	}

	UArcEffectDefinition* MakeImmunityGrant(FGameplayTag Tag)
	{
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 0.f);
		FArcEffectComp_GrantImmunity Grant;
		Grant.GrantedImmunityTags.AddTag(Tag);
		Fix.AddComponent(Definition, Grant);
		return Definition;
	}

	FArcEffectApplyResult ApplyWithOverload(int32 Overload, FMassEntityHandle Target, UArcEffectDefinition* Definition)
	{
		if (Overload == 0)
		{
			return ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		}

		FArcEffectContext Context;
		if (Overload == 1)
		{
			return ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, Context);
		}

		Context.EntityManager = Fix.EntityManager;
		Context.TargetEntity = Target;
		const FSharedStruct Spec = ArcEffects::MakeSpec(Definition, Context);
		return ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Spec);
	}

	float RemainingDurationAfterApply(int32 Overload)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Duration, 10.f, 0.f);

		const FArcEffectApplyResult Result = ApplyWithOverload(Overload, Target, Definition);
		const FArcActiveEffect* Active = Fix.FindActive(Target, Result.Handle);
		return Active ? Active->RemainingDuration : -1.f;
	}

	TEST_METHOD(Veto_RejectsApplication_WithReasonTag)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 5.f);
		FArcTestComp_PreApply PreApply;
		PreApply.bVeto = true;
		PreApply.VetoReason = TAG_Lifecycle_FrostVeto;
		Fix.AddComponent(Definition, PreApply);
		Fix.AddRecorder(Definition, TEXT("Vetoed"));
		Fix.CaptureEvents(ArcGameEventTags::EffectRejected.GetTag());

		const FArcEffectApplyResult Result = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		Fix.DrainEvents();

		ASSERT_THAT(IsFalse(Result.bApplied));
		ASSERT_THAT(IsFalse(Result.Handle.IsValid()));
		ASSERT_THAT(AreEqual(0, Fix.NumActive(Target)));
		ASSERT_THAT(AreEqual(0, Fix.CountHooks(LC::EHookKind::Applied, TEXT("Vetoed"))));
		ASSERT_THAT(AreEqual(1, Fix.CapturedEvents.Num()));

		const FArcEffectRejectedPayload* Payload = Fix.CapturedEvents[0].Payload.GetPtr<FArcEffectRejectedPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue(Payload->Reason == TAG_Lifecycle_FrostVeto.GetTag()));
		const FArcEffectSpec* RejectedSpec = Payload->Spec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(RejectedSpec));
		ASSERT_THAT(IsTrue(RejectedSpec->Definition == Definition));
	}

	TEST_METHOD(Veto_PrebuiltSpecOverload_RejectsApplication)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 5.f);
		FArcTestComp_PreApply PreApply;
		PreApply.bVeto = true;
		PreApply.VetoReason = TAG_Lifecycle_FrostVeto;
		Fix.AddComponent(Definition, PreApply);

		const FArcEffectApplyResult Result = ApplyWithOverload(2, Target, Definition);
		ASSERT_THAT(IsFalse(Result.bApplied));
		ASSERT_THAT(AreEqual(0, Fix.NumActive(Target)));
	}

	TEST_METHOD(Immunity_IsRefCounted_WithTwoGrantors)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* GrantOne = MakeImmunityGrant(TAG_Lifecycle_Fire);
		UArcEffectDefinition* GrantTwo = MakeImmunityGrant(TAG_Lifecycle_Fire);
		const FArcEffectApplyResult One = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, GrantOne, FMassEntityHandle());
		const FArcEffectApplyResult Two = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, GrantTwo, FMassEntityHandle());
		ASSERT_THAT(IsTrue(One.Handle.IsValid()));
		ASSERT_THAT(IsTrue(Two.Handle.IsValid()));

		const FArcImmunityFragment* Immunity = Fix.GetImmunity(Target);
		ASSERT_THAT(IsNotNull(Immunity));
		ASSERT_THAT(AreEqual(2, Immunity->ImmunityTags.GetCount(TAG_Lifecycle_Fire)));

		UArcEffectDefinition* FireDefinition = MakeTagged(TAG_Lifecycle_Fire);
		ASSERT_THAT(IsFalse(ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, FireDefinition, FMassEntityHandle()).bApplied));

		FArcEffectRemovalQuery RemoveOne;
		RemoveOne.Handle = One.Handle;
		ASSERT_THAT(AreEqual(1, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, RemoveOne)));
		ASSERT_THAT(AreEqual(1, Fix.GetImmunity(Target)->ImmunityTags.GetCount(TAG_Lifecycle_Fire)));
		ASSERT_THAT(IsFalse(ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, FireDefinition, FMassEntityHandle()).bApplied));

		FArcEffectRemovalQuery RemoveTwo;
		RemoveTwo.Handle = Two.Handle;
		ASSERT_THAT(AreEqual(1, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, RemoveTwo)));
		ASSERT_THAT(AreEqual(0, Fix.GetImmunity(Target)->ImmunityTags.GetCount(TAG_Lifecycle_Fire)));
		ASSERT_THAT(IsTrue(ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, FireDefinition, FMassEntityHandle()).bApplied));
	}

	TEST_METHOD(DurationRequest_Untouched_KeepsPolicyDuration)
	{
		ASSERT_THAT(IsNear(10.f, RemainingDurationAfterApply(0), 0.001f));
		ASSERT_THAT(IsNear(10.f, RemainingDurationAfterApply(2), 0.001f));
	}

	TEST_METHOD(DurationOverride_RequestOverload_ReplacesPolicyDuration)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Duration, 10.f, 0.f);

		FArcEffectApplyRequest Request;
		Request.Definition = Definition;
		Request.Context.EntityManager = Fix.EntityManager;
		Request.Context.TargetEntity = Target;
		Request.DurationOverride = 3.f;

		const FArcEffectApplyResult Result = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Request);
		const FArcActiveEffect* Active = Fix.FindActive(Target, Result.Handle);
		ASSERT_THAT(IsNotNull(Active));
		ASSERT_THAT(IsNear(3.f, Active->RemainingDuration, 0.001f));
	}

	TEST_METHOD(RequestOverload_SetByCallerSeed_ResolvesIntoActiveSpec)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 0.f);
		FArcEffectModifier SeededModifier = Fix.MakeArmorModifier(0.f);
		SeededModifier.Type = EArcModifierType::SetByCaller;
		SeededModifier.SetByCallerTag = TAG_Lifecycle_Seed;
		Definition->Modifiers.Add(SeededModifier);

		FArcEffectApplyRequest Request;
		Request.Definition = Definition;
		Request.Context.EntityManager = Fix.EntityManager;
		Request.Context.TargetEntity = Target;
		Request.SetByCallerMagnitudes.Add(TAG_Lifecycle_Seed, 7.f);

		const FArcEffectApplyResult Result = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Request);
		ASSERT_THAT(IsTrue(Result.Handle.IsValid()));
		ASSERT_THAT(IsNear(7.f, Fix.FirstAddMagnitude(Target, FArcAttributeSet_TestStats::GetArmorAttribute()), 0.001f));
	}

	TEST_METHOD(PrebuiltSpec_AggregateAcrossTwoTargets_StackCountNotShared)
	{
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Duration, 10.f, 10.f);
		Definition->StackingPolicy.StackPolicy = EArcStackPolicy::Aggregate;
		Definition->StackingPolicy.MaxStacks = 5;

		const FMassEntityHandle TargetA = Fix.MakeEntity();
		const FMassEntityHandle TargetB = Fix.MakeEntity();

		FArcEffectContext Context;
		Context.EntityManager = Fix.EntityManager;
		Context.TargetEntity = TargetA;
		const FSharedStruct Shared = ArcEffects::MakeSpec(Definition, Context);
		const FArcEffectSpec* SharedSpec = Shared.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(SharedSpec));
		const int32 SharedStackBefore = SharedSpec->Context.StackCount;
		const float SharedDurationBefore = SharedSpec->Duration;

		const FArcEffectApplyResult FirstOnA = ArcEffects::TryApplyEffect(*Fix.EntityManager, TargetA, Shared);
		const FArcEffectApplyResult OnB = ArcEffects::TryApplyEffect(*Fix.EntityManager, TargetB, Shared);
		const FArcEffectApplyResult SecondOnA = ArcEffects::TryApplyEffect(*Fix.EntityManager, TargetA, Shared);
		ASSERT_THAT(IsTrue(FirstOnA.Handle.IsValid()));
		ASSERT_THAT(IsTrue(OnB.Handle.IsValid()));
		ASSERT_THAT(IsTrue(SecondOnA.Handle == FirstOnA.Handle));

		const FArcActiveEffect* ActiveA = Fix.FindActive(TargetA, FirstOnA.Handle);
		const FArcActiveEffect* ActiveB = Fix.FindActive(TargetB, OnB.Handle);
		ASSERT_THAT(IsNotNull(ActiveA));
		ASSERT_THAT(IsNotNull(ActiveB));
		ASSERT_THAT(AreEqual(2, ActiveA->Spec.Get<FArcEffectSpec>().Context.StackCount));
		ASSERT_THAT(AreEqual(1, ActiveB->Spec.Get<FArcEffectSpec>().Context.StackCount));
		ASSERT_THAT(IsTrue(ActiveA->Spec.GetPtr<FArcEffectSpec>() != SharedSpec));
		ASSERT_THAT(IsTrue(ActiveB->Spec.GetPtr<FArcEffectSpec>() != SharedSpec));

		ASSERT_THAT(AreEqual(SharedStackBefore, SharedSpec->Context.StackCount));
		ASSERT_THAT(IsNear(SharedDurationBefore, SharedSpec->Duration, 0.001f));
	}

	TEST_METHOD(SpecDuration_DrivesInitialAndRefreshedDuration)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Duration, 10.f, 10.f);
		Definition->StackingPolicy.StackPolicy = EArcStackPolicy::Aggregate;
		Definition->StackingPolicy.MaxStacks = 5;
		Definition->StackingPolicy.RefreshPolicy = EArcStackDurationRefresh::Refresh;

		FArcEffectContext Context;
		Context.EntityManager = Fix.EntityManager;
		Context.TargetEntity = Target;

		FSharedStruct First = ArcEffects::MakeSpec(Definition, Context);
		First.Get<FArcEffectSpec>().Duration = 25.f;
		const FArcEffectApplyResult FirstResult = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, First);
		ASSERT_THAT(IsTrue(FirstResult.Handle.IsValid()));
		const FArcActiveEffect* AfterFirst = Fix.FindActive(Target, FirstResult.Handle);
		ASSERT_THAT(IsNotNull(AfterFirst));
		ASSERT_THAT(IsNear(25.f, AfterFirst->RemainingDuration, 0.001f));

		FSharedStruct Second = ArcEffects::MakeSpec(Definition, Context);
		Second.Get<FArcEffectSpec>().Duration = 40.f;
		ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Second);
		const FArcActiveEffect* AfterSecond = Fix.FindActive(Target, FirstResult.Handle);
		ASSERT_THAT(IsNotNull(AfterSecond));
		ASSERT_THAT(IsNear(40.f, AfterSecond->RemainingDuration, 0.001f));
	}

	TEST_METHOD(RefreshLonger_KeepsMaxOfRemainingAndIncoming)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Duration, 10.f, 10.f);
		Definition->StackingPolicy.StackPolicy = EArcStackPolicy::Aggregate;
		Definition->StackingPolicy.MaxStacks = 5;
		Definition->StackingPolicy.RefreshPolicy = EArcStackDurationRefresh::Longer;

		const FArcEffectApplyResult First = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		ASSERT_THAT(IsTrue(First.Handle.IsValid()));

		Fix.FindActive(Target, First.Handle)->RemainingDuration = 2.f;
		ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		ASSERT_THAT(IsNear(10.f, Fix.FindActive(Target, First.Handle)->RemainingDuration, 0.001f));

		Fix.FindActive(Target, First.Handle)->RemainingDuration = 20.f;
		ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		ASSERT_THAT(IsNear(20.f, Fix.FindActive(Target, First.Handle)->RemainingDuration, 0.001f));
	}

	TEST_METHOD(RefreshLonger_AtMaxStacks_KeepsMaxOfRemainingAndIncoming)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Duration, 10.f, 10.f);
		Definition->StackingPolicy.StackPolicy = EArcStackPolicy::Aggregate;
		Definition->StackingPolicy.MaxStacks = 1;
		Definition->StackingPolicy.RefreshPolicy = EArcStackDurationRefresh::Longer;

		const FArcEffectApplyResult First = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		ASSERT_THAT(IsTrue(First.Handle.IsValid()));

		Fix.FindActive(Target, First.Handle)->RemainingDuration = 2.f;
		ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		ASSERT_THAT(IsNear(10.f, Fix.FindActive(Target, First.Handle)->RemainingDuration, 0.001f));

		Fix.FindActive(Target, First.Handle)->RemainingDuration = 20.f;
		ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		ASSERT_THAT(IsNear(20.f, Fix.FindActive(Target, First.Handle)->RemainingDuration, 0.001f));
	}

	TEST_METHOD(Replace_WithReallocatingOnApplied_ReturnsValidCurrentHandle)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();

		UArcEffectDefinition* Spawned = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 0.f);
		Spawned->StackingPolicy.StackType = EArcStackType::Instance;
		Spawned->StackingPolicy.MaxStacks = 64;

		UArcEffectDefinition* Replacing = Fix.MakeDefinition(EArcEffectDuration::Duration, 50.f, 10.f);
		Replacing->StackingPolicy.StackPolicy = EArcStackPolicy::Replace;
		FArcEffectComp_RemoveEffects RemoveOlder;
		RemoveOlder.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Lifecycle_Fire));
		Fix.AddComponent(Replacing, RemoveOlder);
		FArcEffectComp_ApplyOtherEffect ApplyOther;
		ApplyOther.EffectToApply = Spawned;
		Fix.AddComponent(Replacing, ApplyOther);

		UArcEffectDefinition* Older = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 0.f);
		Fix.AddIdentity(Older, TAG_Lifecycle_Fire);

		const FArcEffectApplyResult First = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Replacing, FMassEntityHandle());
		ASSERT_THAT(IsTrue(First.Handle.IsValid()));
		const FArcEffectApplyResult OlderResult = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Older, FMassEntityHandle());
		ASSERT_THAT(IsTrue(OlderResult.Handle.IsValid()));

		Fix.GetStack(Target)->ActiveEffects.Shrink();
		const FArcEffectApplyResult Second = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Replacing, FMassEntityHandle());

		ASSERT_THAT(IsTrue(Second.bApplied));
		ASSERT_THAT(IsTrue(Second.Handle.IsValid()));
		ASSERT_THAT(IsTrue(Second.Handle != First.Handle));
		ASSERT_THAT(IsNull(Fix.FindActive(Target, First.Handle)));
		ASSERT_THAT(IsNull(Fix.FindActive(Target, OlderResult.Handle)));

		const FArcActiveEffect* Active = Fix.FindActive(Target, Second.Handle);
		ASSERT_THAT(IsNotNull(Active));
		ASSERT_THAT(IsTrue(Active->Spec.Get<FArcEffectSpec>().Definition == Replacing));
		ASSERT_THAT(AreEqual(3, Fix.NumActive(Target)));

		const FArcActiveEffect& LastSlot = Fix.GetStack(Target)->ActiveEffects.Last();
		ASSERT_THAT(IsTrue(LastSlot.Handle.IsValid()));
		ASSERT_THAT(IsTrue(LastSlot.Handle != Second.Handle, TEXT("the slot where the replacing effect was stored now holds a different valid effect")));
	}
};

// ============================================================================
// Identity tags (plan step 5)
// ============================================================================

TEST_CLASS(ArcEffectLifecycle_Identity, "ArcMassAbilities.Effects.Lifecycle.Identity")
{
	FArcEffectLifecycleFixture Fix;

	BEFORE_EACH()
	{
		Fix.Setup();
	}

	TEST_METHOD(CustomComponent_ContributesIdentityTags)
	{
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 5.f);
		FArcTestComp_Identity Identity;
		Identity.Tags.AddTag(TAG_Lifecycle_Fire);
		Identity.Tags.AddTag(TAG_Lifecycle_Ice);
		Fix.AddComponent(Definition, Identity);

		const FGameplayTagContainer& Tags = ArcEffectHelpers::GetEffectTags(Definition);
		ASSERT_THAT(IsTrue(Tags.HasTagExact(TAG_Lifecycle_Fire)));
		ASSERT_THAT(IsTrue(Tags.HasTagExact(TAG_Lifecycle_Ice)));
		ASSERT_THAT(IsFalse(Tags.HasTagExact(TAG_Lifecycle_Plain)));
	}

	TEST_METHOD(CustomComponent_TagsAreUsedByRemoveEffects)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* Tagged = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 5.f);
		Fix.AddIdentity(Tagged, TAG_Lifecycle_Fire);
		UArcEffectDefinition* Untagged = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 5.f);
		const FArcEffectApplyResult TaggedResult = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Tagged, FMassEntityHandle());
		const FArcEffectApplyResult UntaggedResult = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Untagged, FMassEntityHandle());

		FArcEffectRemovalQuery Query;
		Query.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Lifecycle_Fire));
		ASSERT_THAT(AreEqual(1, ArcEffects::RemoveEffects(*Fix.EntityManager, Target, Query)));
		ASSERT_THAT(IsNull(Fix.FindActive(Target, TaggedResult.Handle)));
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, UntaggedResult.Handle)));
	}

	TEST_METHOD(NewObjectDefinition_WithoutBuildCaptureLayout_ReportsEffectTagsComponent)
	{
		UArcEffectDefinition* Definition = NewObject<UArcEffectDefinition>();
		FArcEffectComp_EffectTags EffectTags;
		EffectTags.OwnedTags.AddTag(TAG_Lifecycle_Fire);
		Definition->Components.Add(FInstancedStruct::Make(EffectTags));

		const FGameplayTagContainer& Tags = Definition->GetEffectTags();
		ASSERT_THAT(IsTrue(Tags.HasTagExact(TAG_Lifecycle_Fire)));
		ASSERT_THAT(IsTrue(&Tags == &Definition->GetEffectTags()));
	}

	TEST_METHOD(NewObjectDefinition_WithoutBuildCaptureLayout_ReportsCustomComponentTags)
	{
		UArcEffectDefinition* Definition = NewObject<UArcEffectDefinition>();
		FArcTestComp_Identity Identity;
		Identity.Tags.AddTag(TAG_Lifecycle_Ice);
		Definition->Components.Add(FInstancedStruct::Make(Identity));

		ASSERT_THAT(IsTrue(Definition->GetEffectTags().HasTagExact(TAG_Lifecycle_Ice)));
	}

	TEST_METHOD(ComponentsWithoutOverride_ContributeNoTags)
	{
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 5.f);
		Fix.AddRecorder(Definition, TEXT("A"));
		FArcTestComp_PreApply PreApply;
		Fix.AddComponent(Definition, PreApply);

		ASSERT_THAT(IsTrue(ArcEffectHelpers::GetEffectTags(Definition).IsEmpty()));
		ASSERT_THAT(IsTrue(ArcEffectHelpers::GetEffectTags(nullptr).IsEmpty()));
	}

	TEST_METHOD(EffectTagsAndCustomComponents_TagsMerge)
	{
		UArcEffectDefinition* Definition = NewObject<UArcEffectDefinition>();
		FArcEffectComp_EffectTags EffectTags;
		EffectTags.OwnedTags.AddTag(TAG_Lifecycle_Fire);
		Definition->Components.Add(FInstancedStruct::Make(EffectTags));
		FArcTestComp_Identity Identity;
		Identity.Tags.AddTag(TAG_Lifecycle_Ice);
		Definition->Components.Add(FInstancedStruct::Make(Identity));

		const FGameplayTagContainer& Tags = Definition->GetEffectTags();
		ASSERT_THAT(IsTrue(Tags.HasTagExact(TAG_Lifecycle_Fire)));
		ASSERT_THAT(IsTrue(Tags.HasTagExact(TAG_Lifecycle_Ice)));
	}
};

// ============================================================================
// ConsumeStack and decay rate (plan step 7)
// ============================================================================

TEST_CLASS(ArcEffectLifecycle_Stacks, "ArcMassAbilities.Effects.Lifecycle.Stacks")
{
	FArcEffectLifecycleFixture Fix;

	BEFORE_EACH()
	{
		Fix.Setup();
	}

	UArcEffectDefinition* MakeStackingDefinition()
	{
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Infinite, 0.f, 10.f);
		Definition->StackingPolicy.StackPolicy = EArcStackPolicy::Aggregate;
		Definition->StackingPolicy.MaxStacks = 5;
		Fix.AddRecorder(Definition, TEXT("S"));
		return Definition;
	}

	TEST_METHOD(ConsumeStack_Partial_LowersCountRebuildsModsAndNotifies)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* Definition = MakeStackingDefinition();
		FArcActiveEffectHandle Handle;
		for (int32 Application = 0; Application < 3; ++Application)
		{
			Handle = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle()).Handle;
		}
		ASSERT_THAT(IsTrue(Handle.IsValid()));
		ASSERT_THAT(IsNear(30.f, Fix.FirstAddMagnitude(Target, FArcAttributeSet_TestStats::GetArmorAttribute()), 0.001f));

		ASSERT_THAT(AreEqual(2, ArcEffects::ConsumeStack(*Fix.EntityManager, Target, Handle, 1)));

		const FArcActiveEffect* Active = Fix.FindActive(Target, Handle);
		ASSERT_THAT(IsNotNull(Active));
		ASSERT_THAT(AreEqual(2, Active->Spec.Get<FArcEffectSpec>().Context.StackCount));
		ASSERT_THAT(AreEqual(1, Fix.CountMods(Target, FArcAttributeSet_TestStats::GetArmorAttribute())));
		ASSERT_THAT(IsNear(20.f, Fix.FirstAddMagnitude(Target, FArcAttributeSet_TestStats::GetArmorAttribute()), 0.001f));
		ASSERT_THAT(IsTrue(Fix.CheckModInvariant(Target).IsEmpty()));

		const LC::FHookRecord* Change = Fix.FindLastHook(LC::EHookKind::StackChanged);
		ASSERT_THAT(IsNotNull(Change));
		ASSERT_THAT(AreEqual(3, Change->OldCount));
		ASSERT_THAT(AreEqual(2, Change->NewCount));
		ASSERT_THAT(AreEqual(0, Fix.CountHooks(LC::EHookKind::Removed, TEXT("S"))));
	}

	TEST_METHOD(ConsumeStack_ToZero_RemovesWithRemovedReasonAndReturnsZero)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* Definition = MakeStackingDefinition();
		FArcActiveEffectHandle Handle;
		for (int32 Application = 0; Application < 2; ++Application)
		{
			Handle = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle()).Handle;
		}

		ASSERT_THAT(AreEqual(0, ArcEffects::ConsumeStack(*Fix.EntityManager, Target, Handle, 2)));

		ASSERT_THAT(AreEqual(0, Fix.NumActive(Target)));
		ASSERT_THAT(AreEqual(0, Fix.CountMods(Target, FArcAttributeSet_TestStats::GetArmorAttribute())));
		ASSERT_THAT(AreEqual(1, Fix.CountHooks(LC::EHookKind::Removed, TEXT("S"))));
		const LC::FHookRecord* Removal = Fix.FindHook(LC::EHookKind::Removed, TEXT("S"));
		ASSERT_THAT(IsTrue(Removal->Reason == EArcEffectRemovalReason::Removed));
		ASSERT_THAT(IsTrue(Removal->Handle == Handle));
	}

	TEST_METHOD(ConsumeStack_MoreThanHeld_ClampsToZeroAndRemoves)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* Definition = MakeStackingDefinition();
		ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		const FArcActiveEffectHandle Handle = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle()).Handle;

		ASSERT_THAT(AreEqual(0, ArcEffects::ConsumeStack(*Fix.EntityManager, Target, Handle, 5)));
		ASSERT_THAT(AreEqual(0, Fix.NumActive(Target)));
	}

	TEST_METHOD(DecayRate_Increased_SpeedsExpiry)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* FastDefinition = Fix.MakeDefinition(EArcEffectDuration::Duration, 10.f, 10.f);
		Fix.AddRecorder(FastDefinition, TEXT("Fast"));
		UArcEffectDefinition* SlowDefinition = Fix.MakeDefinition(EArcEffectDuration::Duration, 10.f, 10.f);
		Fix.AddRecorder(SlowDefinition, TEXT("Slow"));

		const FArcEffectApplyResult Fast = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, FastDefinition, FMassEntityHandle());
		const FArcEffectApplyResult Slow = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, SlowDefinition, FMassEntityHandle());
		ASSERT_THAT(IsTrue(ArcEffects::SetEffectDecayRate(*Fix.EntityManager, Target, Fast.Handle, 2.f)));

		Fix.TickDuration(3.f);
		ASSERT_THAT(IsNear(4.f, Fix.FindActive(Target, Fast.Handle)->RemainingDuration, 0.001f));
		ASSERT_THAT(IsNear(7.f, Fix.FindActive(Target, Slow.Handle)->RemainingDuration, 0.001f));

		Fix.TickDuration(3.f);
		ASSERT_THAT(IsNull(Fix.FindActive(Target, Fast.Handle)));
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, Slow.Handle)));
		ASSERT_THAT(IsNear(4.f, Fix.FindActive(Target, Slow.Handle)->RemainingDuration, 0.001f));
		const LC::FHookRecord* Expiry = Fix.FindHook(LC::EHookKind::Removed, TEXT("Fast"));
		ASSERT_THAT(IsNotNull(Expiry));
		ASSERT_THAT(IsTrue(Expiry->Reason == EArcEffectRemovalReason::Expired));
		ASSERT_THAT(AreEqual(0, Fix.CountHooks(LC::EHookKind::Removed, TEXT("Slow"))));
	}

	TEST_METHOD(DecayRate_Zero_FreezesCountdown)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		UArcEffectDefinition* Definition = Fix.MakeDefinition(EArcEffectDuration::Duration, 10.f, 10.f);
		const FArcEffectApplyResult Result = ArcEffects::TryApplyEffect(*Fix.EntityManager, Target, Definition, FMassEntityHandle());
		ASSERT_THAT(IsTrue(ArcEffects::SetEffectDecayRate(*Fix.EntityManager, Target, Result.Handle, 0.f)));

		Fix.TickDuration(50.f);
		ASSERT_THAT(IsNotNull(Fix.FindActive(Target, Result.Handle)));
		ASSERT_THAT(IsNear(10.f, Fix.FindActive(Target, Result.Handle)->RemainingDuration, 0.001f));
	}

	TEST_METHOD(DecayRate_UnknownHandle_ReturnsFalse)
	{
		const FMassEntityHandle Target = Fix.MakeEntity();
		ASSERT_THAT(IsFalse(ArcEffects::SetEffectDecayRate(*Fix.EntityManager, Target, FArcActiveEffectHandle(), 2.f)));
		ASSERT_THAT(IsFalse(ArcEffects::SetEffectDecayRate(*Fix.EntityManager, Target, FArcActiveEffectHandle::Generate(), 2.f)));
	}
};
