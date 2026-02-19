// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBiologicalConditionProcessor.h"

#include "ArcConditionEffectsSubsystem.h"
#include "ArcConditionFragments.h"
#include "ArcConditionTypes.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

namespace
{
	template<typename ConfigType>
	const FArcConditionConfig* GetOptionalConfig(FMassExecutionContext& Ctx)
	{
		const ConfigType* Cfg = Ctx.GetConstSharedFragmentPtr<ConfigType>();
		return Cfg ? &Cfg->Config : nullptr;
	}

	// -----------------------------------------------------------------------
	// Bleeding Application — blocked by Burning and Frozen
	// -----------------------------------------------------------------------
	void ApplyBleeding(float Amount,
		FArcConditionState* Bleeding,  const FArcConditionConfig* BleedingCfg,
		const FArcConditionState* Burning,
		const FArcConditionState* Chilled)
	{
		if (!Bleeding || !BleedingCfg) { return; }

		// Cannot bleed while burning (blood evaporates)
		if (Burning && Burning->bActive) { return; }

		// Cannot bleed while frozen
		if (Chilled && (Chilled->OverloadPhase == EArcConditionOverloadPhase::Overloaded)) { return; }

		const float Effective = ArcConditionHelpers::ApplyResistance(Amount, Bleeding->Resistance);
		ArcConditionHelpers::SetSaturationClamped(*Bleeding, *BleedingCfg,
			Bleeding->Saturation + Effective);
		ArcConditionHelpers::UpdateActiveFlag(*Bleeding, *BleedingCfg);
	}

	// -----------------------------------------------------------------------
	// Poison Application — boosted by Bleeding (+50% absorption)
	// -----------------------------------------------------------------------
	void ApplyPoisoned(float Amount,
		FArcConditionState* Poisoned, const FArcConditionConfig* PoisonedCfg,
		const FArcConditionState* Bleeding)
	{
		if (!Poisoned || !PoisonedCfg) { return; }

		float Effective = Amount;

		// Absorption: open wounds absorb poison faster
		if (Bleeding && Bleeding->bActive)
		{
			Effective *= 1.5f;
		}

		Effective = ArcConditionHelpers::ApplyResistance(Effective, Poisoned->Resistance);
		ArcConditionHelpers::SetSaturationClamped(*Poisoned, *PoisonedCfg,
			Poisoned->Saturation + Effective);
		ArcConditionHelpers::UpdateActiveFlag(*Poisoned, *PoisonedCfg);
	}

	// -----------------------------------------------------------------------
	// Disease Application — generic for now
	// -----------------------------------------------------------------------
	void ApplyDiseased(float Amount,
		FArcConditionState* Diseased, const FArcConditionConfig* DiseasedCfg)
	{
		if (!Diseased || !DiseasedCfg) { return; }

		const float Effective = ArcConditionHelpers::ApplyResistance(Amount, Diseased->Resistance);
		ArcConditionHelpers::SetSaturationClamped(*Diseased, *DiseasedCfg,
			Diseased->Saturation + Effective);
		ArcConditionHelpers::UpdateActiveFlag(*Diseased, *DiseasedCfg);
	}

	// -----------------------------------------------------------------------
	// Weakened Application — generic
	// -----------------------------------------------------------------------
	void ApplyWeakened(float Amount,
		FArcConditionState* Weakened, const FArcConditionConfig* WeakenedCfg)
	{
		if (!Weakened || !WeakenedCfg) { return; }

		const float Effective = ArcConditionHelpers::ApplyResistance(Amount, Weakened->Resistance);
		ArcConditionHelpers::SetSaturationClamped(*Weakened, *WeakenedCfg,
			Weakened->Saturation + Effective);
		ArcConditionHelpers::UpdateActiveFlag(*Weakened, *WeakenedCfg);
	}
}

// ---------------------------------------------------------------------------
// Processor Implementation
// ---------------------------------------------------------------------------

UArcBiologicalConditionProcessor::UArcBiologicalConditionProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcBiologicalConditionProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcConditionEffects::Signals::BleedingApply);
	SubscribeToSignal(*SignalSubsystem, UE::ArcConditionEffects::Signals::PoisonedApply);
	SubscribeToSignal(*SignalSubsystem, UE::ArcConditionEffects::Signals::DiseasedApply);
	SubscribeToSignal(*SignalSubsystem, UE::ArcConditionEffects::Signals::WeakenedApply);
}

void UArcBiologicalConditionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	// Biological conditions
	EntityQuery.AddRequirement<FArcBleedingConditionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcPoisonedConditionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcDiseasedConditionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcWeakenedConditionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);

	// Cross-domain reads (thermal state affects biological application)
	EntityQuery.AddRequirement<FArcBurningConditionFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcChilledConditionFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);

	// Configs
	EntityQuery.AddConstSharedRequirement<FArcBleedingConditionConfig>(EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcPoisonedConditionConfig>(EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcDiseasedConditionConfig>(EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcWeakenedConditionConfig>(EMassFragmentPresence::Optional);
}

void UArcBiologicalConditionProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UArcConditionEffectsSubsystem* Subsystem = Context.GetWorld()->GetSubsystem<UArcConditionEffectsSubsystem>();
	if (!Subsystem) { return; }

	// Grab biological requests
	TArray<FArcConditionApplicationRequest> BioRequests;
	TArray<FArcConditionApplicationRequest>& AllRequests = Subsystem->GetPendingRequests();
	for (int32 i = AllRequests.Num() - 1; i >= 0; --i)
	{
		const EArcConditionType Type = AllRequests[i].ConditionType;
		if (Type == EArcConditionType::Bleeding || Type == EArcConditionType::Poisoned
			|| Type == EArcConditionType::Diseased || Type == EArcConditionType::Weakened)
		{
			BioRequests.Add(AllRequests[i]);
			AllRequests.RemoveAtSwap(i);
		}
	}

	if (BioRequests.Num() == 0) { return; }

	TMap<FMassEntityHandle, TArray<FArcConditionApplicationRequest*>> EntityRequestMap;
	for (FArcConditionApplicationRequest& Req : BioRequests)
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
			TArrayView<FArcBleedingConditionFragment> BleedingFrags = Ctx.GetMutableFragmentView<FArcBleedingConditionFragment>();
			TArrayView<FArcPoisonedConditionFragment> PoisonedFrags = Ctx.GetMutableFragmentView<FArcPoisonedConditionFragment>();
			TArrayView<FArcDiseasedConditionFragment> DiseasedFrags = Ctx.GetMutableFragmentView<FArcDiseasedConditionFragment>();
			TArrayView<FArcWeakenedConditionFragment> WeakenedFrags = Ctx.GetMutableFragmentView<FArcWeakenedConditionFragment>();

			// Cross-domain reads
			TConstArrayView<FArcBurningConditionFragment> BurningFrags = Ctx.GetFragmentView<FArcBurningConditionFragment>();
			TConstArrayView<FArcChilledConditionFragment> ChilledFrags = Ctx.GetFragmentView<FArcChilledConditionFragment>();

			const FArcConditionConfig* BleedingCfg = GetOptionalConfig<FArcBleedingConditionConfig>(Ctx);
			const FArcConditionConfig* PoisonedCfg = GetOptionalConfig<FArcPoisonedConditionConfig>(Ctx);
			const FArcConditionConfig* DiseasedCfg = GetOptionalConfig<FArcDiseasedConditionConfig>(Ctx);
			const FArcConditionConfig* WeakenedCfg = GetOptionalConfig<FArcWeakenedConditionConfig>(Ctx);

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				TArray<FArcConditionApplicationRequest*>* Requests = EntityRequestMap.Find(Entity);
				if (!Requests) { continue; }

				const int32 Idx = EntityIt.GetIndex();

				FArcConditionState* Bleeding = !BleedingFrags.IsEmpty() ? &BleedingFrags[Idx].State : nullptr;
				FArcConditionState* Poisoned = !PoisonedFrags.IsEmpty() ? &PoisonedFrags[Idx].State : nullptr;
				FArcConditionState* Diseased = !DiseasedFrags.IsEmpty() ? &DiseasedFrags[Idx].State : nullptr;
				FArcConditionState* Weakened = !WeakenedFrags.IsEmpty() ? &WeakenedFrags[Idx].State : nullptr;

				const FArcConditionState* Burning = !BurningFrags.IsEmpty() ? &BurningFrags[Idx].State : nullptr;
				const FArcConditionState* Chilled = !ChilledFrags.IsEmpty() ? &ChilledFrags[Idx].State : nullptr;

				// Snapshot
				auto Snapshot = [](const FArcConditionState* S) -> TPair<bool, EArcConditionOverloadPhase>
				{
					return S ? TPair<bool, EArcConditionOverloadPhase>{S->bActive, S->OverloadPhase}
					         : TPair<bool, EArcConditionOverloadPhase>{false, EArcConditionOverloadPhase::None};
				};

				FArcConditionState* WriteStates[] = { Bleeding, Poisoned, Diseased, Weakened };
				TPair<bool, EArcConditionOverloadPhase> Before[4];
				for (int32 i = 0; i < 4; ++i) { Before[i] = Snapshot(WriteStates[i]); }

				for (const FArcConditionApplicationRequest* Req : *Requests)
				{
					if (Req->Amount < 0.f)
					{
						// Direct removal
						FArcConditionState* Target = nullptr;
						const FArcConditionConfig* Cfg = nullptr;
						switch (Req->ConditionType)
						{
						case EArcConditionType::Bleeding: Target = Bleeding; Cfg = BleedingCfg; break;
						case EArcConditionType::Poisoned: Target = Poisoned; Cfg = PoisonedCfg; break;
						case EArcConditionType::Diseased: Target = Diseased; Cfg = DiseasedCfg; break;
						case EArcConditionType::Weakened: Target = Weakened; Cfg = WeakenedCfg; break;
						default: break;
						}
						if (Target && Cfg)
						{
							ArcConditionHelpers::SetSaturationClamped(*Target, *Cfg, Target->Saturation + Req->Amount);
							ArcConditionHelpers::UpdateActiveFlag(*Target, *Cfg);
						}
						continue;
					}

					switch (Req->ConditionType)
					{
					case EArcConditionType::Bleeding:
						ApplyBleeding(Req->Amount, Bleeding, BleedingCfg, Burning, Chilled);
						break;
					case EArcConditionType::Poisoned:
						ApplyPoisoned(Req->Amount, Poisoned, PoisonedCfg, Bleeding);
						break;
					case EArcConditionType::Diseased:
						ApplyDiseased(Req->Amount, Diseased, DiseasedCfg);
						break;
					case EArcConditionType::Weakened:
						ApplyWeakened(Req->Amount, Weakened, WeakenedCfg);
						break;
					default: break;
					}
				}

				bool bStateChanged = false, bOverloadChanged = false;
				for (int32 i = 0; i < 4; ++i)
				{
					auto After = Snapshot(WriteStates[i]);
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
		if (StateChangedEntities.Num() > 0)   { SignalSubsystem->SignalEntities(UE::ArcConditionEffects::Signals::ConditionStateChanged, StateChangedEntities); }
		if (OverloadChangedEntities.Num() > 0) { SignalSubsystem->SignalEntities(UE::ArcConditionEffects::Signals::ConditionOverloadChanged, OverloadChangedEntities); }
	}
}
