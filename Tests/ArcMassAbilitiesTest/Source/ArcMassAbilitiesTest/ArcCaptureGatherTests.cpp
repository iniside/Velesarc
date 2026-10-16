// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "ArcAbilitiesAggregatorTestTypes.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcAttributeCapture.h"
#include "Effects/ArcEffectCaptureLayout.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectExecution.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectTypes.h"
#include "Effects/ArcExtraCapture.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Processors/ArcPendingAttributeOpsProcessor.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassSignalSubsystem.h"
#include "ArcScalableFloat.h"
#include "StructUtils/InstancedStruct.h"

namespace ArcCaptureGatherTestsPrivate
{
	constexpr float SourceAttackPower = 50.f;
	constexpr float TargetArmor = 10.f;
	constexpr float ScopedAddition = 20.f;

	FArcAttributeCaptureDef MakeArmorTargetSnapshotDef()
	{
		return Arc::Capture::MakeCaptureDef<&FArcAttributeSet_TestStats::GetArmorAttribute, EArcCaptureSource::Target, true>();
	}
}

TEST_CLASS(ArcCaptureGather, "ArcMassAbilities.Effects.CaptureGather")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;

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

		SignalSubsystem = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
		check(SignalSubsystem);

		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
		PendingProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);
	}

	void DrainPendingOps(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*PendingProcessor, ProcessingContext);
	}

	struct FTestEntities
	{
		FMassEntityHandle Source;
		FMassEntityHandle Target;
	};

	FTestEntities CreateEntities()
	{
		FArcAttributeSet_TestStats SourceStats;
		SourceStats.AttackPower = { ArcCaptureGatherTestsPrivate::SourceAttackPower, ArcCaptureGatherTestsPrivate::SourceAttackPower };

		TArray<FInstancedStruct> SourceFragments;
		SourceFragments.Add(FInstancedStruct::Make(SourceStats));
		SourceFragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		SourceFragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		SourceFragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
		const FMassEntityHandle SourceEntity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, SourceFragments);

		FArcAttributeSet_TestStats TargetStats;
		TargetStats.Armor = { ArcCaptureGatherTestsPrivate::TargetArmor, ArcCaptureGatherTestsPrivate::TargetArmor };

		TArray<FInstancedStruct> TargetFragments;
		TargetFragments.Add(FInstancedStruct::Make(TargetStats));
		TargetFragments.Add(FInstancedStruct::Make(FArcAttributeSet_TestDerivedStats()));
		TargetFragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		TargetFragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		TargetFragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
		TargetFragments.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));
		const FMassEntityHandle TargetEntity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, TargetFragments);

		return { SourceEntity, TargetEntity };
	}

	UArcEffectDefinition* MakeInstantEffect(const FArcTestGatherCapturesExecution& Exec)
	{
		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));
		Effect->BuildCaptureLayout();
		return Effect;
	}

	FArcTestGatherCapturesExecution MakeExecution(const FArcAttributeRef& ExtraAttribute, EArcCaptureSource ExtraSource, bool bSnapshot)
	{
		FArcTestGatherCapturesExecution Exec;
		Exec.ExtraAttribute = ExtraAttribute;
		Exec.ExtraSource = ExtraSource;
		Exec.bExtraSnapshot = bSnapshot;
		Exec.OutputAttribute = FArcAttributeSet_TestDerivedStats::GetDamageAttribute();
		return Exec;
	}

	float ApplyAndReadDamage(const FTestEntities& Entities, UArcEffectDefinition* Effect)
	{
		ArcEffects::TryApplyEffect(*EntityManager, Entities.Target, Effect, Entities.Source);
		DrainPendingOps(Entities.Target);

		FMassEntityView TargetView(*EntityManager, Entities.Target);
		const FArcAttributeSet_TestDerivedStats* Derived = TargetView.GetSparseFragmentDataPtr<FArcAttributeSet_TestDerivedStats>();
		return Derived ? Derived->Damage.BaseValue : -1.f;
	}

	FArcScopedModifier MakeScopedAdd(const FArcAttributeCaptureDef& Def, float Amount)
	{
		FArcScopedModifier ScopedMod;
		ScopedMod.CaptureDef = Def;
		ScopedMod.Operation = EArcModifierOp::Add;
		ScopedMod.Type = EArcModifierType::Simple;
		ScopedMod.Magnitude = FArcScalableFloat(Amount);
		return ScopedMod;
	}

	TEST_METHOD(DefaultGatherCaptures_AppendsCapturesAndExtrasWithoutClearing)
	{
		FArcEffectExecution Exec;
		const FArcAttributeCaptureDef Declared = ArcCaptureGatherTestsPrivate::MakeArmorTargetSnapshotDef();
		Exec.Captures.Add(Declared);
		Exec.ExtraCaptures.Add(FArcExtraCaptureDef());

		const FArcAttributeCaptureDef Sentinel = Arc::Capture::MakeRuntimeCaptureDef(
			FArcAttributeSet_TestStats::GetHealthAttribute(), EArcCaptureSource::Source, false);
		TArray<FArcAttributeCaptureDef> OutCaptures;
		OutCaptures.Add(Sentinel);
		TArray<FArcExtraCaptureDef> OutExtras;

		Exec.GatherCaptures(OutCaptures, OutExtras);

		ASSERT_THAT(AreEqual(2, OutCaptures.Num()));
		ASSERT_THAT(IsTrue(OutCaptures[0] == Sentinel));
		ASSERT_THAT(IsTrue(OutCaptures[1] == Declared));
		ASSERT_THAT(AreEqual(1, OutExtras.Num()));
	}

	TEST_METHOD(BuildCaptureLayout_IncludesCaptureAddedByGatherCapturesOverride)
	{
		FArcTestGatherCapturesExecution Exec = MakeExecution(
			FArcAttributeSet_TestStats::GetAttackPowerAttribute(), EArcCaptureSource::Source, false);
		const FArcAttributeCaptureDef Declared = ArcCaptureGatherTestsPrivate::MakeArmorTargetSnapshotDef();
		Exec.Captures.Add(Declared);

		UArcEffectDefinition* Effect = MakeInstantEffect(Exec);

		const FArcEffectCaptureLayout& Layout = Effect->GetCaptureLayout();
		const FArcAttributeCaptureDef Gathered = Arc::Capture::MakeCaptureDef<&FArcAttributeSet_TestStats::GetAttackPowerAttribute, EArcCaptureSource::Source, false>();
		ASSERT_THAT(AreEqual(2, Layout.UniqueCaptures.Num()));
		ASSERT_THAT(IsTrue(Layout.UniqueCaptures[0] == Declared));
		ASSERT_THAT(IsTrue(Layout.UniqueCaptures[1] == Gathered));
	}

	TEST_METHOD(BuildCaptureLayout_ReadsGatherCapturesNotTheCapturesMember)
	{
		FArcTestReplacingGatherExecution Exec;
		Exec.GatheredAttribute = FArcAttributeSet_TestStats::GetAttackPowerAttribute();
		Exec.Captures.Add(ArcCaptureGatherTestsPrivate::MakeArmorTargetSnapshotDef());

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));
		Effect->BuildCaptureLayout();

		const FArcEffectCaptureLayout& Layout = Effect->GetCaptureLayout();
		ASSERT_THAT(AreEqual(1, Layout.UniqueCaptures.Num()));
		const FArcAttributeCaptureDef Gathered = Arc::Capture::MakeCaptureDef<&FArcAttributeSet_TestStats::GetAttackPowerAttribute, EArcCaptureSource::Source, true>();
		ASSERT_THAT(IsTrue(Layout.UniqueCaptures[0] == Gathered));
		ASSERT_THAT(IsFalse(Layout.UniqueCaptures[0] == ArcCaptureGatherTestsPrivate::MakeArmorTargetSnapshotDef()));
	}

	TEST_METHOD(ScopedModifier_TargetsLiveCaptureAddedByGatherCaptures)
	{
		const FTestEntities Entities = CreateEntities();

		FArcTestGatherCapturesExecution Exec = MakeExecution(
			FArcAttributeSet_TestStats::GetAttackPowerAttribute(), EArcCaptureSource::Source, false);
		Exec.ScopedModifiers.Add(MakeScopedAdd(Exec.GetExtraDef(), ArcCaptureGatherTestsPrivate::ScopedAddition));

		const float Damage = ApplyAndReadDamage(Entities, MakeInstantEffect(Exec));

		ASSERT_THAT(IsNear(ArcCaptureGatherTestsPrivate::SourceAttackPower + ArcCaptureGatherTestsPrivate::ScopedAddition, Damage, 0.001f));
	}

	TEST_METHOD(ScopedModifier_TargetsSnapshotCaptureAddedByGatherCaptures)
	{
		const FTestEntities Entities = CreateEntities();

		FArcTestGatherCapturesExecution Exec = MakeExecution(
			FArcAttributeSet_TestStats::GetAttackPowerAttribute(), EArcCaptureSource::Source, true);
		Exec.ScopedModifiers.Add(MakeScopedAdd(Exec.GetExtraDef(), ArcCaptureGatherTestsPrivate::ScopedAddition));

		const float Damage = ApplyAndReadDamage(Entities, MakeInstantEffect(Exec));

		ASSERT_THAT(IsNear(ArcCaptureGatherTestsPrivate::SourceAttackPower + ArcCaptureGatherTestsPrivate::ScopedAddition, Damage, 0.001f));
	}

	TEST_METHOD(ScopedModifier_TargetsTargetCaptureAddedByGatherCaptures)
	{
		const FTestEntities Entities = CreateEntities();

		FArcTestGatherCapturesExecution Exec = MakeExecution(
			FArcAttributeSet_TestStats::GetArmorAttribute(), EArcCaptureSource::Target, false);
		Exec.ScopedModifiers.Add(MakeScopedAdd(Exec.GetExtraDef(), 5.f));

		const float Damage = ApplyAndReadDamage(Entities, MakeInstantEffect(Exec));

		ASSERT_THAT(IsNear(ArcCaptureGatherTestsPrivate::TargetArmor + 5.f, Damage, 0.001f));
	}

	TEST_METHOD(ScopedModifier_NotMatchingAnyGatheredCapture_IsSkipped)
	{
		const FTestEntities Entities = CreateEntities();

		FArcTestGatherCapturesExecution Exec = MakeExecution(
			FArcAttributeSet_TestStats::GetAttackPowerAttribute(), EArcCaptureSource::Source, false);
		const FArcAttributeCaptureDef NotGathered = Arc::Capture::MakeRuntimeCaptureDef(
			FArcAttributeSet_TestStats::GetAttackPowerAttribute(), EArcCaptureSource::Source, true);
		Exec.ScopedModifiers.Add(MakeScopedAdd(NotGathered, ArcCaptureGatherTestsPrivate::ScopedAddition));

		const float Damage = ApplyAndReadDamage(Entities, MakeInstantEffect(Exec));

		ASSERT_THAT(IsNear(ArcCaptureGatherTestsPrivate::SourceAttackPower, Damage, 0.001f));
	}

	TEST_METHOD(MakeRuntimeCaptureDef_EqualsMakeCaptureDef_ForEverySourceAndSnapshotCombination)
	{
		const FArcAttributeRef Armor = FArcAttributeSet_TestStats::GetArmorAttribute();

		const FArcAttributeCaptureDef SourceSnapshot = Arc::Capture::MakeCaptureDef<&FArcAttributeSet_TestStats::GetArmorAttribute, EArcCaptureSource::Source, true>();
		const FArcAttributeCaptureDef SourceLive = Arc::Capture::MakeCaptureDef<&FArcAttributeSet_TestStats::GetArmorAttribute, EArcCaptureSource::Source, false>();
		const FArcAttributeCaptureDef TargetSnapshot = Arc::Capture::MakeCaptureDef<&FArcAttributeSet_TestStats::GetArmorAttribute, EArcCaptureSource::Target, true>();
		const FArcAttributeCaptureDef TargetLive = Arc::Capture::MakeCaptureDef<&FArcAttributeSet_TestStats::GetArmorAttribute, EArcCaptureSource::Target, false>();

		const FArcAttributeCaptureDef RuntimeSourceSnapshot = Arc::Capture::MakeRuntimeCaptureDef(Armor, EArcCaptureSource::Source, true);
		const FArcAttributeCaptureDef RuntimeSourceLive = Arc::Capture::MakeRuntimeCaptureDef(Armor, EArcCaptureSource::Source, false);
		const FArcAttributeCaptureDef RuntimeTargetSnapshot = Arc::Capture::MakeRuntimeCaptureDef(Armor, EArcCaptureSource::Target, true);
		const FArcAttributeCaptureDef RuntimeTargetLive = Arc::Capture::MakeRuntimeCaptureDef(Armor, EArcCaptureSource::Target, false);

		ASSERT_THAT(IsTrue(SourceSnapshot == RuntimeSourceSnapshot));
		ASSERT_THAT(IsTrue(SourceLive == RuntimeSourceLive));
		ASSERT_THAT(IsTrue(TargetSnapshot == RuntimeTargetSnapshot));
		ASSERT_THAT(IsTrue(TargetLive == RuntimeTargetLive));

		ASSERT_THAT(AreEqual(GetTypeHash(SourceSnapshot), GetTypeHash(RuntimeSourceSnapshot)));
		ASSERT_THAT(AreEqual(GetTypeHash(SourceLive), GetTypeHash(RuntimeSourceLive)));
		ASSERT_THAT(AreEqual(GetTypeHash(TargetSnapshot), GetTypeHash(RuntimeTargetSnapshot)));
		ASSERT_THAT(AreEqual(GetTypeHash(TargetLive), GetTypeHash(RuntimeTargetLive)));
	}

	TEST_METHOD(MakeRuntimeCaptureDef_SetsEveryIdentityField)
	{
		const FArcAttributeRef AttackPower = FArcAttributeSet_TestStats::GetAttackPowerAttribute();

		const FArcAttributeCaptureDef Def = Arc::Capture::MakeRuntimeCaptureDef(AttackPower, EArcCaptureSource::Target, false);

		ASSERT_THAT(IsTrue(Def.Attribute == AttackPower));
		ASSERT_THAT(IsTrue(Def.CaptureSource == EArcCaptureSource::Target));
		ASSERT_THAT(IsFalse(Def.bSnapshot));
	}

	TEST_METHOD(MakeRuntimeCaptureDef_DiffersFromCompileTimeDefWhenAnyIdentityFieldDiffers)
	{
		const FArcAttributeCaptureDef Reference = Arc::Capture::MakeCaptureDef<&FArcAttributeSet_TestStats::GetArmorAttribute, EArcCaptureSource::Source, true>();

		const FArcAttributeCaptureDef OtherSnapshot = Arc::Capture::MakeRuntimeCaptureDef(
			FArcAttributeSet_TestStats::GetArmorAttribute(), EArcCaptureSource::Source, false);
		const FArcAttributeCaptureDef OtherSource = Arc::Capture::MakeRuntimeCaptureDef(
			FArcAttributeSet_TestStats::GetArmorAttribute(), EArcCaptureSource::Target, true);
		const FArcAttributeCaptureDef OtherAttribute = Arc::Capture::MakeRuntimeCaptureDef(
			FArcAttributeSet_TestStats::GetHealthAttribute(), EArcCaptureSource::Source, true);

		ASSERT_THAT(IsFalse(Reference == OtherSnapshot));
		ASSERT_THAT(IsFalse(Reference == OtherSource));
		ASSERT_THAT(IsFalse(Reference == OtherAttribute));
	}
};
