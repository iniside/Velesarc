// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTraitBase.h"
#include "ArcMassConfigTestTraits.generated.h"

/**
 * Minimal test trait A. Carries a Value UPROPERTY so tests can produce two semantically
 * distinct instances of the same class — TraitsEquivalent will return false when Values differ,
 * exercising the "variant overrode" branch in SetNewOrReplaceTemplate / UpdateFromTemplate.
 */
UCLASS(NotBlueprintable, NotBlueprintType, MinimalAPI)
class UArcMassConfigTestTraitA : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Value = 0;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override {}
};

/**
 * Minimal test trait B. No extra data; used to populate a second distinct class slot.
 */
UCLASS(NotBlueprintable, NotBlueprintType, MinimalAPI)
class UArcMassConfigTestTraitB : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override {}
};
