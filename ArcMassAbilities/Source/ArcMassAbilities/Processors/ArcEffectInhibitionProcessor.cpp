// Copyright Lukasz Baran. All Rights Reserved.

#include "Processors/ArcEffectInhibitionProcessor.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectComp_Inhibition.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectSpec.h"
#include "Signals/ArcAttributeRecalculateSignal.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

UArcEffectInhibitionProcessor::UArcEffectInhibitionProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = FName(TEXT("Tasks"));
}

void UArcEffectInhibitionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcEffectStackFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcOwnedTagsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcAggregatorFragment>(EMassFragmentAccess::ReadWrite);
}

void UArcEffectInhibitionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Context.GetWorld());

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, SignalSubsystem](FMassExecutionContext& ChunkContext)
	{
		const int32 NumEntities = ChunkContext.GetNumEntities();
		TArrayView<FArcEffectStackFragment> Stacks = ChunkContext.GetMutableFragmentView<FArcEffectStackFragment>();
		TConstArrayView<FArcOwnedTagsFragment> Tags = ChunkContext.GetFragmentView<FArcOwnedTagsFragment>();
		TArrayView<FArcAggregatorFragment> Aggregators = ChunkContext.GetMutableFragmentView<FArcAggregatorFragment>();

		for (int32 Idx = 0; Idx < NumEntities; ++Idx)
		{
			FArcEffectStackFragment& Stack = Stacks[Idx];
			const FArcOwnedTagsFragment& OwnedTags = Tags[Idx];
			FArcAggregatorFragment& AggFrag = Aggregators[Idx];
			const FMassEntityHandle Entity = ChunkContext.GetEntity(Idx);
			bool bNeedRecalc = false;

			for (FArcActiveEffect& Effect : Stack.ActiveEffects)
			{
				const FArcEffectSpec* Spec = Effect.Spec.GetPtr<FArcEffectSpec>();
				if (!Spec || !Spec->Definition)
				{
					continue;
				}

				const FArcEffectComp_Inhibition* InhibComp = nullptr;
				for (const FInstancedStruct& CompStruct : Spec->Definition->Components)
				{
					if (CompStruct.IsValid())
					{
						InhibComp = CompStruct.GetPtr<FArcEffectComp_Inhibition>();
						if (InhibComp)
						{
							break;
						}
					}
				}

				if (!InhibComp)
				{
					continue;
				}

				bool bShouldInhibit = OwnedTags.Tags.HasAny(InhibComp->InhibitedByTags);

				if (bShouldInhibit && !Effect.bInhibited)
				{
					Effect.bInhibited = true;
					ArcEffects::Private::RemoveModsFromAggregator(AggFrag, Effect.ModifierHandles);
					bNeedRecalc = true;
				}
				else if (!bShouldInhibit && Effect.bInhibited)
				{
					Effect.bInhibited = false;
					Effect.ModifierHandles.Empty();

					for (int32 ModIdx = 0; ModIdx < Spec->Definition->Modifiers.Num(); ++ModIdx)
					{
						const FArcEffectModifier& Modifier = Spec->Definition->Modifiers[ModIdx];
						if (!Modifier.Attribute.IsValid())
						{
							continue;
						}

						float BaseMag = (Spec->ResolvedMagnitudes.IsValidIndex(ModIdx))
							? Spec->ResolvedMagnitudes[ModIdx]
							: Modifier.Magnitude.GetValue();

						float EffectiveMag = BaseMag;
						switch (Modifier.Operation)
						{
						case EArcModifierOp::Add:
						case EArcModifierOp::AddFinal:
						case EArcModifierOp::MultiplyAdditive:
						case EArcModifierOp::DivideAdditive:
							EffectiveMag = BaseMag * Spec->Context.StackCount;
							break;
						case EArcModifierOp::MultiplyCompound:
							EffectiveMag = FMath::Pow(BaseMag, static_cast<float>(Spec->Context.StackCount));
							break;
						case EArcModifierOp::Override:
							break;
						default:
							break;
						}

						FArcAggregator& Agg = AggFrag.FindOrAddAggregator(Modifier.Attribute);
						FArcModifierHandle Handle = Agg.AddMod(
							Modifier.Operation, EffectiveMag,
							Spec->Context.SourceEntity, nullptr, &Modifier.TagFilter, Modifier.Channel);
						Effect.ModifierHandles.Add(Handle);
					}
					bNeedRecalc = true;
				}
			}

			if (bNeedRecalc && SignalSubsystem)
			{
				SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AttributeRecalculate, Entity);
			}
		}
	});
}
