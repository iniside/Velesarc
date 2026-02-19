// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcThermalConditionProcessor.h"

#include "ArcConditionEffectsSubsystem.h"
#include "ArcConditionFragments.h"
#include "ArcConditionTypes.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

// ---------------------------------------------------------------------------
// Helper: get optional fragment state from chunk, returns nullptr if absent
// ---------------------------------------------------------------------------

namespace
{
	template<typename FragmentType>
	FArcConditionState* GetOptionalState(FMassExecutionContext& Ctx, int32 EntityIndex)
	{
		TArrayView<FragmentType> View = Ctx.GetMutableFragmentView<FragmentType>();
		if (View.IsEmpty()) { return nullptr; }
		return &View[EntityIndex].State;
	}

	template<typename FragmentType>
	const FArcConditionState* GetOptionalStateConst(FMassExecutionContext& Ctx, int32 EntityIndex)
	{
		TConstArrayView<FragmentType> View = Ctx.GetFragmentView<FragmentType>();
		if (View.IsEmpty()) { return nullptr; }
		return &View[EntityIndex].State;
	}

	template<typename ConfigType>
	const FArcConditionConfig* GetOptionalConfig(FMassExecutionContext& Ctx)
	{
		const ConfigType* Cfg = Ctx.GetConstSharedFragmentPtr<ConfigType>();
		return Cfg ? &Cfg->Config : nullptr;
	}

	// -----------------------------------------------------------------------
	// Fire Application
	// -----------------------------------------------------------------------
	void ApplyFire(float Amount,
		FArcConditionState* Burning,  const FArcConditionConfig* BurningCfg,
		FArcConditionState* Chilled,  const FArcConditionConfig* ChilledCfg,
		FArcConditionState* Wet,      const FArcConditionConfig* WetCfg,
		FArcConditionState* Oiled,    const FArcConditionConfig* OiledCfg,
		FArcConditionState* Bleeding,
		FArcConditionState* Diseased,
		FArcConditionState* Blinded,  const FArcConditionConfig* BlindedCfg)
	{
		float RemainingFire = Amount;

		// Cauterization: Fire removes Bleeding
		if (Bleeding && Bleeding->Saturation > 0.f)
		{
			ArcConditionHelpers::ClearCondition(*Bleeding);
		}

		// Sterilization: Fire removes Disease
		if (Diseased && Diseased->Saturation > 0.f)
		{
			ArcConditionHelpers::ClearCondition(*Diseased);
		}

		// Fire on Frozen: shatter ice block, generate steam
		if (Chilled && Chilled->OverloadPhase == EArcConditionOverloadPhase::Overloaded && ChilledCfg)
		{
			// Shatter: end frozen overload, generate blinding steam
			if (Blinded && BlindedCfg)
			{
				ArcConditionHelpers::SetSaturationClamped(*Blinded, *BlindedCfg,
					Blinded->Saturation + Chilled->Saturation * 0.5f);
				ArcConditionHelpers::UpdateActiveFlag(*Blinded, *BlindedCfg);
			}
			ArcConditionHelpers::ClearCondition(*Chilled);
			// Fire energy is spent shattering — reduced by half
			RemainingFire *= 0.5f;
		}

		// Thermal Cascade Step 1: Evaporate Wet
		if (Wet && Wet->Saturation > 0.f && RemainingFire > 0.f && WetCfg)
		{
			const float WetRemoved = FMath::Min(Wet->Saturation, RemainingFire);
			Wet->Saturation -= WetRemoved;
			RemainingFire -= WetRemoved;

			// Steam -> Blinded
			if (Blinded && BlindedCfg)
			{
				ArcConditionHelpers::SetSaturationClamped(*Blinded, *BlindedCfg,
					Blinded->Saturation + WetRemoved * 0.5f);
				ArcConditionHelpers::UpdateActiveFlag(*Blinded, *BlindedCfg);
			}

			ArcConditionHelpers::UpdateActiveFlag(*Wet, *WetCfg);
		}

		// Thermal Cascade Step 2: Melt Chilled (x2 cost)
		if (Chilled && Chilled->Saturation > 0.f && RemainingFire > 0.f && ChilledCfg)
		{
			const float ColdRemovable = RemainingFire * 2.f;
			const float ColdRemoved = FMath::Min(Chilled->Saturation, ColdRemovable);
			Chilled->Saturation -= ColdRemoved;
			RemainingFire -= ColdRemoved / 2.f;

			if (Chilled->Saturation <= 0.f)
			{
				ArcConditionHelpers::ClearCondition(*Chilled);

				// Melted ice becomes Wet
				if (Wet && WetCfg)
				{
					ArcConditionHelpers::SetSaturationClamped(*Wet, *WetCfg,
						Wet->Saturation + ColdRemoved * 0.5f);
					ArcConditionHelpers::UpdateActiveFlag(*Wet, *WetCfg);
				}
			}
			else
			{
				ArcConditionHelpers::UpdateActiveFlag(*Chilled, *ChilledCfg);
			}
		}

		if (RemainingFire <= 0.f) { return; }

		// Fuel Conversion: Oiled consumed, oil x2 added to fire
		if (Oiled && Oiled->Saturation > 0.f)
		{
			RemainingFire += Oiled->Saturation * 2.f;
			Oiled->Saturation = 0.f;
			Oiled->bActive = false;
		}

		// Apply remaining fire as Burning
		if (Burning && BurningCfg)
		{
			RemainingFire = ArcConditionHelpers::ApplyResistance(RemainingFire, Burning->Resistance);
			ArcConditionHelpers::SetSaturationClamped(*Burning, *BurningCfg,
				Burning->Saturation + RemainingFire);
			ArcConditionHelpers::UpdateActiveFlag(*Burning, *BurningCfg);
		}
	}

