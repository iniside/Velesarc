// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcExtraCapture.h"

struct FArcEvaluateParameters;

#include "ArcExtraCaptureSource_Item.generated.h"

/** Resolves the item's Item Stats value (D-77 / D42 read rules) of the entries for Attribute whose StatTags are all in
 *  EvalParams.SourceTags, from the item carried in FArcEffectContext::SourceData
 *  (FArcAbilitySource_Item). Returns false when the context source is not item-flavored,
 *  the item handle is invalid, or the item has no ItemStats instance. */
USTRUCT(BlueprintType, meta=(DisplayName="Item Stat"))
struct ARCABILITYKIT_API FArcExtraCaptureSource_Item : public FArcExtraCaptureSource
{
	GENERATED_BODY()

	/** The attribute to look up in the item's FArcItemInstance_ItemStats. */
	UPROPERTY(EditAnywhere)
	FArcAttributeRef Attribute;

	virtual bool ResolveValue(
		const FArcEffectContext& Context,
		FMassEntityManager& EntityManager,
		const FArcEvaluateParameters& EvalParams,
		float& OutValue) const override;
};
