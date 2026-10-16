// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Attributes/ArcAttribute.h"

#include "ArcStatDisplayRegistry.generated.h"

/** Display metadata for a single item stat, keyed by FArcAttributeRef in the registry. */
USTRUCT(BlueprintType)
struct ARCUIMODELS_API FArcStatDisplayData
{
	GENERATED_BODY()

	/** Human-readable stat name shown in tooltips (e.g. "Damage", "Fire Rate"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	/** Unit suffix appended after the value (e.g. "%", "s"). Empty for unitless stats. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Suffix;
};

/**
 * Authorable registry mapping an item-stat FArcAttributeRef to its display name + suffix.
 * Mirrors UArcTagDisplayRegistry; lives in ArcUIModels (not ArcCore) because FArcAttributeRef
 * belongs to ArcMassAbilities and ArcCore must not depend on it.
 */
UCLASS(BlueprintType)
class ARCUIMODELS_API UArcStatDisplayRegistry : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FArcAttributeRef, FArcStatDisplayData> Entries;

	/** Exact {FragmentType, PropertyName} match. Returns nullptr on miss (caller humanizes). */
	const FArcStatDisplayData* FindDisplay(const FArcAttributeRef& Stat) const;
};
