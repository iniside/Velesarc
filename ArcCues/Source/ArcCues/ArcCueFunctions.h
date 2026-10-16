// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"

#include "ArcCueTypes.h"

class UWorld;
class UArcCueDefinition;
struct FStateTreeEvent;

/**
 * Public free-function entry points for the cue system. UWorld* (not
 * FMassEntityManager&) is the entry-point because non-entity callers exist
 * (camera shake on a player controller, world-space niagara). The subsystem
 * fetches FMassEntityManager from UWorld when needed.
 *
 * Per project rule: every free function lives in a named namespace.
 */
namespace ArcCues
{
	/** One-shot — runs OnExecute on every behavior of every matching definition. */
	ARCCUES_API void ExecuteCue(UWorld* World, FGameplayTag CueTag, const FArcCueParameters& Params);
	ARCCUES_API void ExecuteCue(UWorld* World, UArcCueDefinition* Definition, const FArcCueParameters& Params);

	/** Persistent — creates a retained instance and runs OnApplication. */
	ARCCUES_API FArcCueHandle AddCue(UWorld* World, FGameplayTag CueTag, const FArcCueParameters& Params);
	ARCCUES_API FArcCueHandle AddCue(UWorld* World, UArcCueDefinition* Definition, const FArcCueParameters& Params);

	ARCCUES_API void SendCueEvent(UWorld* World, FArcCueHandle Handle, const FArcCueEvent& Event);
	ARCCUES_API void SendCueStateTreeEvent(UWorld* World, FArcCueHandle Handle, const FStateTreeEvent& Event);
	ARCCUES_API void RemoveCue(UWorld* World, FArcCueHandle Handle);
	ARCCUES_API void RemoveCuesByTag(UWorld* World, FGameplayTag CueTag, FMassEntityHandle TargetEntity);
	ARCCUES_API void RemoveAllCues(UWorld* World, FMassEntityHandle TargetEntity);
	ARCCUES_API void RemoveCuesByOwner(UWorld* World, FMassEntityHandle OwnerEntity);
}
