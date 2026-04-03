#pragma once
#include "GameplayTagContainer.h"
#include "Mass/EntityElementTypes.h"
#include "MassEntityConcepts.h"

#include "ArcMassGameplayTagContainerFragment.generated.h"

USTRUCT()
struct FArcMassGameplayTagContainerFragment : public FMassFragment
{
	GENERATED_BODY()

public:
	FArcMassGameplayTagContainerFragment() = default;
	
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer Tags;
};

template<>
struct TMassFragmentTraits<FArcMassGameplayTagContainerFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};