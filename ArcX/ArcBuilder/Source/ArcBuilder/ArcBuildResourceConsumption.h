// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcBuildResourceConsumption.generated.h"

class UArcBuilderComponent;
class UArcBuildingDefinition;

/**
 * Base resource consumption strategy for building placement.
 * Checks whether required resources are available and optionally consumes them.
 * Configured per building definition via instanced struct.
 */
USTRUCT(BlueprintType)
struct ARCBUILDER_API FArcBuildResourceConsumption
{
	GENERATED_BODY()

public:
	/**
	 * Check if resources are available. If bConsume is true, also deduct them.
	 * @return True if all requirements are met (and consumed if requested).
	 */
	virtual bool CheckAndConsumeResources(
		const UArcBuildingDefinition* BuildingDef,
		const UArcBuilderComponent* BuilderComponent,
		bool bConsume) const;

	virtual ~FArcBuildResourceConsumption() = default;
};

/**
 * Default implementation: finds UArcItemsStoreComponent on the builder's
 * owner and checks/consumes items against the building definition's ingredients.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Item Store Consumption"))
struct ARCBUILDER_API FArcBuildResourceConsumption_ItemStore : public FArcBuildResourceConsumption
{
	GENERATED_BODY()

public:
	virtual bool CheckAndConsumeResources(
		const UArcBuildingDefinition* BuildingDef,
		const UArcBuilderComponent* BuilderComponent,
		bool bConsume) const override;
};
