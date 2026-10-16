// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "UtilityAI/ArcUtilityConsideration.h"
#include "ArcUtilityConsideration_EntityHealth.generated.h"

/**
 * Reads the target Mass entity's health/max-health attributes (designer-configured
 * via FArcAttributeRef) and returns Health / MaxHealth as a [0,1] score.
 */
USTRUCT(BlueprintType, DisplayName = "Entity Health Consideration")
struct ARCAI_API FArcUtilityConsideration_EntityHealth : public FArcUtilityConsideration
{
	GENERATED_BODY()

	/** Attribute ref pointing at the entity's current-health attribute. */
	UPROPERTY(EditAnywhere, Category = "Health")
	FArcAttributeRef HealthAttribute;

	/** Attribute ref pointing at the entity's max-health attribute. */
	UPROPERTY(EditAnywhere, Category = "Health")
	FArcAttributeRef MaxHealthAttribute;

	/** Whose health this reads: the querier itself, or the candidate target. */
	UPROPERTY(EditAnywhere, Category = "Health")
	EArcUtilitySubject Subject = EArcUtilitySubject::Target;

	virtual FArcUtilityConsiderationResult Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;

#if WITH_EDITOR
	virtual EArcUtilityConsiderationRequirements GetRequirements() const override
	{
		return Subject == EArcUtilitySubject::Target ? EArcUtilityConsiderationRequirements::TargetEntity : EArcUtilityConsiderationRequirements::None;
	}
#endif
};
