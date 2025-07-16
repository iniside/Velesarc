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
#include "UObject/Interface.h"

#include "ArcCameraAssistInterface.generated.h"

/** */
UINTERFACE(BlueprintType)
class UArcCameraAssistInterface : public UInterface
{
	GENERATED_BODY()
};

class IArcCameraAssistInterface
{
	GENERATED_BODY()

public:
	/**
	 * Get the list of actors that we're allowing the camera to penetrate. Useful in 3rd
	 * person cameras when you need the following camera to ignore things like the a
	 * collection of view targets, the pawn, a vehicle..etc.
	 */
	virtual void GetIgnoredActorsForCameraPentration(TArray<const AActor*>& OutActorsAllowPenetration) const
	{
	}

	/**
	 * The target actor to prevent penetration on.  Normally, this is almost always the
	 * view target, which if unimplemented will remain true.  However, sometimes the view
	 * target, isn't the same as the root actor you need to keep in frame.
	 */
	virtual TOptional<AActor*> GetCameraPreventPenetrationTarget() const
	{
		return TOptional<AActor*>();
	}

	/** Called if the camera penetrates the focal target.  Useful if you want to hide the
	 * target actor when being overlapped. */
	virtual void OnCameraPenetratingTarget()
	{
	}
};
