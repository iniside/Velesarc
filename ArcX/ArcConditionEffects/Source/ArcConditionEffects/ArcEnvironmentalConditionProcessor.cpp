// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEnvironmentalConditionProcessor.h"

#include "ArcConditionEffectsSubsystem.h"
#include "ArcConditionFragments.h"
#include "ArcConditionTypes.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"



// ---------------------------------------------------------------------------
// Processor Implementation
// ---------------------------------------------------------------------------

UArcEnvironmentalConditionProcessor::UArcEnvironmentalConditionProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcEnvironmentalConditionProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcConditionEffects::Signals::BlindedApply);
	SubscribeToSignal(*SignalSubsystem, UE::ArcConditionEffects::Signals::SuffocatingApply);
	SubscribeToSignal(*SignalSubsystem, UE::ArcConditionEffects::Signals::ExhaustedApply);
	SubscribeToSignal(*SignalSubsystem, UE::ArcConditionEffects::Signals::CorrodedApply);
	SubscribeToSignal(*SignalSubsystem, UE::ArcConditionEffects::Signals::ShockedApply);
}

void UArcEnvironmentalConditionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcBlindedConditionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcSuffocatingConditionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcExhaustedConditionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcCorrodedConditionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcShockedConditionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);

	EntityQuery.AddConstSharedRequirement<FArcBlindedConditionConfig>(EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcSuffocatingConditionConfig>(EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcExhaustedConditionConfig>(EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcCorrodedConditionConfig>(EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcShockedConditionConfig>(EMassFragmentPresence::Optional);
}

void UArcEnvironmentalConditionProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UArcConditionEffectsSubsystem* Subsystem = Context.GetWorld()->GetSubsystem<UArcConditionEffectsSubsystem>();
	if (!Subsystem) { return; }

	// Grab environmental requests
	TArray<FArcConditionApplicationRequest> EnvRequests;
	TArray<FArcConditionApplicationRequest>& AllRequests = Subsystem->GetPendingRequests();
	for (int32 i = AllRequests.Num() - 1; i >= 0; --i)
	{
		const EArcConditionType Type = AllRequests[i].ConditionType;
		if (Type == EArcConditionType::Blinded || Type == EArcConditionType::Suffocating
			|| Type == EArcConditionType::Exhausted || Type == EArcConditionType::Corroded
			|| Type == EArcConditionType::Shocked)
		{
			EnvRequests.Add(AllRequests[i]);
			AllRequests.RemoveAtSwap(i);
		}
	}

	if (EnvRequests.Num() == 0) { return; }

	TMap<FMassEntityHandle, TArray<FArcConditionApplicationRequest*>> EntityRequestMap;
	for (FArcConditionApplicationRequest& Req : EnvRequests)
	{
		if (EntityManager.IsEntityValid(Req.Entity))
		{
			EntityRequestMap.FindOrAdd(Req.Entity).Add(&Req);
		}
	}

	TArray<FMassEntityHandle> StateChangedEntities;
	TArray<FMassEntityHandle> OverloadChangedEntities;

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityRequestMap, &StateChangedEntities, &OverloadChangedEntities](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcBlindedConditionFragment>     BlindedFrags     = Ctx.GetMutableFragmentView<FArcBlindedConditionFragment>();
			TArrayView<FArcSuffocatingConditionFragment>  SuffocatingFrags = Ctx.GetMutableFragmentView<FArcSuffocatingConditionFragment>();
			TArrayView<FArcExhaustedConditionFragment>    ExhaustedFrags   = Ctx.GetMutableFragmentView<FArcExhaustedConditionFragment>();
			TArrayView<FArcCorrodedConditionFragment>     CorrodedFrags    = Ctx.GetMutableFragmentView<FArcCorrodedConditionFragment>();
			TArrayView<FArcShockedConditionFragment>      ShockedFrags     = Ctx.GetMutableFragmentView<FArcShockedConditionFragment>();

			const FArcConditionConfig* BlindedCfg     = Arc::Condition::GetOptionalConfig<FArcBlindedConditionConfig>(Ctx);
			const FArcConditionConfig* SuffocatingCfg = Arc::Condition::GetOptionalConfig<FArcSuffocatingConditionConfig>(Ctx);
			const FArcConditionConfig* ExhaustedCfg   = Arc::Condition::GetOptionalConfig<FArcExhaustedConditionConfig>(Ctx);
			const FArcConditionConfig* CorrodedCfg    = Arc::Condition::GetOptionalConfig<FArcCorrodedConditionConfig>(Ctx);
			const FArcConditionConfig* ShockedCfg     = Arc::Condition::GetOptionalConfig<FArcShockedConditionConfig>(Ctx);

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				TArray<FArcConditionApplicationRequest*>* Requests = EntityRequestMap.Find(Entity);
				if (!Requests) { continue; }

				const int32 Idx = *EntityIt;

				FArcConditionState* Blinded     = !BlindedFrags.IsEmpty()     ? &BlindedFrags[Idx].State     : nullptr;
				FArcConditionState* Suffocating = !SuffocatingFrags.IsEmpty() ? &SuffocatingFrags[Idx].State : nullptr;
				FArcConditionState* Exhausted   = !ExhaustedFrags.IsEmpty()   ? &ExhaustedFrags[Idx].State   : nullptr;
				FArcConditionState* Corroded    = !CorrodedFrags.IsEmpty()    ? &CorrodedFrags[Idx].State    : nullptr;
				FArcConditionState* Shocked     = !ShockedFrags.IsEmpty()     ? &ShockedFrags[Idx].State     : nullptr;

				// Snapshot for change detection
				auto Snapshot = [](const FArcConditionState* S) -> TPair<bool, EArcConditionOverloadPhase>
				{
					return S ? TPair<bool, EArcConditionOverloadPhase>{S->bActive, S->OverloadPhase}
					         : TPair<bool, EArcConditionOverloadPhase>{false, EArcConditionOverloadPhase::None};
				};

				FArcConditionState* AllStates[] = { Blinded, Suffocating, Exhausted, Corroded, Shocked };
				TPair<bool, EArcConditionOverloadPhase> Before[5];
				for (int32 i = 0; i < 5; ++i) { Before[i] = Snapshot(AllStates[i]); }

				for (const FArcConditionApplicationRequest* Req : *Requests)
				{
					FArcConditionState* Target = nullptr;
					const FArcConditionConfig* Cfg = nullptr;

					switch (Req->ConditionType)
					{
					case EArcConditionType::Blinded:     Target = Blinded;     Cfg = BlindedCfg;     break;
					case EArcConditionType::Suffocating: Target = Suffocating; Cfg = SuffocatingCfg; break;
					case EArcConditionType::Exhausted:   Target = Exhausted;   Cfg = ExhaustedCfg;   break;
					case EArcConditionType::Corroded:    Target = Corroded;    Cfg = CorrodedCfg;    break;
					case EArcConditionType::Shocked:     Target = Shocked;     Cfg = ShockedCfg;     break;
					default: break;
					}

					if (Target && Cfg)
					{
						if (Req->Amount < 0.f)
						{
							ArcConditionHelpers::SetSaturationClamped(*Target, *Cfg, Target->Saturation + Req->Amount);
						}
						else
						{
							Arc::Condition::ApplyGenericCondition(Req->Amount, Target, Cfg);
						}
						ArcConditionHelpers::UpdateActiveFlag(*Target, *Cfg);
					}
				}

				bool bStateChanged = false, bOverloadChanged = false;
				for (int32 i = 0; i < 5; ++i)
				{
					auto After = Snapshot(AllStates[i]);
					if (Before[i].Key != After.Key) { bStateChanged = true; }
					if (Before[i].Value != After.Value) { bOverloadChanged = true; }
				}

				if (bStateChanged) { StateChangedEntities.Add(Entity); }
				if (bOverloadChanged) { OverloadChangedEntities.Add(Entity); }
			}
		}
	);

	UWorld* World = EntityManager.GetWorld();
	UMassSignalSubsystem* SignalSubsystem = World ? World->GetSubsystem<UMassSignalSubsystem>() : nullptr;
	if (SignalSubsystem)
	{
		if (StateChangedEntities.Num() > 0)    { SignalSubsystem->SignalEntities(UE::ArcConditionEffects::Signals::ConditionStateChanged, StateChangedEntities); }
		if (OverloadChangedEntities.Num() > 0) { SignalSubsystem->SignalEntities(UE::ArcConditionEffects::Signals::ConditionOverloadChanged, OverloadChangedEntities); }
	}
}
