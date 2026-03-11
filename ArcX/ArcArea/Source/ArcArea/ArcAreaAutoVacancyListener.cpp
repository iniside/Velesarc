// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaAutoVacancyListener.h"
#include "ArcAreaSubsystem.h"
#include "ArcAreaVacancyPayload.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "Engine/World.h"

void UArcAreaAutoVacancyListener::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UArcAreaSubsystem>();
	Collection.InitializeDependency<UArcKnowledgeSubsystem>();

	if (UArcAreaSubsystem* AreaSub = GetWorld()->GetSubsystem<UArcAreaSubsystem>())
	{
		DelegateHandle = AreaSub->AddOnSlotStateChanged(
			FArcAreaSlotStateChanged::FDelegate::CreateUObject(this, &UArcAreaAutoVacancyListener::OnSlotStateChanged));
	}
}

void UArcAreaAutoVacancyListener::Deinitialize()
{
	if (UArcAreaSubsystem* AreaSub = GetWorld()->GetSubsystem<UArcAreaSubsystem>())
	{
		AreaSub->RemoveOnSlotStateChanged(DelegateHandle);
	}

	// Clean up all posted vacancies
	if (UArcKnowledgeSubsystem* KnowledgeSub = GetWorld()->GetSubsystem<UArcKnowledgeSubsystem>())
	{
		for (const auto& Pair : PostedVacancies)
		{
			if (Pair.Value.IsValid())
			{
				KnowledgeSub->RemoveKnowledge(Pair.Value);
			}
		}
	}

	PostedVacancies.Empty();
	Super::Deinitialize();
}

void UArcAreaAutoVacancyListener::OnSlotStateChanged(const FArcAreaSlotHandle& SlotHandle, EArcAreaSlotState NewState)
{
	if (NewState == EArcAreaSlotState::Vacant)
	{
		PostVacancy(SlotHandle);
	}
	else
	{
		RemoveVacancy(SlotHandle);
	}
}

void UArcAreaAutoVacancyListener::PostVacancy(const FArcAreaSlotHandle& SlotHandle)
{
	const UArcAreaSubsystem* AreaSub = GetWorld()->GetSubsystem<UArcAreaSubsystem>();
	if (!AreaSub)
	{
		return;
	}

	const FArcAreaData* Data = AreaSub->GetAreaData(SlotHandle.AreaHandle);
	if (!Data || !Data->SlotDefinitions.IsValidIndex(SlotHandle.SlotIndex))
	{
		return;
	}

	const FArcAreaSlotDefinition& SlotDef = Data->SlotDefinitions[SlotHandle.SlotIndex];
	if (!SlotDef.VacancyConfig.bAutoPostVacancy)
	{
		return;
	}

	// Don't double-post
	if (const FArcKnowledgeHandle* Existing = PostedVacancies.Find(SlotHandle))
	{
		if (Existing->IsValid())
		{
			return;
		}
	}

	UArcKnowledgeSubsystem* KnowledgeSub = GetWorld()->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!KnowledgeSub)
	{
		return;
	}

	FArcKnowledgeEntry VacancyEntry;

	// Build tags: use custom vacancy tags or auto-construct from area tags
	if (!SlotDef.VacancyConfig.VacancyTags.IsEmpty())
	{
		VacancyEntry.Tags = SlotDef.VacancyConfig.VacancyTags;
	}
	else
	{
		VacancyEntry.Tags.AddTag(FGameplayTag::RequestGameplayTag(FName("Area.Vacancy")));
		VacancyEntry.Tags.AppendTags(Data->AreaTags);
	}

	VacancyEntry.Location = Data->Location;
	VacancyEntry.SourceEntity = Data->OwnerEntity;
	VacancyEntry.Relevance = SlotDef.VacancyConfig.VacancyRelevance;

	// Attach payload so NPCs can identify which area/slot this vacancy belongs to
	FArcAreaVacancyPayload Payload;
	Payload.SlotHandle = SlotHandle;
	VacancyEntry.Payload.InitializeAs<FArcAreaVacancyPayload>(Payload);

	PostedVacancies.Add(SlotHandle, KnowledgeSub->PostAdvertisement(VacancyEntry));
}

void UArcAreaAutoVacancyListener::RemoveVacancy(const FArcAreaSlotHandle& SlotHandle)
{
	FArcKnowledgeHandle Handle;
	if (!PostedVacancies.RemoveAndCopyValue(SlotHandle, Handle))
	{
		return;
	}

	if (!Handle.IsValid())
	{
		return;
	}

	if (UArcKnowledgeSubsystem* KnowledgeSub = GetWorld()->GetSubsystem<UArcKnowledgeSubsystem>())
	{
		KnowledgeSub->RemoveKnowledge(Handle);
	}
}
