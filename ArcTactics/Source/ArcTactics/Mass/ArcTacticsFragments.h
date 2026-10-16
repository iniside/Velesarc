// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Decision/ArcTacticsCompiledData.h"
#include "Mass/ArcTacticsInstanceHandle.h"
#include "Mass/ArchetypeCompositionTypes.h"

#include "ArcTacticsFragments.generated.h"

class UArcTacticsConfig;

USTRUCT()
struct ARCTACTICS_API FArcTacticsTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct ARCTACTICS_API FArcTacticsInstanceFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Tactics")
	FArcTacticsInstanceHandle Handle;

	UPROPERTY(VisibleAnywhere, Category = "Tactics")
	bool bEnabled = true;
};

USTRUCT()
struct ARCTACTICS_API FArcTacticsConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<const UArcTacticsConfig>> Configs;

	UPROPERTY()
	FArcTacticsDecisionSettings Settings;
};

template <>
struct TMassFragmentTraits<FArcTacticsConfigFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
