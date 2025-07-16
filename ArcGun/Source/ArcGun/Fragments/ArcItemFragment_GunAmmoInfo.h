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
#include "Items/Fragments/ArcItemFragment.h"

#include "ArcItemFragment_GunAmmoInfo.generated.h"

class UArcItemDefinition;

USTRUCT(BlueprintType, meta = (DisplayName = "Gun Ammo Info"))
struct ARCGUN_API FArcItemFragment_GunAmmoInfo : public FArcItemFragment
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gameplay")
	TObjectPtr<UArcItemDefinition> AmmoItem;

	FArcItemFragment_GunAmmoInfo()
		: AmmoItem(nullptr)
	{}

	virtual ~FArcItemFragment_GunAmmoInfo() override = default;
};