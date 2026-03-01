// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "MassEntityTraitBase.h"
#include "SmartObjectTypes.h"

#include "ArcMassSmartObjectFragments.generated.h"

class USmartObjectDefinition;

/**
 * Const shared fragment: references the SmartObject definition asset.
 * Shared across all entities of the same archetype.
 */
USTRUCT()
struct ARCMASS_API FArcSmartObjectDefinitionSharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TObjectPtr<USmartObjectDefinition> SmartObjectDefinition;
};

template<>
struct TMassFragmentTraits<FArcSmartObjectDefinitionSharedFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/**
 * Per-entity mutable fragment: stores the runtime handle of a SmartObject owned by this entity.
 * The entity creates and owns this SmartObject — cleanup must destroy it.
 */
USTRUCT()
struct ARCMASS_API FArcSmartObjectOwnerFragment : public FMassFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere)
	FSmartObjectHandle SmartObjectHandle;
};

/**
 * Trait that adds SmartObject ownership to a Mass entity.
 * Adds FArcSmartObjectOwnerFragment and FArcSmartObjectDefinitionSharedFragment.
 * SmartObject creation must be handled by an observer or spawning system.
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Smart Object Owner"))
class ARCMASS_API UArcMassSmartObjectOwnerTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FArcSmartObjectDefinitionSharedFragment SmartObjectDefinition;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
