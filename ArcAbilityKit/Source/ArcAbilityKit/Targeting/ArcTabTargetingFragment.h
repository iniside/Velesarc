// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "Mass/EntityHandle.h"
#include "Targeting/ArcTabTargetingTypes.h"
#include "ArcTabTargetingFragment.generated.h"

USTRUCT()
struct ARCABILITYKIT_API FArcTabTargetingFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMassEntityHandle> VisitedTargets;

	UPROPERTY()
	double LastPressTime = 0.0;

	UPROPERTY()
	FGameplayTag SelectedTargetTag;

	UPROPERTY()
	FArcTabTargetRetentionPolicy RetentionPolicy;
};

template <>
struct TMassFragmentTraits<FArcTabTargetingFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
