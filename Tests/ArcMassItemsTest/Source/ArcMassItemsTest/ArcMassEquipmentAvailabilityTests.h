// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Equipment/ArcMassEquipmentTypes.h"
#include "ArcMassEquipmentAvailabilityTests.generated.h"

/** Test-only fit condition that always returns false — exercises FArcMassEquipmentSlotCondition virtual dispatch. */
USTRUCT()
struct FArcTestEquipmentSlotCondition_AlwaysFalse : public FArcMassEquipmentSlotCondition
{
	GENERATED_BODY()

	virtual bool CanEquip(FMassEntityHandle /*OwnerEntity*/, FMassEntityManager& /*EntityManager*/,
		const FArcItemDefinitionData* /*InItemDefinition*/, const FArcItemData* /*InItemData*/) const override
	{
		return false;
	}
};
