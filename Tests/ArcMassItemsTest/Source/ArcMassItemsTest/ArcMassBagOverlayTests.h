// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Items/ArcItemData.h"
#include "Bags/ArcBagDefinition.h"
#include "ArcMassBagOverlayTests.generated.h"

/**
 * Custom bag acceptance condition used in bag overlay unit tests.
 * Always rejects the item regardless of its properties.
 */
USTRUCT(meta = (Hidden))
struct ARCMASSITEMSTEST_API FArcBagTestRejectCondition : public FArcBagAcceptanceCondition
{
	GENERATED_BODY()

	virtual bool CanAccept(const FArcItemData& Item) const override { return false; }
};
