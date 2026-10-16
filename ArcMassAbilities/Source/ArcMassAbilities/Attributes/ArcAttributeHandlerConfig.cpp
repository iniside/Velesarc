// Copyright Lukasz Baran. All Rights Reserved.

#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Attributes/ArcAttributeNotification.h"
#include "ArcMassAbilitiesLogs.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"

// ---------------------------------------------------------------------------
// Cycle-detection helpers — file-scoped
// ---------------------------------------------------------------------------
namespace ArcObserverCycleDetail
{
	// DFS colour values
	static constexpr uint8 White = 0; // unvisited
	static constexpr uint8 Grey  = 1; // on the current DFS stack
	static constexpr uint8 Black = 2; // fully processed

	static TMap<FArcAttributeRef, TArray<FArcAttributeRef>> GatherHandlerInputs(const TMap<FArcAttributeRef, FInstancedStruct>& Handlers)
	{
		TMap<FArcAttributeRef, TArray<FArcAttributeRef>> InputsByObserver;
		for (const TPair<FArcAttributeRef, FInstancedStruct>& Pair : Handlers)
		{
			const FArcAttributeHandler* Handler = Pair.Value.GetPtr<FArcAttributeHandler>();
			if (!Pair.Key.IsValid() || !Handler)
			{
				continue;
			}

			TArray<FArcAttributeRef> Inputs;
			Handler->GatherInputs(Inputs);
			if (!Inputs.IsEmpty())
			{
				InputsByObserver.Add(Pair.Key, MoveTemp(Inputs));
			}
		}
		return InputsByObserver;
	}

	// Returns true if a cycle was found starting from StartAttr.
	// Colour map is updated in place. CyclePath receives the cycle nodes when a cycle is found.
	static bool DfsDetectCycle(
		const FArcAttributeRef& StartAttr,
		const TMap<FArcAttributeRef, TArray<FArcAttributeRef>>& InputsByObserver,
		TMap<FArcAttributeRef, uint8>& Colours,
		TArray<FArcAttributeRef>& CyclePath)
	{
		// Iterative DFS with an explicit stack of (node, iterator-index) pairs
		// to avoid recursion on potentially deep graphs.
		struct FStackFrame
		{
			FArcAttributeRef Node;
			int32            NeighbourIndex;
		};

		TArray<FStackFrame> Stack;
		Stack.Add({ StartAttr, 0 });
		Colours.Add(StartAttr, Grey);

		while (!Stack.IsEmpty())
		{
			FStackFrame& Frame = Stack.Last();

			const TArray<FArcAttributeRef>* Inputs = InputsByObserver.Find(Frame.Node);
			bool bFoundUnvisited = false;

			if (Inputs)
			{
				while (Frame.NeighbourIndex < Inputs->Num())
				{
					const FArcAttributeRef& Neighbour = (*Inputs)[Frame.NeighbourIndex];
					++Frame.NeighbourIndex;

					if (!Neighbour.IsValid())
					{
						continue;
					}

					const uint8* NeighbourColour = Colours.Find(Neighbour);
					if (NeighbourColour && *NeighbourColour == Grey)
					{
						// Found a cycle — collect the path from the stack
						for (const FStackFrame& SF : Stack)
						{
							CyclePath.Add(SF.Node);
						}
						CyclePath.Add(Neighbour);
						return true;
					}

					if (!NeighbourColour || *NeighbourColour == White)
					{
						Colours.Add(Neighbour, Grey);
						Stack.Add({ Neighbour, 0 });
						bFoundUnvisited = true;
						break;
					}
					// Black — already fully processed, skip
				}
			}

			if (!bFoundUnvisited)
			{
				// All neighbours processed — colour this node Black and pop
				Colours.Add(Frame.Node, Black);
				Stack.Pop(EAllowShrinking::No);
			}
		}

		return false;
	}

} // namespace ArcObserverCycleDetail

