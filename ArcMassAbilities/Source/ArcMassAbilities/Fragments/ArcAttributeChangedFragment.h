// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "MassEntityTypes.h"
#include "Mass/EntityElementTypes.h"
#include "ArcAttributeChangedFragment.generated.h"

USTRUCT()
struct ARCMASSABILITIES_API FArcAttributeChangeEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FArcAttributeRef Attribute;

	UPROPERTY()
	float OldValue = 0.f;

	UPROPERTY()
	float NewValue = 0.f;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcAttributeChangedFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	TArray<FArcAttributeChangeEntry> Changes;
};

template <>
struct TMassFragmentTraits<FArcAttributeChangedFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
