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

void UArcAreaAutoVacancyListener::OnSlotStateChanged(FArcAreaHandle AreaHandle, int32 SlotIndex, EArcAreaSlotState NewState)
{
	if (NewState == EArcAreaSlotState::Vacant)
	{
		PostVacancy(AreaHandle, SlotIndex);
	}
	else
	{
		RemoveVacancy(AreaHandle, SlotIndex);
	}
}

void UArcAreaAutoVacancyListener::PostVacancy(FArcAreaHandle AreaHandle, int32 SlotIndex)
{
	const UArcAreaSubsystem* AreaSub = GetWorld()->GetSubsystem<UArcAreaSubsystem>();
	if (!AreaSub)
	{
		return;
	}

	const FArcAreaData* Data = AreaSub->GetAreaData(AreaHandle);
	if (!Data || !Data->SlotDefinitions.IsValidIndex(SlotIndex))
	{
		return;
	}

	const FArcAreaSlotDefinition& SlotDef = Data->SlotDefinitions[SlotIndex];
	if (!SlotDef.VacancyConfig.bAutoPostVacancy)
	{
		return;
	}

	FArcAreaSlotHandle Key;
	Key.AreaHandle = AreaHandle;
	Key.SlotIndex = SlotIndex;

	// Don't double-post
	if (const FArcKnowledgeHandle* Existing = PostedVacancies.Find(Key))
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

	// Build tags: use custom tags or auto-construct from role
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

	VacancyEntry.Location = Data->Location;
	VacancyEntry.SourceEntity = Data->OwnerEntity;
	VacancyEntry.Relevance = SlotDef.VacancyConfig.VacancyRelevance;

	// Attach payload so NPCs can identify which area/slot this vacancy belongs to
	FArcAreaVacancyPayload Payload;
	Payload.AreaHandle = AreaHandle;
	Payload.SlotIndex = SlotIndex;
	Payload.RoleTag = SlotDef.RoleTag;
	VacancyEntry.Payload.InitializeAs<FArcAreaVacancyPayload>(Payload);

	PostedVacancies.Add(Key, KnowledgeSub->PostAdvertisement(VacancyEntry));
}

void UArcAreaAutoVacancyListener::RemoveVacancy(FArcAreaHandle AreaHandle, int32 SlotIndex)
{
	FArcAreaSlotHandle Key;
	Key.AreaHandle = AreaHandle;
	Key.SlotIndex = SlotIndex;

	FArcKnowledgeHandle Handle;
	if (!PostedVacancies.RemoveAndCopyValue(Key, Handle))
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
