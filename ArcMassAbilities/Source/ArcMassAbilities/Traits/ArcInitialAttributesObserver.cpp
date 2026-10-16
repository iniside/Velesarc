// Copyright Lukasz Baran. All Rights Reserved.

#include "Traits/ArcInitialAttributesObserver.h"
#include "ArcMassAbilitiesLogs.h"

#include "Attributes/ArcAttribute.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Attributes/ArcInitialAttributesPreset.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcAttributeHandlerSharedFragment.h"
#include "Fragments/ArcInitialAttributesPresetSharedFragment.h"
#include "Modifiers/ArcModifierFunctions.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/UnrealType.h"

namespace ArcInitialAttributesObserverInternal
{
	/** Cache of FArcAttribute FStructProperty* per UScriptStruct, populated lazily. */
	static TMap<const UScriptStruct*, TArray<FStructProperty*>> GAttributePropertyCache;

	/**
	 * Returns all FStructProperty members of FragmentStruct whose inner struct is FArcAttribute.
	 * Results are cached in a function-local static map keyed on FragmentStruct.
	 */
	static const TArray<FStructProperty*>& GatherAttributeProperties(const UScriptStruct* FragmentStruct)
	{
		TArray<FStructProperty*>* Cached = GAttributePropertyCache.Find(FragmentStruct);
		if (Cached)
		{
			return *Cached;
		}

		TArray<FStructProperty*>& Result = GAttributePropertyCache.Add(FragmentStruct);
		for (TFieldIterator<FStructProperty> PropIt(FragmentStruct); PropIt; ++PropIt)
		{
			FStructProperty* Prop = *PropIt;
			if (Prop && Prop->Struct == FArcAttribute::StaticStruct())
			{
				Result.Add(Prop);
			}
		}
		return Result;
	}

	/**
	 * Collects attribute fragment types from the preset's AttributeFragments array. Attribute
	 * fragments are sparse (since Phase 1 of the sparse-fragment conversion), and sparse elements
	 * are not part of archetype composition (ForEachArchetypeElementType's docstring is explicit:
	 * "sparse elements are not archetype characteristics"), so the archetype can't be the source
	 * of truth here. The preset is — it declares which attribute fragments the entity should be
	 * seeded with. SeedAndSync below null-checks each entity's sparse storage so types declared
	 * in the preset but absent on the entity are silently skipped.
	 */
	static TArray<const UScriptStruct*> GatherAttributeFragmentTypesFromPreset(const UArcInitialAttributesPreset& Preset)
	{
		TArray<const UScriptStruct*> Result;
		for (const FInstancedStruct& Entry : Preset.AttributeFragments)
		{
			if (!Entry.IsValid())
			{
				continue;
			}
			const UScriptStruct* StructType = Entry.GetScriptStruct();
			if (StructType && StructType->IsChildOf(FArcMassAttributesBase::StaticStruct()))
			{
				Result.AddUnique(StructType);
			}
		}
		return Result;
	}

	/**
	 * Build a struct-type-keyed lookup from the preset's FInstancedStruct array. Each entry's
	 * memory is the authored fragment data, used as the BaseValue source for the matching
	 * attribute fragment type on the entity.
	 */
	static TMap<const UScriptStruct*, const uint8*> BuildPresetMemoryByType(const UArcInitialAttributesPreset& Preset)
	{
		TMap<const UScriptStruct*, const uint8*> Result;
		for (const FInstancedStruct& Entry : Preset.AttributeFragments)
		{
			if (!Entry.IsValid())
			{
				continue;
			}
			const UScriptStruct* StructType = Entry.GetScriptStruct();
			if (!StructType)
			{
				continue;
			}
			Result.Add(StructType, Entry.GetMemory());
		}
		return Result;
	}

	/**
	 * Pass 1 — Seed each attribute fragment on the entity. For fragments with a matching preset
	 * entry, copy BaseValue from preset; for any fragment (with or without preset entry), sync
	 * CurrentValue = BaseValue so Pass 2 PostChange handlers see correct sibling CurrentValues
	 * regardless of property-iteration order. The CurrentValue sync also preserves behavior for
	 * fragment types the preset doesn't author.
	 */
	static void SeedAndSync(
		FMassEntityManager& EntityManager,
		FMassEntityHandle EntityHandle,
		const TArray<const UScriptStruct*>& AttributeFragmentTypes,
		const TMap<const UScriptStruct*, const uint8*>& PresetByType)
	{
		for (const UScriptStruct* FragmentType : AttributeFragmentTypes)
		{
			const FStructView FragView = EntityManager.GetMutableSparseElementDataForEntity(FragmentType, EntityHandle);
			uint8* EntityMem = FragView.GetMemory();
			if (!EntityMem)
			{
				continue;
			}

			const uint8* const* PresetMemPtr = PresetByType.Find(FragmentType);
			const uint8* PresetMem = PresetMemPtr ? *PresetMemPtr : nullptr;

			const TArray<FStructProperty*>& Props = GatherAttributeProperties(FragmentType);
			for (FStructProperty* Prop : Props)
			{
				FArcAttribute* EntityAttr = Prop->ContainerPtrToValuePtr<FArcAttribute>(EntityMem);
				if (!EntityAttr)
				{
					continue;
				}
				if (PresetMem)
				{
					const FArcAttribute* PresetAttr = Prop->ContainerPtrToValuePtr<FArcAttribute>(PresetMem);
					if (PresetAttr)
					{
						EntityAttr->BaseValue = PresetAttr->BaseValue;
					}
				}
				EntityAttr->CurrentValue = EntityAttr->BaseValue;
			}
		}
	}

