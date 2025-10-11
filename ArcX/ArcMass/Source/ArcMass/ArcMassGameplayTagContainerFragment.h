#pragma once
#include "GameplayTagContainer.h"
#include "MassEntityElementTypes.h"
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