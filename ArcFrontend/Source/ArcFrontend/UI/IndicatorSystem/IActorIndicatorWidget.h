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


#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"

#include "IActorIndicatorWidget.generated.h"

class AActor;
class FIndicatorDescriptor;

UINTERFACE()
class UActorIndicatorWidget : public UInterface
{
	GENERATED_BODY()
};

class IActorIndicatorWidget
{
	GENERATED_BODY()

public:
	virtual void BindToEntry(TSharedPtr<FIndicatorDescriptor> CanvasEntry) = 0;
};

UINTERFACE(MinimalAPI, Blueprintable)
class UArcIndicatorLocationInterface : public UInterface
{
	GENERATED_BODY()
};

class IArcIndicatorLocationInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent)
	FVector GetIndicatorLocation() const;
};
