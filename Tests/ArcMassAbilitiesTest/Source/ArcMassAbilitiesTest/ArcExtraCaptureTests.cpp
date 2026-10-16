// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcItemTestRecordFiles.h"
#include "Components/ActorTestSpawner.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "ArcExtraCaptureTestTypes.h"
#include "Effects/ArcEffectSpec.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectExecution.h"
#include "Effects/ArcExtraCapture.h"
#include "Effects/ArcEffectContext.h"
#include "Effects/Sources/ArcExtraCaptureSource_SourceEntity.h"
#include "Captures/ArcExtraCaptureSource_Item.h"
#include "Sources/ArcAbilitySource_Item.h"
#include "ItemFragments/ArcItemFragment_ItemStats.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemId.h"
#include "GameplayTagContainer.h"
#include "StructUtils/SharedStruct.h"
#include "StructUtils/InstancedStruct.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityBuilder.h"

// ---------------------------------------------------------------------------
// File-scoped namespace (unique per CLAUDE.md rule)
// ---------------------------------------------------------------------------

namespace ArcExtraCaptureTestsImpl
{

/** Build a UArcEffectDefinition containing a single execution entry whose
 *  ExtraCaptures array has been pre-filled by the caller.
 *  The FArcTestAddExecution is a throw-away shell; its TargetAttribute/Value
 *  don't matter for extra-capture tests. */
static UArcEffectDefinition* MakeDefWithExtraCaptures(TArray<FArcExtraCaptureDef> ExtraCaptures)
{
	UArcEffectDefinition* Def = NewObject<UArcEffectDefinition>();

	FInstancedStruct ExecInst = FInstancedStruct::Make<FArcTestAddExecution>();
	FArcTestAddExecution& Exec = ExecInst.GetMutable<FArcTestAddExecution>();
	Exec.ExtraCaptures = MoveTemp(ExtraCaptures);

	FArcEffectExecutionEntry Entry;
	Entry.CustomExecution = MoveTemp(ExecInst);
	Def->Executions.Add(Entry);

	Def->BuildCaptureLayout();

	return Def;
}

} // namespace ArcExtraCaptureTestsImpl

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

