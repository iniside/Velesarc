// Copyright Lukasz Baran. All Rights Reserved.

#include "Perception/ArcSharedTargetKnowledge.h"

#include "ArcKnowledgeEntry.h"
#include "ArcKnowledgeSubsystem.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_AI_Knowledge_SharedTarget, "AI.Knowledge.SharedTarget");

double ArcSharedTargetKnowledge::GetRetentionSeconds()
{
	// The sweep reads a Lifetime of exactly zero as "never expires", so an authored retention of
	// zero must still be written as the shortest finite lifetime: reclaimed by the next sweep.
	const double Authored = static_cast<double>(GetDefault<UArcSharedTargetKnowledgeSettings>()->RetentionSeconds);
	return FMath::IsFinite(Authored) ? FMath::Max(Authored, UE_DOUBLE_SMALL_NUMBER) : UE_DOUBLE_SMALL_NUMBER;
}

FArcKnowledgeHandle ArcSharedTargetKnowledge::FindTeamEntry(
	const UArcKnowledgeSubsystem& Subsystem,
	FMassEntityHandle Target,
	int32 TeamId)
{
	TArray<FArcKnowledgeHandle> Handles;
	Subsystem.GetKnowledgeBySource(Target, Handles);

	for (const FArcKnowledgeHandle& Handle : Handles)
	{
		const FArcKnowledgeEntry* Entry = Subsystem.GetKnowledgeEntry(Handle);
		if (!Entry || !Entry->Tags.HasTagExact(TAG_AI_Knowledge_SharedTarget))
		{
			continue;
		}

		const FArcSharedTargetPayload* Payload = Entry->Payload.GetPtr<FArcSharedTargetPayload>();
		if (Payload && Payload->TeamId == TeamId)
		{
			return Handle;
		}
	}

	return FArcKnowledgeHandle();
}

bool ArcSharedTargetKnowledge::PublishSighting(
	UArcKnowledgeSubsystem& Subsystem,
	FMassEntityHandle Target,
	int32 TeamId,
	const FVector& Location,
	double SightingTime,
	double RefreshInterval)
{
	if (!Target.IsValid() || !FMath::IsFinite(SightingTime) || Location.ContainsNaN())
	{
		return false;
	}

	const FArcKnowledgeHandle ExistingHandle = FindTeamEntry(Subsystem, Target, TeamId);
	if (ExistingHandle.IsValid())
	{
		const FArcKnowledgeEntry* Stored = Subsystem.GetKnowledgeEntry(ExistingHandle);
		if (!Stored)
		{
			return false;
		}

		if (SightingTime <= Stored->Timestamp)
		{
			return false;
		}

		// Deliberately an exact comparison rather than the planned distance threshold: a threshold in
		// centimetres has no source here, and inventing a speed to derive one from RefreshInterval
		// would be a magic constant. A stationary target still costs nothing - it fails this test.
		const bool bMoved = Stored->Location != Location;
		if (!bMoved && (SightingTime - Stored->Timestamp) < RefreshInterval)
		{
			return false;
		}

		// UpdateKnowledge overwrites every field but Handle, so the update is read-modify-write.
		FArcKnowledgeEntry Updated = *Stored;
		Updated.Location = Location;
		Updated.Timestamp = SightingTime;
		Updated.Lifetime = static_cast<float>(GetRetentionSeconds());
		Subsystem.UpdateKnowledge(ExistingHandle, Updated);
		return true;
	}

	FArcSharedTargetPayload Payload;
	Payload.TeamId = TeamId;

	FArcKnowledgeEntry Entry;
	Entry.Tags.AddTag(TAG_AI_Knowledge_SharedTarget);
	Entry.Location = Location;
	Entry.SourceEntity = Target;
	Entry.Timestamp = SightingTime;
	Entry.Lifetime = static_cast<float>(GetRetentionSeconds());
	Entry.SpatialBroadcastRadius = FArcKnowledgeEntry::SuppressSpatialBroadcast;
	Entry.Payload.InitializeAs<FArcSharedTargetPayload>(Payload);

	Subsystem.RegisterKnowledge(Entry);
	return true;
}
