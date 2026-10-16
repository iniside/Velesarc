// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CQTest.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "ArcEffectLifecycleTestTypes.h"
#include "ArcScalableFloat.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectDurationProcessor.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectSpec.h"
#include "Events/ArcEffectEventPayloads.h"
#include "Events/ArcGameEvent.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcImmunityFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Mass/EntityBuilder.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "Processors/ArcEffectInhibitionProcessor.h"
#include "Signals/ArcAttributeRecalculateSignal.h"

struct FArcEffectLifecycleFixture
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcEffectDurationProcessor* DurationProcessor = nullptr;
	UArcEffectInhibitionProcessor* InhibitionProcessor = nullptr;
	TArray<FArcGameEvent> CapturedEvents;

	void Setup()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MassSubsystem = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MassSubsystem);
		EntityManager = &MassSubsystem->GetMutableEntityManager();

		SignalSubsystem = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
		check(SignalSubsystem);

		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		DurationProcessor = NewObject<UArcEffectDurationProcessor>();
		DurationProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);
		InhibitionProcessor = NewObject<UArcEffectInhibitionProcessor>();
		InhibitionProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);

		ArcEffectLifecycleTest::GetHookLog().Reset();
		CapturedEvents.Reset();
	}

	FMassEntityHandle MakeEntity()
	{
		FArcAttributeSet_TestStats Stats;
		UE::Mass::FEntityBuilder Builder(*EntityManager);
		return Builder
			.Add<FArcAttributeSet_TestStats>(Stats)
			.Add<FArcEffectStackFragment>()
			.Add<FArcAggregatorFragment>()
			.Add<FArcOwnedTagsFragment>()
			.Add<FArcPendingAttributeOpsFragment>()
			.Add<FArcImmunityFragment>()
			.Commit();
	}

	void TickDuration(float DeltaTime)
	{
		FMassProcessingContext ProcessingContext(*EntityManager, DeltaTime);
		UE::Mass::Executor::Run(*DurationProcessor, ProcessingContext);
	}

	void RunInhibition(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AttributeRecalculate, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*InhibitionProcessor, ProcessingContext);
	}

	FArcEffectStackFragment* GetStack(FMassEntityHandle Entity)
	{
		FMassEntityView View(*EntityManager, Entity);
		return View.GetFragmentDataPtr<FArcEffectStackFragment>();
	}

	FArcActiveEffect* FindActive(FMassEntityHandle Entity, FArcActiveEffectHandle Handle)
	{
		FArcEffectStackFragment* Stack = GetStack(Entity);
		return Stack ? ArcEffects::FindActiveEffect(*Stack, Handle) : nullptr;
	}

	int32 NumActive(FMassEntityHandle Entity)
	{
		const FArcEffectStackFragment* Stack = GetStack(Entity);
		return Stack ? Stack->ActiveEffects.Num() : -1;
	}

	const FArcImmunityFragment* GetImmunity(FMassEntityHandle Entity)
	{
		FMassEntityView View(*EntityManager, Entity);
		return View.GetSparseFragmentDataPtr<FArcImmunityFragment>();
	}

	int32 CountMods(FMassEntityHandle Entity, const FArcAttributeRef& Attribute)
	{
		FMassEntityView View(*EntityManager, Entity);
		const FArcAggregatorFragment* Aggregators = View.GetFragmentDataPtr<FArcAggregatorFragment>();
		const FArcAggregator* Aggregator = Aggregators ? Aggregators->FindAggregator(Attribute) : nullptr;
		if (!Aggregator)
		{
			return 0;
		}

		int32 Total = 0;
		for (int32 OpIndex = 0; OpIndex < static_cast<int32>(EArcModifierOp::Max); ++OpIndex)
		{
			Total += Aggregator->Mods[OpIndex].Num();
		}
		return Total;
	}

	float FirstAddMagnitude(FMassEntityHandle Entity, const FArcAttributeRef& Attribute)
	{
		FMassEntityView View(*EntityManager, Entity);
		const FArcAggregatorFragment* Aggregators = View.GetFragmentDataPtr<FArcAggregatorFragment>();
		const FArcAggregator* Aggregator = Aggregators ? Aggregators->FindAggregator(Attribute) : nullptr;
		if (!Aggregator || Aggregator->Mods[static_cast<int32>(EArcModifierOp::Add)].IsEmpty())
		{
			return 0.f;
		}
		return Aggregator->Mods[static_cast<int32>(EArcModifierOp::Add)][0].Magnitude;
	}

	// Empty when every effect satisfies: mods are in the aggregator iff !bInhibited.
	FString CheckModInvariant(FMassEntityHandle Entity)
	{
		const FArcEffectStackFragment* Stack = GetStack(Entity);
		if (!Stack)
		{
			return TEXT("no stack fragment");
		}

		int32 ExpectedAggregatorMods = 0;
		for (const FArcActiveEffect& Active : Stack->ActiveEffects)
		{
			const FArcEffectSpec* Spec = Active.Spec.GetPtr<FArcEffectSpec>();
			if (!Spec || !Spec->Definition)
			{
				continue;
			}

			int32 DefinitionMods = 0;
			for (const FArcEffectModifier& Modifier : Spec->Definition->Modifiers)
			{
				if (Modifier.Attribute.IsValid())
				{
					++DefinitionMods;
				}
			}

			const bool bShouldHaveMods = !Active.bInhibited;
			const int32 HeldMods = Active.ModifierHandles.Num();
			if (bShouldHaveMods && HeldMods != DefinitionMods)
			{
				return FString::Printf(TEXT("active effect inhibited=%d should hold %d mod handles but holds %d"),
					Active.bInhibited ? 1 : 0, DefinitionMods, HeldMods);
			}
			if (!bShouldHaveMods && HeldMods != 0)
			{
				return FString::Printf(TEXT("active effect inhibited=%d should hold no mod handles but holds %d"),
					Active.bInhibited ? 1 : 0, HeldMods);
			}
			if (bShouldHaveMods)
			{
				ExpectedAggregatorMods += DefinitionMods;
			}
		}

		const int32 AggregatorMods = CountMods(Entity, FArcAttributeSet_TestStats::GetArmorAttribute());
		if (AggregatorMods != ExpectedAggregatorMods)
		{
			return FString::Printf(TEXT("aggregator holds %d armor mods but %d are expected"), AggregatorMods, ExpectedAggregatorMods);
		}
		return FString();
	}

	static FArcEffectModifier MakeArmorModifier(float Magnitude)
	{
		FArcEffectModifier Modifier;
		Modifier.Attribute = FArcAttributeSet_TestStats::GetArmorAttribute();
		Modifier.Operation = EArcModifierOp::Add;
		Modifier.Magnitude = FArcScalableFloat(Magnitude);
		return Modifier;
	}

	static UArcEffectDefinition* MakeDefinition(EArcEffectDuration DurationType, float Duration, float ArmorAdd)
	{
		UArcEffectDefinition* Definition = NewObject<UArcEffectDefinition>();
		Definition->StackingPolicy.DurationType = DurationType;
		Definition->StackingPolicy.Duration = Duration;
		if (ArmorAdd != 0.f)
		{
			Definition->Modifiers.Add(MakeArmorModifier(ArmorAdd));
		}
		return Definition;
	}

	template <typename TComponent>
	static void AddComponent(UArcEffectDefinition* Definition, const TComponent& Component)
	{
		Definition->Components.Add(FInstancedStruct::Make(Component));
	}

	static void AddRecorder(UArcEffectDefinition* Definition, FName Label)
	{
		FArcTestComp_Recorder Recorder;
		Recorder.Label = Label;
		AddComponent(Definition, Recorder);
	}

	static void AddTaskRecorder(UArcEffectDefinition* Definition, FName Label)
	{
		FArcTestTask_RemovalRecorder Task;
		Task.Label = Label;
		Definition->Tasks.Add(FInstancedStruct::Make(Task));
	}

	static void AddIdentity(UArcEffectDefinition* Definition, FGameplayTag Tag)
	{
		FArcTestComp_Identity Identity;
		Identity.Tags.AddTag(Tag);
		AddComponent(Definition, Identity);
	}

	int32 CountHooks(ArcEffectLifecycleTest::EHookKind Kind, FName Label = NAME_None) const
	{
		int32 Count = 0;
		for (const ArcEffectLifecycleTest::FHookRecord& Record : ArcEffectLifecycleTest::GetHookLog())
		{
			if (Record.Kind == Kind && (Label.IsNone() || Record.Label == Label))
			{
				++Count;
			}
		}
		return Count;
	}

	const ArcEffectLifecycleTest::FHookRecord* FindHook(ArcEffectLifecycleTest::EHookKind Kind, FName Label = NAME_None, int32 Nth = 0) const
	{
		int32 Seen = 0;
		for (const ArcEffectLifecycleTest::FHookRecord& Record : ArcEffectLifecycleTest::GetHookLog())
		{
			if (Record.Kind == Kind && (Label.IsNone() || Record.Label == Label))
			{
				if (Seen == Nth)
				{
					return &Record;
				}
				++Seen;
			}
		}
		return nullptr;
	}

	const ArcEffectLifecycleTest::FHookRecord* FindLastHook(ArcEffectLifecycleTest::EHookKind Kind) const
	{
		const TArray<ArcEffectLifecycleTest::FHookRecord>& Log = ArcEffectLifecycleTest::GetHookLog();
		for (int32 Index = Log.Num() - 1; Index >= 0; --Index)
		{
			if (Log[Index].Kind == Kind)
			{
				return &Log[Index];
			}
		}
		return nullptr;
	}

	int32 IndexOfHook(ArcEffectLifecycleTest::EHookKind Kind, FName Label = NAME_None) const
	{
		const TArray<ArcEffectLifecycleTest::FHookRecord>& Log = ArcEffectLifecycleTest::GetHookLog();
		for (int32 Index = 0; Index < Log.Num(); ++Index)
		{
			if (Log[Index].Kind == Kind && (Label.IsNone() || Log[Index].Label == Label))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	void CaptureEvents(FGameplayTag Tag)
	{
		UArcGameEventSubsystem* Bus = Spawner.GetWorld().GetSubsystem<UArcGameEventSubsystem>();
		check(Bus);
		Bus->Subscribe(Tag, FMassEntityHandle(), EArcEventRole::Any, FArcGameEventDelegate::CreateRaw(this, &FArcEffectLifecycleFixture::HandleEvent));
	}

	void DrainEvents()
	{
		UArcGameEventSubsystem* Bus = Spawner.GetWorld().GetSubsystem<UArcGameEventSubsystem>();
		check(Bus);
		Bus->DrainOnPostActorTick(&Spawner.GetWorld(), LEVELTICK_All, 0.f);
	}

	void HandleEvent(const FArcGameEvent& Event)
	{
		CapturedEvents.Add(Event);
	}
};
