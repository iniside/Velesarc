// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanics/Resources/ArcResourceProcessor.h"

#include "ArcMassMechanics/ArcMassMechanicsLogs.h"
#include "ArcMassMechanics/Resources/ArcResourceTypes.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcAttributeHandlerSharedFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Mass/EntityManager.h"
#include "Mass/ExecutionContext.h"
#include "Modifiers/ArcModifierFunctions.h"

UArcResourceProcessor::UArcResourceProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	bRequiresGameThreadExecution = true;
}

void UArcResourceProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcResourceStateFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcAggregatorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddSparseRequirement<FArcOwnedTagsFragment>(EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcResourceConfigSharedFragment>();
	EntityQuery.AddConstSharedRequirement<FArcAttributeHandlerSharedFragment>(EMassFragmentPresence::Optional);
}

void UArcResourceProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcResourceProcessor);

	const float DeltaTime = Context.GetDeltaTimeSeconds();

	EntityQuery.ForEachEntityChunk(Context, [this, &EntityManager, DeltaTime](FMassExecutionContext& Ctx)
	{
		const UArcResourceConfig* Config = Ctx.GetConstSharedFragment<FArcResourceConfigSharedFragment>().Config;
		if (!Config)
		{
			return;
		}

		const FArcAttributeHandlerSharedFragment* HandlerShared = Ctx.GetConstSharedFragmentPtr<FArcAttributeHandlerSharedFragment>();
		const UArcAttributeHandlerConfig* HandlerConfig = HandlerShared ? HandlerShared->Config.Get() : nullptr;

		const TArrayView<FArcResourceStateFragment> States = Ctx.GetMutableFragmentView<FArcResourceStateFragment>();
		const float Interval = FMath::Max(0.f, Config->UpdateInterval);

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcResourceStateFragment& State = States[EntityIt];
			State.UpdateAccumulator += DeltaTime;
			if (State.UpdateAccumulator < Interval)
			{
				continue;
			}

			const float StepSeconds = State.UpdateAccumulator;
			State.UpdateAccumulator = 0.f;

			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
			const FArcOwnedTagsFragment* OwnedTags = EntityManager.GetSparseElementDataForEntity(FArcOwnedTagsFragment::StaticStruct(), Entity).GetPtr<const FArcOwnedTagsFragment>();
			UpdateEntity(EntityManager, Entity, *Config, HandlerConfig, OwnedTags ? &OwnedTags->GetTags() : nullptr, StepSeconds);
		}
	});
}

void UArcResourceProcessor::UpdateEntity(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UArcResourceConfig& Config,
	const UArcAttributeHandlerConfig* HandlerConfig, const FArcTagCountContainer* OwnedTags, float StepSeconds)
{
	for (const FArcResourceDef& Resource : Config.Resources)
	{
		const FArcAttribute* ValueAttribute = ArcAttributes::ResolveAttribute(EntityManager, Entity, Resource.Value);
		if (!ValueAttribute)
		{
			continue;
		}

		if (!OwnedTags && !Resource.BlockedBy.IsEmpty())
		{
			bool bAlreadyLogged = false;
			ConfigsLoggedWithoutOwnedTags.Add(FObjectKey(&Config), &bAlreadyLogged);
			if (!bAlreadyLogged)
			{
				UE_LOGF(LogArcMassMechanics, Warning, "Entities using resource config %ls have no FArcOwnedTagsFragment; BlockedBy never blocks them.", *Config.GetName());
			}
		}

		FArcResourceContext ResourceContext(EntityManager, Resource);
		ResourceContext.Entity = Entity;
		ResourceContext.OwnedTags = OwnedTags;
		ResourceContext.CurrentValue = ValueAttribute->GetCurrentValue();

		const float Step = Resource.ComputeStep(ResourceContext, StepSeconds);
		if (Step != 0.f && Resource.Delta.IsValid())
		{
			MetaWriter.Write(EntityManager, Entity, HandlerConfig, Resource.Delta, Step, Config);
		}
	}
}
