// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaPostVacancyTask.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcAreaSubsystem.h"
#include "ArcArea/Mass/ArcAreaFragments.h"
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

	const FMassStateTreeExecutionContext& MassCtx = static_cast<const FMassStateTreeExecutionContext&>(Context);
	const FMassEntityView EntityView(MassCtx.GetEntityManager(), MassCtx.GetEntity());
	const FArcAreaFragment* AreaFragment = EntityView.GetFragmentDataPtr<FArcAreaFragment>();
	if (!AreaFragment || !AreaFragment->AreaHandle.IsValid() || InstanceData.SlotIndex == INDEX_NONE)
	{
		return EStateTreeRunStatus::Failed;
	}

	UWorld* World = MassCtx.GetWorld();
	const UArcAreaSubsystem* AreaSubsystem = World->GetSubsystem<UArcAreaSubsystem>();
	UArcKnowledgeSubsystem* KnowledgeSubsystem = World->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!AreaSubsystem || !KnowledgeSubsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FArcAreaData* AreaData = AreaSubsystem->GetAreaData(AreaFragment->AreaHandle);
	if (!AreaData || !AreaData->SlotDefinitions.IsValidIndex(InstanceData.SlotIndex))
	{
		return EStateTreeRunStatus::Failed;
	}

	const FArcAreaSlotDefinition& SlotDef = AreaData->SlotDefinitions[InstanceData.SlotIndex];

	// Build vacancy entry — same pattern as UArcAreaAutoVacancyListener::PostVacancy
	FArcKnowledgeEntry VacancyEntry;

	if (!SlotDef.VacancyConfig.VacancyTags.IsEmpty())
	{
		VacancyEntry.Tags = SlotDef.VacancyConfig.VacancyTags;
	}
	else
	{
		VacancyEntry.Tags.AddTag(FGameplayTag::RequestGameplayTag(FName("Area.Vacancy")));
		if (SlotDef.RoleTag.IsValid())
		{
			VacancyEntry.Tags.AddTag(SlotDef.RoleTag);
		}
	}

	VacancyEntry.Location = AreaData->Location;
	VacancyEntry.SourceEntity = AreaData->OwnerEntity;
	VacancyEntry.Relevance = SlotDef.VacancyConfig.VacancyRelevance;

	// Attach payload so NPCs can identify which area/slot this vacancy belongs to
	FArcAreaVacancyPayload Payload;
	Payload.AreaHandle = AreaFragment->AreaHandle;
	Payload.SlotIndex = InstanceData.SlotIndex;
	Payload.RoleTag = SlotDef.RoleTag;
	VacancyEntry.Payload.InitializeAs<FArcAreaVacancyPayload>(Payload);

	InstanceData.VacancyHandle = KnowledgeSubsystem->PostAdvertisement(VacancyEntry);
	InstanceData.bSucceeded = InstanceData.VacancyHandle.IsValid();

	return InstanceData.bSucceeded ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}
