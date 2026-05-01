// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/StructView.h"
#include "ArcKnowledgeTypes.h"
#include "ArcKnowledgeEntry.h"

/**
 * Lightweight view struct for query pipeline evaluation.
 * Holds just the fields that filters/scorers need, avoiding full entry copies.
 * Both static (R-tree) and dynamic (subsystem TMap) tiers produce these.
 * NOT a USTRUCT — contains raw pointer and FStructView.
 */
struct ARCKNOWLEDGE_API FArcKnowledgeQueryCandidate
{
	FVector Location = FVector::ZeroVector;

	/** Non-owning pointer to the entry's tag container. Lifetime tied to source. */
	const FGameplayTagContainer* Tags = nullptr;

	FMassEntityHandle Entity;

	/** Non-owning view into the entry's payload. May be empty. */
	FStructView Payload;

	FArcKnowledgeHandle Handle;

	float Relevance = 1.0f;
	double Timestamp = 0.0;
	bool bClaimed = false;

	/** Build a candidate from a knowledge entry (dynamic or promoted static). */
	static FArcKnowledgeQueryCandidate FromEntry(const FArcKnowledgeEntry& Entry)
	{
		FArcKnowledgeQueryCandidate Candidate;
		Candidate.Location = Entry.Location;
		Candidate.Tags = &Entry.Tags;
		Candidate.Entity = Entry.SourceEntity;
		Candidate.Payload = Entry.Payload.IsValid()
			? FStructView(const_cast<UScriptStruct*>(Entry.Payload.GetScriptStruct()), const_cast<uint8*>(Entry.Payload.GetMemory()))
			: FStructView();
		Candidate.Handle = Entry.Handle;
		Candidate.Relevance = Entry.Relevance;
		Candidate.Timestamp = Entry.Timestamp;
		Candidate.bClaimed = Entry.bClaimed;
		return Candidate;
	}
};