	/**
	 * Pass 2 — For every FArcAttribute property on every attribute fragment, call
	 * HandlerContext.InitInline with the entity's current BaseValue so Initialize fires
	 * unconditionally and the cascade seeds derived attributes. Reads from entity memory (not
	 * preset) so Initialize handlers that mutate sibling/derived attributes in an earlier
	 * iteration are observed by later iterations within the same pass.
	 */
	static void FireInitializeCascade(
		FMassEntityManager& EntityManager,
		FMassEntityHandle EntityHandle,
		const TArray<const UScriptStruct*>& AttributeFragmentTypes,
		FArcAttributeHandlerContext& HandlerContext)
	{
		for (const UScriptStruct* FragmentType : AttributeFragmentTypes)
		{
			const FStructView FragView = EntityManager.GetMutableSparseElementDataForEntity(FragmentType, EntityHandle);
			uint8* EntityMem = FragView.GetMemory();
			if (!EntityMem)
			{
				continue;
			}

			// Rebind the context's owning memory to this fragment so ResolveAttribute works correctly.
			HandlerContext.OwningFragmentMemory = EntityMem;

			const TArray<FStructProperty*>& Props = GatherAttributeProperties(FragmentType);
			for (FStructProperty* Prop : Props)
			{
				const FArcAttribute* Attr = Prop->ContainerPtrToValuePtr<FArcAttribute>(EntityMem);
				if (!Attr)
				{
					continue;
				}

				FArcAttributeRef Ref;
				Ref.FragmentType = const_cast<UScriptStruct*>(FragmentType);
				Ref.PropertyName = Prop->GetFName();

				const bool bOk = HandlerContext.InitInline(Ref, Attr->BaseValue);
				if (!bOk)
				{
					UE_LOGF(LogArcMassAbilities, Warning,"UArcInitialAttributesObserver: InitInline failed for fragment '%ls' property '%ls' — attribute not resolvable.",
						*FragmentType->GetName(), *Prop->GetName());
				}
			}
		}
	}

	static void NotifyObserversAfterCascade(
		FMassEntityManager& EntityManager,
		FMassExecutionContext& MassContext,
		FMassEntityHandle EntityHandle,
		const UArcAttributeHandlerConfig& HandlerConfig)
	{
		for (const TPair<FArcAttributeRef, FInstancedStruct>& HandlerEntry : HandlerConfig.Handlers)
		{
			const FArcAttributeRef& ObserverRef = HandlerEntry.Key;
			const FArcAttributeHandler* Handler = HandlerEntry.Value.GetPtr<FArcAttributeHandler>();
			if (!ObserverRef.IsValid() || !Handler)
			{
				continue;
			}

			TArray<FArcAttributeRef> Inputs;
			Handler->GatherInputs(Inputs);

			const FArcAttributeRef* ChangedRef = nullptr;
			const FArcAttribute* Changed = nullptr;
			for (const FArcAttributeRef& Observed : Inputs)
			{
				Changed = Observed.IsValid() ? ArcAttributes::ResolveAttribute(EntityManager, EntityHandle, Observed) : nullptr;
				if (Changed)
				{
					ChangedRef = &Observed;
					break;
				}
			}
			if (!ChangedRef)
			{
				continue;
			}

			const FStructView ObserverFragView = EntityManager.GetMutableSparseElementDataForEntity(ObserverRef.FragmentType, EntityHandle);
			uint8* ObserverFragMem = ObserverFragView.GetMemory();
			if (!ObserverFragMem)
			{
				continue;
			}

			FArcAttributeHandlerContext ObserverContext(EntityManager, &MassContext, EntityHandle, ObserverFragMem, &HandlerConfig, ObserverRef);
			FArcAttribute* Self = ObserverContext.ResolveAttribute(ObserverRef);
			if (!Self)
			{
				continue;
			}

			const float ChangedValue = Changed->GetCurrentValue();

			Handler->OnObservedAttributeChanged(ObserverContext, *Self, *ChangedRef, ChangedValue, ChangedValue);
		}
	}

} // namespace ArcInitialAttributesObserverInternal