FArcAttributeHandlerContext::FArcAttributeHandlerContext(
	FMassEntityManager& InEntityManager,
	FMassExecutionContext* InMassContext,
	FMassEntityHandle InEntity,
	uint8* InOwningFragmentMemory,
	const UArcAttributeHandlerConfig* InHandlerConfig,
	const FArcAttributeRef& InAttribute,
	FMassEntityHandle InSourceEntity,
	const FGameplayTagContainer& InCapturedSourceTags,
	const FGameplayTagContainer& InCapturedTargetTags)
	: EntityManager(InEntityManager)
	, MassContext(InMassContext)
	, World(InEntityManager.GetWorld())
	, Entity(InEntity)
	, Attribute(InAttribute)
	, SourceEntity(InSourceEntity)
	, CapturedSourceTags(InCapturedSourceTags)
	, CapturedTargetTags(InCapturedTargetTags)
	, OwningFragmentMemory(InOwningFragmentMemory)
	, HandlerConfig(InHandlerConfig)
{}

FMassEntityHandle FArcAttributeHandlerContext::GetTargetEntity() const
{
	return Entity;
}

FArcAttribute* FArcAttributeHandlerContext::ResolveAttribute(const FArcAttributeRef& AttrRef) const
{
	check(OwningFragmentMemory);
	check(AttrRef.FragmentType);

	FProperty* Prop = AttrRef.GetCachedProperty();
	if (!Prop)
	{
		return nullptr;
	}

	return Prop->ContainerPtrToValuePtr<FArcAttribute>(OwningFragmentMemory);
}

bool FArcAttributeHandlerContext::ModifyInline(const FArcAttributeRef& AttrRef, float NewValue)
{
	FArcAttribute* Attr = ResolveAttribute(AttrRef);
	if (!Attr)
	{
		return false;
	}

	float OldValue = Attr->CurrentValue;
	if (OldValue == NewValue)
	{
		return true;
	}

	const FArcAttributeHandler* Handler = HandlerConfig ? HandlerConfig->FindHandler(AttrRef) : nullptr;
	if (Handler)
	{
		TGuardValue<FArcAttributeRef> AttributeGuard(Attribute, AttrRef);
		if (!Handler->PreChange(*this, *Attr, NewValue))
		{
			return false;
		}
	}

	Attr->CurrentValue = NewValue;

	if (Handler)
	{
		TGuardValue<FArcAttributeRef> AttributeGuard(Attribute, AttrRef);
		Handler->PostChange(*this, *Attr, OldValue, NewValue);
	}

	ArcAttributes::RecalculateLiveDependents(*this, AttrRef);

	return true;
}

bool FArcAttributeHandlerContext::InitInline(const FArcAttributeRef& AttrRef, float NewValue)
{
	FArcAttribute* Attr = ResolveAttribute(AttrRef);
	if (!Attr)
	{
		return false;
	}

	Attr->InitValue(NewValue);

	const FArcAttributeHandler* Handler = HandlerConfig ? HandlerConfig->FindHandler(AttrRef) : nullptr;
	if (Handler)
	{
		TGuardValue<FArcAttributeRef> AttributeGuard(Attribute, AttrRef);
		Handler->Initialize(*this, *Attr, NewValue);
	}

	return true;
}

float FArcAttributeHandlerContext::EvaluateWithTags(
	const FArcAttributeRef& AttrRef, float BaseValue,
	const FGameplayTagContainer& SourceTags,
	const FGameplayTagContainer& TargetTags) const
{
	FMassEntityView EntityView(EntityManager, Entity);
	const FArcAggregatorFragment* AggFrag = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();
	if (!AggFrag)
	{
		return BaseValue;
	}

	const FArcAggregator* Agg = AggFrag->Aggregators.Find(AttrRef);
	if (!Agg)
	{
		return BaseValue;
	}

	const FArcOwnedTagsFragment* OwnedTags = EntityView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
	const FArcAggregationContext AggContext{Entity,
		OwnedTags ? OwnedTags->GetTags().GetTagContainer() : FGameplayTagContainer::EmptyContainer,
		EntityManager, BaseValue};
	return Agg->EvaluateWithTags(AggContext, SourceTags, TargetTags);
}

const FArcAttributeHandler* UArcAttributeHandlerConfig::FindHandler(const FArcAttributeRef& AttributeRef) const
{
	const FInstancedStruct* Found = Handlers.Find(AttributeRef);
	if (Found && Found->IsValid())
	{
		return Found->GetPtr<FArcAttributeHandler>();
	}
	return nullptr;
}

