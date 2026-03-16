// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaPostVacancyTask.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcAreaSubsystem.h"
#include "ArcAreaVacancyPayload.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "MassEntityView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAreaPostVacancyTask)

EStateTreeRunStatus FArcAreaPostVacancyTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bSucceeded = false;
	InstanceData.VacancyHandle = {};

	if (!InstanceData.SlotHandle.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	const FMassStateTreeExecutionContext& MassCtx = static_cast<const FMassStateTreeExecutionContext&>(Context);

	UWorld* World = MassCtx.GetWorld();
	const UArcAreaSubsystem* AreaSubsystem = World->GetSubsystem<UArcAreaSubsystem>();
	UArcKnowledgeSubsystem* KnowledgeSubsystem = World->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!AreaSubsystem || !KnowledgeSubsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FArcAreaData* AreaData = AreaSubsystem->GetAreaData(InstanceData.SlotHandle.AreaHandle);
	if (!AreaData || !AreaData->SlotDefinitions.IsValidIndex(InstanceData.SlotHandle.SlotIndex))
	{
		return EStateTreeRunStatus::Failed;
	}

	const FArcAreaSlotDefinition& SlotDef = AreaData->SlotDefinitions[InstanceData.SlotHandle.SlotIndex];

	// Build vacancy entry — same pattern as UArcAreaAutoVacancyListener::PostVacancy
	FArcKnowledgeEntry VacancyEntry;

	if (!SlotDef.VacancyConfig.VacancyTags.IsEmpty())
	{
		VacancyEntry.Tags = SlotDef.VacancyConfig.VacancyTags;
	}
	else
	{
		VacancyEntry.Tags.AddTag(FGameplayTag::RequestGameplayTag(FName("Area.Vacancy")));
		VacancyEntry.Tags.AppendTags(AreaData->AreaTags);
	}

	VacancyEntry.Location = AreaData->Location;
	VacancyEntry.SourceEntity = AreaData->OwnerEntity;
	VacancyEntry.Relevance = SlotDef.VacancyConfig.VacancyRelevance;

	// Attach payload so NPCs can identify which area/slot this vacancy belongs to
	FArcAreaVacancyPayload Payload;
	Payload.SlotHandle = InstanceData.SlotHandle;
	VacancyEntry.Payload.InitializeAs<FArcAreaVacancyPayload>(Payload);

	InstanceData.VacancyHandle = KnowledgeSubsystem->PostAdvertisement(VacancyEntry);
	InstanceData.bSucceeded = InstanceData.VacancyHandle.IsValid();

	return InstanceData.bSucceeded ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}
