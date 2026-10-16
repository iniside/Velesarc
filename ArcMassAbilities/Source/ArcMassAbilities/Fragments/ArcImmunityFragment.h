// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Tags/ArcTagCountContainer.h"
#include "ArcImmunityFragment.generated.h"

USTRUCT()
struct ARCMASSABILITIES_API FArcImmunityFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	UPROPERTY()
	FArcTagCountContainer ImmunityTags;
};

template<>
struct TMassFragmentTraits<FArcImmunityFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