const FInstancedStruct* UArcAttributeHandlerConfig::FindAggregationPolicy(const FArcAttributeRef& AttributeRef) const
{
	const FInstancedStruct* Found = AggregationPolicies.Find(AttributeRef);
	if (Found && Found->IsValid())
	{
		return Found;
	}
	return nullptr;
}

const TArray<FArcAttributeRef>* UArcAttributeHandlerConfig::FindObserversFor(const FArcAttributeRef& Changed) const
{
	return ObserversByObserved.Find(Changed);
}

void UArcAttributeHandlerConfig::BuildReverseIndex()
{
	ObserversByObserved.Reset();

	const TMap<FArcAttributeRef, TArray<FArcAttributeRef>> InputsByObserver = ArcObserverCycleDetail::GatherHandlerInputs(Handlers);
	for (const TPair<FArcAttributeRef, TArray<FArcAttributeRef>>& Pair : InputsByObserver)
	{
		for (const FArcAttributeRef& ObservedAttr : Pair.Value)
		{
			if (!ObservedAttr.IsValid())
			{
				continue;
			}
			ObserversByObserved.FindOrAdd(ObservedAttr).AddUnique(Pair.Key);
		}
	}
}

bool UArcAttributeHandlerConfig::ValidateNoCycles(FString& OutError) const
{
	const TMap<FArcAttributeRef, TArray<FArcAttributeRef>> InputsByObserver = ArcObserverCycleDetail::GatherHandlerInputs(Handlers);

	// Check self-edges first (A observes A)
	for (const TPair<FArcAttributeRef, TArray<FArcAttributeRef>>& Pair : InputsByObserver)
	{
		const FArcAttributeRef& ObserverAttr = Pair.Key;
		for (const FArcAttributeRef& ObservedAttr : Pair.Value)
		{
			if (ObserverAttr == ObservedAttr)
			{
				OutError = FString::Printf(
					TEXT("Attribute observer self-cycle: %s.%s observes itself."),
					ObserverAttr.FragmentType ? *ObserverAttr.FragmentType->GetName() : TEXT("?"),
					*ObserverAttr.PropertyName.ToString());
				return false;
			}
		}
	}

	// Iterative DFS cycle detection across all unvisited nodes
	TMap<FArcAttributeRef, uint8> Colours;
	Colours.Reserve(InputsByObserver.Num());

	for (const TPair<FArcAttributeRef, TArray<FArcAttributeRef>>& Pair : InputsByObserver)
	{
		const FArcAttributeRef& StartAttr = Pair.Key;

		const uint8* Colour = Colours.Find(StartAttr);
		if (Colour && *Colour != ArcObserverCycleDetail::White)
		{
			continue;
		}

		TArray<FArcAttributeRef> CyclePath;
		if (ArcObserverCycleDetail::DfsDetectCycle(StartAttr, InputsByObserver, Colours, CyclePath))
		{
			TArray<FString> PathStrings;
			for (const FArcAttributeRef& Ref : CyclePath)
			{
				PathStrings.Add(FString::Printf(
					TEXT("%s.%s"),
					Ref.FragmentType ? *Ref.FragmentType->GetName() : TEXT("?"),
					*Ref.PropertyName.ToString()));
			}
			OutError = FString::Printf(
				TEXT("Attribute observer cycle detected: %s"),
				*FString::Join(PathStrings, TEXT(" -> ")));
			return false;
		}
	}

	return true;
}

void UArcAttributeHandlerConfig::PostLoad()
{
	Super::PostLoad();

	BuildReverseIndex();

	FString CycleError;
	if (!ValidateNoCycles(CycleError))
	{
		UE_LOGF(LogArcMassAbilities, Warning,"UArcAttributeHandlerConfig '%ls': %ls",
			*GetName(), *CycleError);
	}
}

#if WITH_EDITOR
void UArcAttributeHandlerConfig::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	BuildReverseIndex();

	FString CycleError;
	if (!ValidateNoCycles(CycleError))
	{
		UE_LOGF(LogArcMassAbilities, Warning,"UArcAttributeHandlerConfig '%ls': %ls",
			*GetName(), *CycleError);
	}
}
#endif
