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

#include "MassEntityTypes.h"
#include "ArcNeedFragment.generated.h"

UENUM(BlueprintType)
enum class EArcNeedOperation : uint8
{
	Add,
	Subtract,
	Override
};

UENUM(BlueprintType)
enum class EArcNeedFillType : uint8
{
	Fixed,
	FixedTicks,
	UntilValue
};

USTRUCT(BlueprintType)
struct ARCNEEDS_API FArcNeedFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 NeedType = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Resistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ChangeRate = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CurrentValue = 0.f;
};

USTRUCT(BlueprintType)
struct ARCNEEDS_API FArcHungerNeedFragment : public FArcNeedFragment
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCNEEDS_API FArcThirstNeedFragment : public FArcNeedFragment
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCNEEDS_API FArcFatigueNeedFragment : public FArcNeedFragment
{
	GENERATED_BODY()
};