TEST_CLASS(ArcExtraCapture_Channel, "ArcMassAbilities.ExtraCapture")
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

	// -------------------------------------------------------------------------
	// 1. Snapshot_Resolves_StoresValue
	// -------------------------------------------------------------------------
	TEST_METHOD(Snapshot_Resolves_StoresValue)
	{
		FArcExtraCaptureDef Def;
		Def.Source = FInstancedStruct::Make<FArcTestExtraCaptureSource_Constant>();
		Def.Source.GetMutable<FArcTestExtraCaptureSource_Constant>().ConstantValue = 42.f;
		Def.bSnapshot = true;

		TArray<FArcExtraCaptureDef> Extras;
		Extras.Add(Def); // keep a copy — Def used again below

		UArcEffectDefinition* EffectDef = ArcExtraCaptureTestsImpl::MakeDefWithExtraCaptures(MoveTemp(Extras));

		FArcEffectContext Ctx;
		Ctx.EntityManager = EntityManager;
		FSharedStruct SharedSpec = ArcEffects::MakeSpec(EffectDef, Ctx);
		const FArcEffectSpec* Spec = SharedSpec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(Spec));

		ASSERT_THAT(AreEqual(Spec->NumCapturedExtras(), 1));
		const FArcCapturedExtra* Entry = Spec->FindCapturedExtra(Def);
		ASSERT_THAT(IsNotNull(Entry));
		ASSERT_THAT(IsTrue(Entry->Definition.bSnapshot));
		ASSERT_THAT(IsTrue(Entry->bHasValue));
		ASSERT_THAT(IsNear(Entry->SnapshotValue, 42.f, 0.001f));

		// Verify AttemptResolveExtraCapture returns the snapshotted value.
		FArcEffectExecutionContext ExecCtx;
		ExecCtx.OwningSpec = Spec;
		ExecCtx.EntityManager = EntityManager;

		float Out = 0.f;
		FArcEvaluateParameters EvalParams;
		const bool bOk = ExecCtx.AttemptResolveExtraCapture(Def, EvalParams, Out);
		ASSERT_THAT(IsTrue(bOk));
		ASSERT_THAT(IsNear(Out, 42.f, 0.001f));
	}

	// -------------------------------------------------------------------------
	// 2. Snapshot_ResolveFails_EntryPresentButNoValue
	// -------------------------------------------------------------------------
	TEST_METHOD(Snapshot_ResolveFails_EntryPresentButNoValue)
	{
		FArcExtraCaptureDef Def;
		Def.Source = FInstancedStruct::Make<FArcTestExtraCaptureSource_Constant>();
		FArcTestExtraCaptureSource_Constant& Src = Def.Source.GetMutable<FArcTestExtraCaptureSource_Constant>();
		Src.bResolveSucceeds = false;
		Def.bSnapshot = true;

		TArray<FArcExtraCaptureDef> Extras;
		Extras.Add(Def); // keep a copy — Def used again below

		UArcEffectDefinition* EffectDef = ArcExtraCaptureTestsImpl::MakeDefWithExtraCaptures(MoveTemp(Extras));

		FArcEffectContext Ctx;
		Ctx.EntityManager = EntityManager;
		FSharedStruct SharedSpec = ArcEffects::MakeSpec(EffectDef, Ctx);
		const FArcEffectSpec* Spec = SharedSpec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(Spec));

		// Positional slot is still created even when resolve fails (bHasValue=false).
		ASSERT_THAT(AreEqual(Spec->NumCapturedExtras(), 1));
		const FArcCapturedExtra* Entry = Spec->FindCapturedExtra(Def);
		ASSERT_THAT(IsNotNull(Entry));
		ASSERT_THAT(IsTrue(Entry->Definition.bSnapshot));
		ASSERT_THAT(IsFalse(Entry->bHasValue));

		// AttemptResolveExtraCapture must return false when bHasValue is false.
		FArcEffectExecutionContext ExecCtx;
		ExecCtx.OwningSpec = Spec;
		ExecCtx.EntityManager = EntityManager;

		float Out = 0.f;
		FArcEvaluateParameters EvalParams;
		const bool bOk = ExecCtx.AttemptResolveExtraCapture(Def, EvalParams, Out);
		ASSERT_THAT(IsFalse(bOk));
	}

	// -------------------------------------------------------------------------
	// 3. Live_DoesNotResolveAtMakeSpec
	// -------------------------------------------------------------------------
	TEST_METHOD(Live_DoesNotResolveAtMakeSpec)
	{
		FArcExtraCaptureDef Def;
		Def.Source = FInstancedStruct::Make<FArcTestExtraCaptureSource_Constant>();
		Def.Source.GetMutable<FArcTestExtraCaptureSource_Constant>().ConstantValue = 7.f;
		Def.bSnapshot = false;

		TArray<FArcExtraCaptureDef> Extras;
		Extras.Add(MoveTemp(Def));

		UArcEffectDefinition* EffectDef = ArcExtraCaptureTestsImpl::MakeDefWithExtraCaptures(MoveTemp(Extras));

		FArcEffectContext Ctx;
		Ctx.EntityManager = EntityManager;
		FSharedStruct SharedSpec = ArcEffects::MakeSpec(EffectDef, Ctx);
		const FArcEffectSpec* Spec = SharedSpec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(Spec));

		// Live def: slot exists, bSnapshot=false; resolver has NOT been called yet.
		ASSERT_THAT(AreEqual(Spec->NumCapturedExtras(), 1));
		const FArcCapturedExtra* Entry = nullptr;
		Spec->ForEachCapturedExtra([&](const FArcCapturedExtra& E) { if (!Entry) { Entry = &E; } });
		ASSERT_THAT(IsNotNull(Entry));
		ASSERT_THAT(IsFalse(Entry->Definition.bSnapshot));

		const FArcTestExtraCaptureSource_Constant* LivePtr =
			Entry->Definition.Source.GetPtr<FArcTestExtraCaptureSource_Constant>();
		ASSERT_THAT(IsNotNull(LivePtr));
		ASSERT_THAT(AreEqual(LivePtr->ResolveCount, 0));
	}

	// -------------------------------------------------------------------------
	// 4. Live_ResolvesEachCall
	// -------------------------------------------------------------------------
	TEST_METHOD(Live_ResolvesEachCall)
	{
		FArcExtraCaptureDef Def;
		Def.Source = FInstancedStruct::Make<FArcTestExtraCaptureSource_Constant>();
		Def.Source.GetMutable<FArcTestExtraCaptureSource_Constant>().ConstantValue = 5.f;
		Def.bSnapshot = false;

		TArray<FArcExtraCaptureDef> Extras;
		Extras.Add(Def); // keep a copy — Def reused for both AttemptResolveExtraCapture calls

		UArcEffectDefinition* EffectDef = ArcExtraCaptureTestsImpl::MakeDefWithExtraCaptures(MoveTemp(Extras));

		FArcEffectContext EffCtx;
		EffCtx.EntityManager = EntityManager;
		FSharedStruct SharedSpec = ArcEffects::MakeSpec(EffectDef, EffCtx);
		const FArcEffectSpec* Spec = SharedSpec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(Spec));

		FArcEffectExecutionContext ExecCtx;
		ExecCtx.OwningSpec = Spec;
		ExecCtx.EntityManager = EntityManager;

		// First call — should resolve to 5
		float Out1 = 0.f;
		FArcEvaluateParameters EvalParams;
		const bool bOk1 = ExecCtx.AttemptResolveExtraCapture(Def, EvalParams, Out1);
		ASSERT_THAT(IsTrue(bOk1));
		ASSERT_THAT(IsNear(Out1, 5.f, 0.001f));

		// Mutate the live def value in-spec to verify re-resolution.
		// Spec is const* (SharedSpec owns it) — cast away const for test-local mutation.
		FArcEffectSpec* MutableSpec = const_cast<FArcEffectSpec*>(Spec);
		FArcCapturedExtra* MutableEntry = MutableSpec->FindMutableCapturedExtra(Def);
		ASSERT_THAT(IsNotNull(MutableEntry));
		FArcTestExtraCaptureSource_Constant* MutableLivePtr =
			MutableEntry->Definition.Source.GetMutablePtr<FArcTestExtraCaptureSource_Constant>();
		ASSERT_THAT(IsNotNull(MutableLivePtr));
		MutableLivePtr->ConstantValue = 9.f;

		// Second call — should resolve to 9
		float Out2 = 0.f;
		const bool bOk2 = ExecCtx.AttemptResolveExtraCapture(Def, EvalParams, Out2);
		ASSERT_THAT(IsTrue(bOk2));
		ASSERT_THAT(IsNear(Out2, 9.f, 0.001f));

		// ResolveCount on the live def should be 2
		ASSERT_THAT(AreEqual(MutableLivePtr->ResolveCount, 2));
	}

	// -------------------------------------------------------------------------
	// 5. Live_ResolveFails_ReturnsFalse
	// -------------------------------------------------------------------------
	TEST_METHOD(Live_ResolveFails_ReturnsFalse)
	{
		FArcExtraCaptureDef Def;
		Def.Source = FInstancedStruct::Make<FArcTestExtraCaptureSource_Constant>();
		FArcTestExtraCaptureSource_Constant& Src = Def.Source.GetMutable<FArcTestExtraCaptureSource_Constant>();
		Src.bResolveSucceeds = false;
		Def.bSnapshot = false;

		TArray<FArcExtraCaptureDef> Extras;
		Extras.Add(Def); // keep a copy — Def reused for AttemptResolveExtraCapture

		UArcEffectDefinition* EffectDef = ArcExtraCaptureTestsImpl::MakeDefWithExtraCaptures(MoveTemp(Extras));

		FArcEffectContext EffCtx;
		EffCtx.EntityManager = EntityManager;
		FSharedStruct SharedSpec = ArcEffects::MakeSpec(EffectDef, EffCtx);
		const FArcEffectSpec* Spec = SharedSpec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(Spec));

		FArcEffectExecutionContext ExecCtx;
		ExecCtx.OwningSpec = Spec;
		ExecCtx.EntityManager = EntityManager;

		float Out = 0.f;
		FArcEvaluateParameters EvalParams;
		const bool bResult = ExecCtx.AttemptResolveExtraCapture(Def, EvalParams, Out);
		ASSERT_THAT(IsFalse(bResult));
	}

	// -------------------------------------------------------------------------
	// 6. UnregisteredDef_ReturnsFalse
	// -------------------------------------------------------------------------
	TEST_METHOD(UnregisteredDef_ReturnsFalse)
	{
		// Build a spec that registers one Def (snapshot, ConstantValue=10).
		FArcExtraCaptureDef RegisteredDef;
		RegisteredDef.Source = FInstancedStruct::Make<FArcTestExtraCaptureSource_Constant>();
		RegisteredDef.Source.GetMutable<FArcTestExtraCaptureSource_Constant>().ConstantValue = 10.f;
		RegisteredDef.bSnapshot = true;

		TArray<FArcExtraCaptureDef> Extras;
		Extras.Add(MoveTemp(RegisteredDef));

		UArcEffectDefinition* EffectDef = ArcExtraCaptureTestsImpl::MakeDefWithExtraCaptures(MoveTemp(Extras));

		FArcEffectContext EffCtx;
		EffCtx.EntityManager = EntityManager;
		FSharedStruct SharedSpec = ArcEffects::MakeSpec(EffectDef, EffCtx);
		const FArcEffectSpec* Spec = SharedSpec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(Spec));
		ASSERT_THAT(AreEqual(Spec->NumCapturedExtras(), 1));

		FArcEffectExecutionContext ExecCtx;
		ExecCtx.OwningSpec = Spec;
		ExecCtx.EntityManager = EntityManager;

		// Construct a different Def — uses SourceEntity resolver type instead of Constant.
		// This Def was never registered in the spec's CapturedExtras, so lookup must fail.
		FArcExtraCaptureDef DifferentDef;
		DifferentDef.Source = FInstancedStruct::Make<FArcExtraCaptureSource_SourceEntity>();
		DifferentDef.bSnapshot = true;

		float Out = 0.f;
		FArcEvaluateParameters EvalParams;
		const bool bResult = ExecCtx.AttemptResolveExtraCapture(DifferentDef, EvalParams, Out);
		ASSERT_THAT(IsFalse(bResult));
	}

	// -------------------------------------------------------------------------
	// 7. SourceEntity_Resolves_EndToEnd
	// -------------------------------------------------------------------------
	TEST_METHOD(SourceEntity_Resolves_EndToEnd)
	{
		// Build an entity with FArcAttributeSet_TestStats sparse fragment, Health seeded to 50.
		FArcAttributeSet_TestStats Stats;
		Stats.Health.BaseValue = 50.f;
		Stats.Health.CurrentValue = 50.f;

		UE::Mass::FEntityBuilder Builder(*EntityManager);
		FMassEntityHandle SourceEntity = Builder
			.Add<FArcAttributeSet_TestStats>(Stats)
			.Commit();

		ASSERT_THAT(IsTrue(EntityManager->IsEntityValid(SourceEntity)));

		// Build an effect def whose single execution has one extra-capture:
		// source = FArcExtraCaptureSource_SourceEntity, attribute = Health on FArcAttributeSet_TestStats.
		FArcExtraCaptureDef CaptureDef;
		CaptureDef.Source = FInstancedStruct::Make<FArcExtraCaptureSource_SourceEntity>();
		CaptureDef.Source.GetMutable<FArcExtraCaptureSource_SourceEntity>().Attribute =
			FArcAttributeSet_TestStats::GetHealthAttribute();
		CaptureDef.bSnapshot = true;

		TArray<FArcExtraCaptureDef> Extras;
		Extras.Add(CaptureDef); // keep a copy — CaptureDef reused for AttemptResolveExtraCapture

		UArcEffectDefinition* EffectDef = ArcExtraCaptureTestsImpl::MakeDefWithExtraCaptures(MoveTemp(Extras));

		FArcEffectContext EffCtx;
		EffCtx.EntityManager = EntityManager;
		EffCtx.SourceEntity = SourceEntity;

		FSharedStruct SharedSpec = ArcEffects::MakeSpec(EffectDef, EffCtx);
		const FArcEffectSpec* Spec = SharedSpec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(Spec));

		// CaptureExtras is called by MakeSpec for snapshot entries; verify the result.
		ASSERT_THAT(AreEqual(Spec->NumCapturedExtras(), 1));
		const FArcCapturedExtra* Entry = Spec->FindCapturedExtra(CaptureDef);
		ASSERT_THAT(IsNotNull(Entry));
		ASSERT_THAT(IsTrue(Entry->Definition.bSnapshot));
		ASSERT_THAT(IsTrue(Entry->bHasValue));
		ASSERT_THAT(IsNear(Entry->SnapshotValue, 50.f, 0.001f));

		// Also verify via AttemptResolveExtraCapture using the same Def.
		FArcEffectExecutionContext ExecCtx;
		ExecCtx.OwningSpec = Spec;
		ExecCtx.EntityManager = EntityManager;

		float Out = 0.f;
		FArcEvaluateParameters EvalParams;
		const bool bOk = ExecCtx.AttemptResolveExtraCapture(CaptureDef, EvalParams, Out);
		ASSERT_THAT(IsTrue(bOk));
		ASSERT_THAT(IsNear(Out, 50.f, 0.001f));
	}

	// -------------------------------------------------------------------------
	// 8. SourceEntity_Resolves_LivePath
	//    bSnapshot=false routes through the live re-resolve path
	//    (FArcEffectExecutionContext::ResolveExtraAt → Source->ResolveValue),
	//    proving the attribute moved into the Source payload is read on the
	//    live path, not just at snapshot time.
	// -------------------------------------------------------------------------
	TEST_METHOD(SourceEntity_Resolves_LivePath)
	{
		FArcAttributeSet_TestStats Stats;
		Stats.Health.BaseValue = 70.f;
		Stats.Health.CurrentValue = 70.f;

		UE::Mass::FEntityBuilder Builder(*EntityManager);
		FMassEntityHandle SourceEntity = Builder
			.Add<FArcAttributeSet_TestStats>(Stats)
			.Commit();

		ASSERT_THAT(IsTrue(EntityManager->IsEntityValid(SourceEntity)));

		FArcExtraCaptureDef CaptureDef;
		CaptureDef.Source = FInstancedStruct::Make<FArcExtraCaptureSource_SourceEntity>();
		CaptureDef.Source.GetMutable<FArcExtraCaptureSource_SourceEntity>().Attribute =
			FArcAttributeSet_TestStats::GetHealthAttribute();
		CaptureDef.bSnapshot = false;

		TArray<FArcExtraCaptureDef> Extras;
		Extras.Add(CaptureDef); // keep a copy — CaptureDef reused for AttemptResolveExtraCapture

		UArcEffectDefinition* EffectDef = ArcExtraCaptureTestsImpl::MakeDefWithExtraCaptures(MoveTemp(Extras));

		FArcEffectContext EffCtx;
		EffCtx.EntityManager = EntityManager;
		EffCtx.SourceEntity = SourceEntity;

		FSharedStruct SharedSpec = ArcEffects::MakeSpec(EffectDef, EffCtx);
		const FArcEffectSpec* Spec = SharedSpec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(Spec));

		// Live entry: present but not snapshotted.
		ASSERT_THAT(AreEqual(Spec->NumCapturedExtras(), 1));
		const FArcCapturedExtra* Entry = Spec->FindCapturedExtra(CaptureDef);
		ASSERT_THAT(IsNotNull(Entry));
		ASSERT_THAT(IsFalse(Entry->Definition.bSnapshot));

		// Live re-resolve must read this->Attribute from the Source payload.
		FArcEffectExecutionContext ExecCtx;
		ExecCtx.OwningSpec = Spec;
		ExecCtx.EntityManager = EntityManager;

		float Out = 0.f;
		FArcEvaluateParameters EvalParams;
		const bool bOk = ExecCtx.AttemptResolveExtraCapture(CaptureDef, EvalParams, Out);
		ASSERT_THAT(IsTrue(bOk));
		ASSERT_THAT(IsNear(Out, 70.f, 0.001f));
	}
};

