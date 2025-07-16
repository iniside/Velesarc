/**
 * This file is part of ArcX.
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

#include "CoreMinimal.h"
#include "Tasks/TargetingTask.h"
#include "ArcTargetingTask_LineTrace.generated.h"

/**
 * 
 */


UCLASS()
class ARCCORE_API UArcTargetingTask_LineTrace : public UTargetingTask
{
	GENERATED_BODY()

public:
	UArcTargetingTask_LineTrace(const FObjectInitializer& ObjectInitializer);

	/** Evaluation function called by derived classes to process the targeting request */
	virtual void Execute(const FTargetingRequestHandle& TargetingHandle) const override;
};

UCLASS()
class ARCCORE_API UArcTargetingTask_LineTraceFromSocket : public UTargetingTask
{
	GENERATED_BODY()

	/** The trace channel to use */
	UPROPERTY(EditAnywhere, Category = "Target Trace Selection | Collision Data")
	TEnumAsByte<ETraceTypeQuery> TraceChannel;

	/** The trace channel to use */
	UPROPERTY(EditAnywhere, Category = "Target Trace Selection | Collision Data")
	FName SocketName;
	
public:
	UArcTargetingTask_LineTraceFromSocket(const FObjectInitializer& ObjectInitializer);

	/** Evaluation function called by derived classes to process the targeting request */
	virtual void Execute(const FTargetingRequestHandle& TargetingHandle) const override;
};
