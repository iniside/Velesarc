/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once

#include "Engine/DataAsset.h"
#include "WorldConditionQuery.h"
#include "ArcWorldConditionDefinition.generated.h"

class UWorldConditionSchema;

/**
 * DataAsset wrapping a WorldCondition query definition.
 * Select a Schema to control which conditions are available in the picker.
 * Reference this from entities, components, or other assets to share condition logic.
 */
UCLASS(BlueprintType)
class ARCCORE_API UArcWorldConditionDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	const FWorldConditionQueryDefinition& GetQueryDefinition() const { return QueryDefinition; }

	/** Check if this definition has valid, initialized conditions. */
	bool IsValid() const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual void PostLoad() override;

protected:
	/** Schema controlling which conditions are available and what context data they can access. */
	UPROPERTY(EditAnywhere, Category = "Conditions")
	TSubclassOf<UWorldConditionSchema> SchemaClass;

	UPROPERTY(EditAnywhere, Category = "Conditions")
	FWorldConditionQueryDefinition QueryDefinition;
};