// ---------------------------------------------------------------------------
// Helper: build a minimal FArcItemData with a FArcItemFragment_ItemStats
// containing a single DefaultStat entry.  Calls InitializeMass(nullptr) so
// Recalculate runs and StatTags / Stats caches are populated.
// ---------------------------------------------------------------------------

namespace ArcExtraCaptureItemTestsImpl
{

/** Attribute key used across item-stat filter tests. Points at the Health
 *  property on FArcAttributeSet_TestStats — the same struct used throughout this
 *  suite.  The attribute reference only carries type + property name and does
 *  not require an entity to be valid. */
static FArcAttributeRef TestStatAttribute()
{
	static FArcAttributeRef Ref = []()
	{
		FArcAttributeRef R;
		R.FragmentType = FArcAttributeSet_TestStats::StaticStruct();
		R.PropertyName = GET_MEMBER_NAME_CHECKED(FArcAttributeSet_TestStats, Health);
		return R;
	}();
	return Ref;
}

/** Build an FArcItemData whose FArcItemInstance_ItemStats has been seeded via
 *  the normal InitializeMass → Recalculate path.
 *
 *  @param StatValue   Final additive value for the single DefaultStat row.
 *  @param StatTags    Tags attached to that row (may be empty).
 *  @param OutItemData Storage for the item — must outlive the returned handle.
 */
static FArcItemDataHandle MakeItemDataWithStat(
	FArcItemTestRecordFiles& ItemRecords,
	float StatValue,
	const FGameplayTagContainer& StatTags,
	FArcItemData& OutItemData)
{
	FArcItemDefinitionData Def;

	FArcItemAttributeStat Stat(TestStatAttribute(), StatValue);
	Stat.StatTags = StatTags;

	FArcItemFragment_ItemStats StatsFragment;
	StatsFragment.DefaultStats.Add(Stat);
	ArcItemTestRecords::AddFragment(Def, StatsFragment);

	const FPrimaryAssetId DefId = ItemRecords.Write(Def);
	if (!DefId.IsValid())
	{
		return FArcItemDataHandle();
	}

	// Wire the item data.
	OutItemData.Spec.SetItemDefinition(DefId);
	OutItemData.ItemDefinition = FArcItemDefinitionData::Find(DefId);
	OutItemData.ItemId = FArcItemId::Generate();

	// InitializeMass builds InstancedData and calls Recalculate via OnItemInitialize.
	OutItemData.InitializeMass(nullptr);

	return FArcItemDataHandle(&OutItemData);
}

/** Build an FArcEffectContext whose SourceData is an FArcAbilitySource_Item
 *  wrapping the supplied handle. */
static FArcEffectContext MakeItemContext(
	FMassEntityManager* EntityManager,
	FArcItemDataHandle ItemHandle)
{
	FArcEffectContext Ctx;
	Ctx.EntityManager = EntityManager;

	FArcAbilitySource_Item ItemSource;
	ItemSource.ItemDataHandle = ItemHandle;
	Ctx.SourceData = FInstancedStruct::Make<FArcAbilitySource_Item>(ItemSource);

	return Ctx;
}

} // namespace ArcExtraCaptureItemTestsImpl

