// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcConditionTickProcessors.h"

#include "ArcConditionFragments.h"
#include "ArcConditionTypes.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

// ---------------------------------------------------------------------------
// Macro: Implements a per-condition tick processor.
// All share identical logic — decay, overload, burnout, hysteresis.
// ---------------------------------------------------------------------------

#define ARC_IMPLEMENT_CONDITION_TICK_PROCESSOR(Name) \
	UArc##Name##ConditionTickProcessor::UArc##Name##ConditionTickProcessor() \
		: EntityQuery{*this} \
	{ \
		bAutoRegisterWithProcessingPhases = true; \
		bRequiresGameThreadExecution = false; \
		ProcessingPhase = EMassProcessingPhase::PrePhysics; \
		ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone); \
	} \
	\
	void UArc##Name##ConditionTickProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) \
	{ \
		EntityQuery.AddRequirement<FArc##Name##ConditionFragment>(EMassFragmentAccess::ReadWrite); \
		EntityQuery.AddConstSharedRequirement<FArc##Name##ConditionConfig>(EMassFragmentPresence::All); \
	} \
	\
	void UArc##Name##ConditionTickProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) \
	{ \
		const float DeltaTime = Context.GetDeltaTimeSeconds(); \
		if (DeltaTime <= 0.f) { return; } \
		\
		TArray<FMassEntityHandle> StateChangedEntities; \
		TArray<FMassEntityHandle> OverloadChangedEntities; \
		\
		EntityQuery.ForEachEntityChunk(Context, \
			[DeltaTime, &StateChangedEntities, &OverloadChangedEntities](FMassExecutionContext& Ctx) \
			{ \
				TArrayView<FArc##Name##ConditionFragment> Fragments = Ctx.GetMutableFragmentView<FArc##Name##ConditionFragment>(); \
				const FArc##Name##ConditionConfig& SharedConfig = Ctx.GetConstSharedFragment<FArc##Name##ConditionConfig>(); \
				\
				for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt) \
				{ \
					FArc##Name##ConditionFragment& Fragment = Fragments[EntityIt]; \
					bool bStateChanged = false; \
					bool bOverloadChanged = false; \
					ArcConditionHelpers::TickCondition(Fragment.State, SharedConfig.Config, DeltaTime, bStateChanged, bOverloadChanged); \
					\
					if (bStateChanged)    { StateChangedEntities.Add(Ctx.GetEntity(EntityIt)); } \
					if (bOverloadChanged) { OverloadChangedEntities.Add(Ctx.GetEntity(EntityIt)); } \
				} \
			} \
		); \
		\
		UWorld* World = EntityManager.GetWorld(); \
		UMassSignalSubsystem* SignalSubsystem = World ? World->GetSubsystem<UMassSignalSubsystem>() : nullptr; \
		if (SignalSubsystem) \
		{ \
			if (StateChangedEntities.Num() > 0) \
			{ \
				SignalSubsystem->SignalEntities(UE::ArcConditionEffects::Signals::ConditionStateChanged, StateChangedEntities); \
			} \
			if (OverloadChangedEntities.Num() > 0) \
			{ \
				SignalSubsystem->SignalEntities(UE::ArcConditionEffects::Signals::ConditionOverloadChanged, OverloadChangedEntities); \
			} \
		} \
	}

ARC_IMPLEMENT_CONDITION_TICK_PROCESSOR(Burning)
ARC_IMPLEMENT_CONDITION_TICK_PROCESSOR(Bleeding)
ARC_IMPLEMENT_CONDITION_TICK_PROCESSOR(Chilled)
ARC_IMPLEMENT_CONDITION_TICK_PROCESSOR(Shocked)
ARC_IMPLEMENT_CONDITION_TICK_PROCESSOR(Poisoned)
ARC_IMPLEMENT_CONDITION_TICK_PROCESSOR(Diseased)
ARC_IMPLEMENT_CONDITION_TICK_PROCESSOR(Weakened)
ARC_IMPLEMENT_CONDITION_TICK_PROCESSOR(Oiled)
ARC_IMPLEMENT_CONDITION_TICK_PROCESSOR(Wet)
ARC_IMPLEMENT_CONDITION_TICK_PROCESSOR(Corroded)
ARC_IMPLEMENT_CONDITION_TICK_PROCESSOR(Blinded)
ARC_IMPLEMENT_CONDITION_TICK_PROCESSOR(Suffocating)
ARC_IMPLEMENT_CONDITION_TICK_PROCESSOR(Exhausted)

#undef ARC_IMPLEMENT_CONDITION_TICK_PROCESSOR
