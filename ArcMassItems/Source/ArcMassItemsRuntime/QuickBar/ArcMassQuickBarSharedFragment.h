// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "ArcMassQuickBarSharedFragment.generated.h"

class UArcMassQuickBarConfig;

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassQuickBarSharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<UArcMassQuickBarConfig>> Configs;
};

template<>
struct TMassFragmentTraits<FArcMassQuickBarSharedFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
