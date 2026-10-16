#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ArcTargetingPipelineTypes.h"
#include "Mass/EntityHandle.h"
#include "UObject/StrongObjectPtr.h"

class UArcTargetingPreset;

struct ARCTARGETING_API FArcTargetingPersistentKey
{
	FMassEntityHandle OwnerEntity;
	FGameplayTag Tag;

	friend bool operator==(const FArcTargetingPersistentKey& A, const FArcTargetingPersistentKey& B)
	{
		return A.OwnerEntity == B.OwnerEntity && A.Tag == B.Tag;
	}

	friend uint32 GetTypeHash(const FArcTargetingPersistentKey& Key)
	{
		return HashCombine(GetTypeHash(Key.OwnerEntity), GetTypeHash(Key.Tag));
	}
};

struct FArcTargetingPersistentEntry
{
	FArcTargetingPersistentKey Key;
	FArcTargetingHandle Handle;
	TStrongObjectPtr<const UArcTargetingPreset> Preset;
	float MinInterval = 0.f;
	double LastCompletedSeconds = -1.0;
	TArray<FArcTargetCandidate> LatestCandidates;
	bool bRunPending = true;
	bool bHasCompleted = false;
};
