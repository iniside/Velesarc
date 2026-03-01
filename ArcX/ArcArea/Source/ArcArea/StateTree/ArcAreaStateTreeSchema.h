// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeSchema.h"
#include "ArcAreaStateTreeSchema.generated.h"

/**
 * StateTree schema for Area entities.
 * Extends the Mass behavior schema so all Mass tasks/conditions/evaluators work.
 * Used for asset filtering — Area StateTrees must use this schema.
 *
 * External data auto-resolved from the entity:
 * - FArcAreaFragment (mutable) — area handle
 * - FArcAreaConfigFragment (const shared) — area definition reference
 * - UArcAreaSubsystem (world subsystem) — area data queries
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Area Behavior"))
class ARCAREA_API UArcAreaStateTreeSchema : public UMassStateTreeSchema
{
	GENERATED_BODY()
};
