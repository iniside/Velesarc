// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "MassEntityTraitBase.h"
#include "SmartObjectTypes.h"

#include "ArcMassSmartObjectFragments.generated.h"

class USmartObjectDefinition;
class UInputMappingContext;

/**
 * Tag that triggers automatic SmartObject creation/destruction via observers.
 * Any entity with this tag plus FArcSmartObjectOwnerFragment and
 * FArcSmartObjectDefinitionSharedFragment will have its SmartObject managed automatically.
 */
USTRUCT()
struct ARCMASS_API FArcSmartObjectTag : public FMassTag
{
	GENERATED_BODY()
};

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

	UPROPERTY(EditAnywhere)
	TObjectPtr<UInputMappingContext> InteractionMappingContext = nullptr;
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
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Smart Object Owner", Category = "SmartObjects"))
class ARCMASS_API UArcMassSmartObjectOwnerTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FArcSmartObjectDefinitionSharedFragment SmartObjectDefinition;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