	// -----------------------------------------------------------------------
	// Cold Application
	// -----------------------------------------------------------------------
	void ApplyCold(float Amount,
		FArcConditionState* Burning,  const FArcConditionConfig* BurningCfg,
		FArcConditionState* Chilled,  const FArcConditionConfig* ChilledCfg,
		FArcConditionState* Wet,      const FArcConditionConfig* WetCfg,
		FArcConditionState* Blinded,  const FArcConditionConfig* BlindedCfg)
	{
		float RemainingCold = Amount;

		// Thermal Cascade: Cold extinguishes Burning (x2 efficiency)
		if (Burning && Burning->Saturation > 0.f && BurningCfg)
		{
			const float FireRemovable = RemainingCold * 2.f;
			const float FireRemoved = FMath::Min(Burning->Saturation, FireRemovable);
			Burning->Saturation -= FireRemoved;
			RemainingCold -= FireRemoved / 2.f;

			// Steam
			if (Blinded && BlindedCfg)
			{
				ArcConditionHelpers::SetSaturationClamped(*Blinded, *BlindedCfg,
					Blinded->Saturation + FireRemoved * 0.3f);
				ArcConditionHelpers::UpdateActiveFlag(*Blinded, *BlindedCfg);
			}

			if (Burning->Saturation <= 0.f)
			{
				ArcConditionHelpers::ClearCondition(*Burning);

				// Condensation: excess cold -> Wet (not Chilled)
				if (RemainingCold > 0.f && Wet && WetCfg)
				{
					ArcConditionHelpers::SetSaturationClamped(*Wet, *WetCfg,
						Wet->Saturation + RemainingCold * 0.5f);
					ArcConditionHelpers::UpdateActiveFlag(*Wet, *WetCfg);
					return;
				}
			}
			else
			{
				ArcConditionHelpers::UpdateActiveFlag(*Burning, *BurningCfg);
			}
		}

		if (RemainingCold <= 0.f) { return; }

		// Flash Freeze: Wet target -> Cold x3, wet consumed
		if (Wet && Wet->Saturation > 0.f)
		{
			RemainingCold *= 3.f;
			Wet->Saturation = 0.f;
			Wet->bActive = false;
		}

		// Apply Chilled
		if (Chilled && ChilledCfg)
		{
			RemainingCold = ArcConditionHelpers::ApplyResistance(RemainingCold, Chilled->Resistance);
			ArcConditionHelpers::SetSaturationClamped(*Chilled, *ChilledCfg,
				Chilled->Saturation + RemainingCold);
			ArcConditionHelpers::UpdateActiveFlag(*Chilled, *ChilledCfg);
		}
	}

