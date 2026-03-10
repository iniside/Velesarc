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

#include "ArcCore/Conditions/ArcWorldConditionSchema.h"
#include "ArcTQSWorldConditionSchema.generated.h"

/**
 * World condition schema for Target Query System.
 * Extends base schema with target entity context data.
 */
UCLASS()
class ARCAI_API UArcTQSWorldConditionSchema : public UArcWorldConditionSchema
{
	GENERATED_BODY()

public:
	explicit UArcTQSWorldConditionSchema(const FObjectInitializer& ObjectInitializer);

	FWorldConditionContextDataRef GetTargetEntityRef() const { return TargetEntityRef; }
	FWorldConditionContextDataRef GetTargetActorRef() const { return TargetActorRef; }

protected:
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;

private:
	FWorldConditionContextDataRef TargetEntityRef;
	FWorldConditionContextDataRef TargetActorRef;
};
