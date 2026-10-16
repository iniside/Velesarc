// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fragments/ArcMassItemStoreFragment.h"

#include "ArcWorldSimStores.generated.h"

USTRUCT()
struct ARCWORLDSIM_API FArcBuildingInputStore : public FArcMassItemStoreFragment
{
	GENERATED_BODY()
};

template<>
struct TMassFragmentTraits<FArcBuildingInputStore> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

// Settlement storage uses this type: it is a take-from place, and receiving goods does not make it demand.
USTRUCT()
struct ARCWORLDSIM_API FArcBuildingOutputStore : public FArcMassItemStoreFragment
{
	GENERATED_BODY()
};

template<>
struct TMassFragmentTraits<FArcBuildingOutputStore> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