// ---------------------------------------------------------------------------

UArcInitialAttributesObserver::UArcInitialAttributesObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcAggregatorFragment::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcInitialAttributesObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcAggregatorFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddConstSharedRequirement<FArcAttributeHandlerSharedFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddConstSharedRequirement<FArcInitialAttributesPresetSharedFragment>(EMassFragmentPresence::All);
}

void UArcInitialAttributesObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcInitialAttributesApply);

	ObserverQuery.ForEachEntityChunk(Context, [&EntityManager](FMassExecutionContext& Ctx)
	{
		const FArcAttributeHandlerSharedFragment& HandlerFrag = Ctx.GetConstSharedFragment<FArcAttributeHandlerSharedFragment>();
		const UArcAttributeHandlerConfig* HandlerConfig = HandlerFrag.Config.Get();
		if (!HandlerConfig)
		{
			UE_LOGF(LogArcMassAbilities, Warning,"UArcInitialAttributesObserver: FArcAttributeHandlerSharedFragment present but Config is null — skipping initialization for this chunk.");
			return;
		}

		const FArcInitialAttributesPresetSharedFragment& PresetFrag =
			Ctx.GetConstSharedFragment<FArcInitialAttributesPresetSharedFragment>();
		const UArcInitialAttributesPreset* Preset = PresetFrag.Preset.Get();
		if (!Preset)
		{
			UE_LOGF(LogArcMassAbilities, Warning,"UArcInitialAttributesObserver: FArcInitialAttributesPresetSharedFragment present but Preset is null — skipping initialization for this chunk.");
			return;
		}

		const TConstArrayView<FMassEntityHandle> Entities = Ctx.GetEntities();
		if (Entities.Num() == 0)
		{
			return;
		}

		// Attribute fragments are sparse — the archetype doesn't carry them, so the preset's
		// AttributeFragments list is the canonical source of which fragment types to seed.
		TArray<const UScriptStruct*> AttributeFragmentTypes =
			ArcInitialAttributesObserverInternal::GatherAttributeFragmentTypesFromPreset(*Preset);

		if (AttributeFragmentTypes.Num() == 0)
		{
			UE_LOGF(LogArcMassAbilities, Warning,"UArcInitialAttributesObserver: Preset has no FArcMassAttributesBase-derived fragments — nothing to initialize.");
			return;
		}

		const TMap<const UScriptStruct*, const uint8*> PresetByType =
			ArcInitialAttributesObserverInternal::BuildPresetMemoryByType(*Preset);

		if (PresetByType.Num() == 0)
		{
			UE_LOGF(LogArcMassAbilities, Warning,"UArcInitialAttributesObserver: Preset '%ls' has no valid AttributeFragments entries — nothing to seed.",
				*Preset->GetName());
			return;
		}

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

			// Pass 1: copy BaseValue from preset to entity (per matching fragment type) and sync
			// CurrentValue = BaseValue across all attribute fragments so Initialize handlers see
			// correct sibling CurrentValues regardless of processing order.
			ArcInitialAttributesObserverInternal::SeedAndSync(EntityManager, Entity, AttributeFragmentTypes, PresetByType);

			ARCMA_VLOG(Ctx.GetWorld(), Log,
				TEXT("[Attr] Seeded preset=%s on entity=%s fragments=%d"),
				*Preset->GetName(),
				*Entity.DebugGetDescription(),
				AttributeFragmentTypes.Num());

			// Pass 2: fire Initialize cascade via InitInline for every attribute on every attribute
			// fragment. Reads BaseValue from entity memory so cascades from earlier iterations
			// propagate to later ones. HandlerContext rebinds OwningFragmentMemory per fragment.
			FArcAttributeHandlerContext HandlerContext(EntityManager, &Ctx, Entity, nullptr, HandlerConfig, FArcAttributeRef());
			ArcInitialAttributesObserverInternal::FireInitializeCascade(
				EntityManager, Entity, AttributeFragmentTypes, HandlerContext);

			// Initialize reads observed inputs once, in preset order; this notify makes observers recompute from final inputs.
			ArcInitialAttributesObserverInternal::NotifyObserversAfterCascade(EntityManager, Ctx, Entity, *HandlerConfig);

			ARCMA_VLOG(Ctx.GetWorld(), Log,
				TEXT("[Attr] Cascade complete entity=%s"),
				*Entity.DebugGetDescription());
		}
	});
}
