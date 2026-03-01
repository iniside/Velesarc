// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "AsyncMessageBindingEndpoint.h"
#include "ArcMassAsyncMessageEndpointFragment.generated.h"

/**
 * Per-entity mutable fragment holding an async message endpoint.
 * Allows systems to send async messages directly to specific Mass entities
 * without requiring actor hydration.
 */
USTRUCT()
struct ARCMASS_API FArcMassAsyncMessageEndpointFragment : public FMassFragment
{
	GENERATED_BODY()

	TSharedPtr<FAsyncMessageBindingEndpoint> Endpoint;
};

template<>
struct TMassFragmentTraits<FArcMassAsyncMessageEndpointFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/** Tag marking entities that have an async message endpoint. */
USTRUCT()
struct ARCMASS_API FArcMassAsyncMessageEndpointTag : public FMassTag
{
	GENERATED_BODY()
};
