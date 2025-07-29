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
#include "Nodes/Input/CameraRigInput2DSlot.h"

#include "ArcInputAxisBinding2DCameraNode.generated.h"

class UInputAction;

/**
 * An input node that reads player input from an input action.
 */
UCLASS(MinimalAPI, meta=(CameraNodeCategories="Input"))
class UArcInputAxisBinding2DCameraNode : public UCameraRigInput2DSlot
{
	GENERATED_BODY()

public:

	/** The axis input action(s) to read from. */
	UPROPERTY(EditAnywhere, Category="Input")
	TArray<TObjectPtr<UInputAction>> AxisActions;

	/** A multiplier to use on the input values. */
	UPROPERTY(EditAnywhere, Category="Input Processing")
	FVector2dCameraParameter Multiplier;

public:

	UArcInputAxisBinding2DCameraNode(const FObjectInitializer& ObjInit);
	
protected:

	// UCameraNode interface.
	
	virtual FCameraNodeEvaluatorPtr OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const override;
};