// ---------------------------------------------------------------------------
// Test class: FArcExtraCaptureSource_Item tag-filter behaviour
// ---------------------------------------------------------------------------

TEST_CLASS(ArcExtraCapture_Item, "ArcMassAbilities.ExtraCapture")
{
	FArcItemTestRecordFiles ItemRecords;
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

	// -------------------------------------------------------------------------
	// 1. Item_TaggedStat_MatchingSourceTags_Resolves
	//    A stat tagged {GameplayEffect.Damage} resolves when SourceTags contains
	//    that tag (HasAll satisfied).
	// -------------------------------------------------------------------------
	TEST_METHOD(Item_TaggedStat_MatchingSourceTags_Resolves)
	{
		const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(
			FName("ArcEffect.Damage"));
		FGameplayTagContainer StatTagContainer;
		StatTagContainer.AddTag(DamageTag);

		FArcItemData ItemData;
		const FArcItemDataHandle ItemHandle =
			ArcExtraCaptureItemTestsImpl::MakeItemDataWithStat(
				ItemRecords,
				30.f, StatTagContainer, ItemData);
		ASSERT_THAT(IsTrue(ItemHandle.IsValid(), TEXT("Item record must be written and indexed")));

		FGameplayTagContainer ContextTags;
		ContextTags.AddTag(DamageTag);

		const FArcEffectContext Ctx =
			ArcExtraCaptureItemTestsImpl::MakeItemContext(
				EntityManager, ItemHandle);

		FArcExtraCaptureSource_Item Resolver;
		Resolver.Attribute = ArcExtraCaptureItemTestsImpl::TestStatAttribute();
		float OutValue = 0.f;
		FArcEvaluateParameters EvalParams;
		EvalParams.SourceTags = ContextTags;
		const bool bResult = Resolver.ResolveValue(Ctx, *EntityManager, EvalParams, OutValue);

		ASSERT_THAT(IsTrue(bResult));
		ASSERT_THAT(IsNear(OutValue, 30.f, 0.001f));
	}

	// -------------------------------------------------------------------------
	// 2. Item_TaggedStat_NonMatchingSourceTags_Filtered
	//    Same stat tagged {GameplayEffect.Damage} is filtered out when SourceTags
	//    contains a different tag (HasAll not satisfied).
	// -------------------------------------------------------------------------
	TEST_METHOD(Item_TaggedStat_NonMatchingSourceTags_Filtered)
	{
		const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(
			FName("ArcEffect.Damage"));
		const FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(
			FName("ArcEvent.Attack.Execute"));

		FGameplayTagContainer StatTagContainer;
		StatTagContainer.AddTag(DamageTag);

		FArcItemData ItemData;
		const FArcItemDataHandle ItemHandle =
			ArcExtraCaptureItemTestsImpl::MakeItemDataWithStat(
				ItemRecords,
				30.f, StatTagContainer, ItemData);
		ASSERT_THAT(IsTrue(ItemHandle.IsValid(), TEXT("Item record must be written and indexed")));

		// SourceTags only contains AttackTag — does not satisfy the stat's DamageTag requirement.
		FGameplayTagContainer ContextTags;
		ContextTags.AddTag(AttackTag);

		const FArcEffectContext Ctx =
			ArcExtraCaptureItemTestsImpl::MakeItemContext(
				EntityManager, ItemHandle);

		FArcExtraCaptureSource_Item Resolver;
		Resolver.Attribute = ArcExtraCaptureItemTestsImpl::TestStatAttribute();
		float OutValue = 0.f;
		FArcEvaluateParameters EvalParams;
		EvalParams.SourceTags = ContextTags;
		const bool bResult = Resolver.ResolveValue(Ctx, *EntityManager, EvalParams, OutValue);

		ASSERT_THAT(IsFalse(bResult));
	}

	// -------------------------------------------------------------------------
	// 3. Item_UntaggedStat_EmptySourceTags_AlwaysApplies
	//    A stat with empty StatTags always contributes regardless of SourceTags.
	// -------------------------------------------------------------------------
	TEST_METHOD(Item_UntaggedStat_EmptySourceTags_AlwaysApplies)
	{
		FArcItemData ItemData;
		const FArcItemDataHandle ItemHandle =
			ArcExtraCaptureItemTestsImpl::MakeItemDataWithStat(
				ItemRecords,
				15.f, FGameplayTagContainer(), ItemData);
		ASSERT_THAT(IsTrue(ItemHandle.IsValid(), TEXT("Item record must be written and indexed")));

		// Empty SourceTags — untagged stat must still resolve.
		const FArcEffectContext Ctx =
			ArcExtraCaptureItemTestsImpl::MakeItemContext(
				EntityManager, ItemHandle);

		FArcExtraCaptureSource_Item Resolver;
		Resolver.Attribute = ArcExtraCaptureItemTestsImpl::TestStatAttribute();
		float OutValue = 0.f;
		FArcEvaluateParameters EvalParams;
		const bool bResult = Resolver.ResolveValue(Ctx, *EntityManager, EvalParams, OutValue);

		ASSERT_THAT(IsTrue(bResult));
		ASSERT_THAT(IsNear(OutValue, 15.f, 0.001f));
	}

	// -------------------------------------------------------------------------
	// 4. Item_NoItemSource_ReturnsFalse
	//    When SourceData is not FArcAbilitySource_Item the resolver returns false.
	// -------------------------------------------------------------------------
	TEST_METHOD(Item_NoItemSource_ReturnsFalse)
	{
		// SourceData holds a completely different payload type.
		FArcEffectContext Ctx;
		Ctx.EntityManager = EntityManager;
		Ctx.SourceData = FInstancedStruct::Make<FArcTestSourcePayload>();

		FArcExtraCaptureSource_Item Resolver;
		Resolver.Attribute = ArcExtraCaptureItemTestsImpl::TestStatAttribute();
		float OutValue = 0.f;
		FArcEvaluateParameters EvalParams;
		const bool bResult = Resolver.ResolveValue(Ctx, *EntityManager, EvalParams, OutValue);

		ASSERT_THAT(IsFalse(bResult));
	}
};