	// -----------------------------------------------------------------------
	// Oil Application
	// -----------------------------------------------------------------------
	void ApplyOil(float Amount,
		FArcConditionState* Burning, const FArcConditionConfig* BurningCfg,
		FArcConditionState* Oiled,   const FArcConditionConfig* OiledCfg)
	{
		// Stoking: Oil on burning target feeds fire (x1.5)
		if (Burning && (Burning->Saturation > 0.f || Burning->OverloadPhase != EArcConditionOverloadPhase::None) && BurningCfg)
		{
			const float FireBoost = Amount * 1.5f;
			ArcConditionHelpers::SetSaturationClamped(*Burning, *BurningCfg,
				Burning->Saturation + FireBoost);
			ArcConditionHelpers::UpdateActiveFlag(*Burning, *BurningCfg);
			return;
		}

		// Normal oil application
		if (Oiled && OiledCfg)
		{
			const float Effective = ArcConditionHelpers::ApplyResistance(Amount, Oiled->Resistance);
			ArcConditionHelpers::SetSaturationClamped(*Oiled, *OiledCfg,
				Oiled->Saturation + Effective);
			ArcConditionHelpers::UpdateActiveFlag(*Oiled, *OiledCfg);
		}
	}

	// -----------------------------------------------------------------------
	// Water Application
	// -----------------------------------------------------------------------
	void ApplyWet(float Amount,
		FArcConditionState* Burning,  const FArcConditionConfig* BurningCfg,
		FArcConditionState* Chilled,  const FArcConditionConfig* ChilledCfg,
		FArcConditionState* Wet,      const FArcConditionConfig* WetCfg)
	{
		float RemainingWet = Amount;

		// Ablative shield: Water reduces Burning
		if (Burning && Burning->Saturation > 0.f && BurningCfg)
		{
			const float FireRemoved = FMath::Min(Burning->Saturation, RemainingWet);
			Burning->Saturation -= FireRemoved;
			RemainingWet -= FireRemoved;

			if (Burning->Saturation <= 0.f)
			{
				ArcConditionHelpers::ClearCondition(*Burning);
			}
			else
			{
				ArcConditionHelpers::UpdateActiveFlag(*Burning, *BurningCfg);
			}
		}

		if (RemainingWet <= 0.f) { return; }

		// Thermal mass: Water on cold target accelerates Chilled
		if (Chilled && Chilled->Saturation > 0.f && ChilledCfg)
		{
			const float ColdBoost = RemainingWet * 0.5f;
			ArcConditionHelpers::SetSaturationClamped(*Chilled, *ChilledCfg,
				Chilled->Saturation + ColdBoost);
			ArcConditionHelpers::UpdateActiveFlag(*Chilled, *ChilledCfg);
			return; // Wet consumed into cold
		}

		// Normal Wet application
		if (Wet && WetCfg)
		{
			const float Effective = ArcConditionHelpers::ApplyResistance(RemainingWet, Wet->Resistance);
			ArcConditionHelpers::SetSaturationClamped(*Wet, *WetCfg, Wet->Saturation + Effective);
			ArcConditionHelpers::UpdateActiveFlag(*Wet, *WetCfg);
		}
	}
}

// ---------------------------------------------------------------------------
// Processor Implementation
// ---------------------------------------------------------------------------

UArcThermalConditionProcessor::UArcThermalConditionProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcThermalConditionProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcConditionEffects::Signals::BurningApply);
	SubscribeToSignal(*SignalSubsystem, UE::ArcConditionEffects::Signals::ChilledApply);
	SubscribeToSignal(*SignalSubsystem, UE::ArcConditionEffects::Signals::OiledApply);
	SubscribeToSignal(*SignalSubsystem, UE::ArcConditionEffects::Signals::WetApply);
}

void UArcThermalConditionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	// Thermal conditions — required ReadWrite for the primary ones
	EntityQuery.AddRequirement<FArcBurningConditionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcChilledConditionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcWetConditionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcOiledConditionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);

	// Cross-domain conditions that thermal can affect
	EntityQuery.AddRequirement<FArcBleedingConditionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcDiseasedConditionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcBlindedConditionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);

	// Configs — optional since entity may not have all conditions
	EntityQuery.AddConstSharedRequirement<FArcBurningConditionConfig>(EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcChilledConditionConfig>(EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcWetConditionConfig>(EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcOiledConditionConfig>(EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcBlindedConditionConfig>(EMassFragmentPresence::Optional);
}

void UArcThermalConditionProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UArcConditionEffectsSubsystem* Subsystem = Context.GetWorld()->GetSubsystem<UArcConditionEffectsSubsystem>();
	if (!Subsystem) { return; }

	// Grab only thermal requests
	TArray<FArcConditionApplicationRequest> ThermalRequests;
	TArray<FArcConditionApplicationRequest>& AllRequests = Subsystem->GetPendingRequests();
	for (int32 i = AllRequests.Num() - 1; i >= 0; --i)
	{
		const EArcConditionType Type = AllRequests[i].ConditionType;
		if (Type == EArcConditionType::Burning || Type == EArcConditionType::Chilled
			|| Type == EArcConditionType::Oiled || Type == EArcConditionType::Wet)
		{
			ThermalRequests.Add(AllRequests[i]);
			AllRequests.RemoveAtSwap(i);
		}
	}

	if (ThermalRequests.Num() == 0) { return; }

	// Build per-entity map
	TMap<FMassEntityHandle, TArray<FArcConditionApplicationRequest*>> EntityRequestMap;
	for (FArcConditionApplicationRequest& Req : ThermalRequests)
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
			// Get optional fragment views
			TArrayView<FArcBurningConditionFragment> BurningFrags  = Ctx.GetMutableFragmentView<FArcBurningConditionFragment>();
			TArrayView<FArcChilledConditionFragment> ChilledFrags  = Ctx.GetMutableFragmentView<FArcChilledConditionFragment>();
			TArrayView<FArcWetConditionFragment>     WetFrags      = Ctx.GetMutableFragmentView<FArcWetConditionFragment>();
			TArrayView<FArcOiledConditionFragment>   OiledFrags    = Ctx.GetMutableFragmentView<FArcOiledConditionFragment>();
			TArrayView<FArcBleedingConditionFragment> BleedingFrags = Ctx.GetMutableFragmentView<FArcBleedingConditionFragment>();
			TArrayView<FArcDiseasedConditionFragment> DiseasedFrags = Ctx.GetMutableFragmentView<FArcDiseasedConditionFragment>();
			TArrayView<FArcBlindedConditionFragment>  BlindedFrags  = Ctx.GetMutableFragmentView<FArcBlindedConditionFragment>();

			const FArcConditionConfig* BurningCfg = GetOptionalConfig<FArcBurningConditionConfig>(Ctx);
			const FArcConditionConfig* ChilledCfg = GetOptionalConfig<FArcChilledConditionConfig>(Ctx);
			const FArcConditionConfig* WetCfg     = GetOptionalConfig<FArcWetConditionConfig>(Ctx);
			const FArcConditionConfig* OiledCfg   = GetOptionalConfig<FArcOiledConditionConfig>(Ctx);
			const FArcConditionConfig* BlindedCfg = GetOptionalConfig<FArcBlindedConditionConfig>(Ctx);

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				TArray<FArcConditionApplicationRequest*>* Requests = EntityRequestMap.Find(Entity);
				if (!Requests) { continue; }

				const int32 Idx = *EntityIt;

				FArcConditionState* Burning  = !BurningFrags.IsEmpty()  ? &BurningFrags[Idx].State  : nullptr;
				FArcConditionState* Chilled  = !ChilledFrags.IsEmpty()  ? &ChilledFrags[Idx].State  : nullptr;
				FArcConditionState* Wet      = !WetFrags.IsEmpty()      ? &WetFrags[Idx].State      : nullptr;
				FArcConditionState* Oiled    = !OiledFrags.IsEmpty()    ? &OiledFrags[Idx].State    : nullptr;
				FArcConditionState* Bleeding = !BleedingFrags.IsEmpty() ? &BleedingFrags[Idx].State : nullptr;
				FArcConditionState* Diseased = !DiseasedFrags.IsEmpty() ? &DiseasedFrags[Idx].State : nullptr;
				FArcConditionState* Blinded  = !BlindedFrags.IsEmpty()  ? &BlindedFrags[Idx].State  : nullptr;

				// Snapshot for change detection
				auto Snapshot = [](const FArcConditionState* S) -> TPair<bool, EArcConditionOverloadPhase>
				{
					return S ? TPair<bool, EArcConditionOverloadPhase>{S->bActive, S->OverloadPhase}
					         : TPair<bool, EArcConditionOverloadPhase>{false, EArcConditionOverloadPhase::None};
				};

				FArcConditionState* AllStates[] = { Burning, Chilled, Wet, Oiled, Bleeding, Diseased, Blinded };
				TPair<bool, EArcConditionOverloadPhase> Before[7];
				for (int32 i = 0; i < 7; ++i) { Before[i] = Snapshot(AllStates[i]); }

				for (const FArcConditionApplicationRequest* Req : *Requests)
				{
					switch (Req->ConditionType)
					{
					case EArcConditionType::Burning:
						ApplyFire(Req->Amount, Burning, BurningCfg, Chilled, ChilledCfg,
							Wet, WetCfg, Oiled, OiledCfg, Bleeding, Diseased, Blinded, BlindedCfg);
						break;
					case EArcConditionType::Chilled:
						ApplyCold(Req->Amount, Burning, BurningCfg, Chilled, ChilledCfg,
							Wet, WetCfg, Blinded, BlindedCfg);
						break;
					case EArcConditionType::Oiled:
						ApplyOil(Req->Amount, Burning, BurningCfg, Oiled, OiledCfg);
						break;
					case EArcConditionType::Wet:
						ApplyWet(Req->Amount, Burning, BurningCfg, Chilled, ChilledCfg, Wet, WetCfg);
						break;
					default: break;
					}
				}

				// Change detection
				bool bStateChanged = false, bOverloadChanged = false;
				for (int32 i = 0; i < 7; ++i)
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

	// Signal downstream
	UWorld* World = EntityManager.GetWorld();
	UMassSignalSubsystem* SignalSubsystem = World ? World->GetSubsystem<UMassSignalSubsystem>() : nullptr;
	if (SignalSubsystem)
	{
		if (StateChangedEntities.Num() > 0)
		{
			SignalSubsystem->SignalEntities(UE::ArcConditionEffects::Signals::ConditionStateChanged, StateChangedEntities);
		}
		if (OverloadChangedEntities.Num() > 0)
		{
			SignalSubsystem->SignalEntities(UE::ArcConditionEffects::Signals::ConditionOverloadChanged, OverloadChangedEntities);
		}
	}
}
