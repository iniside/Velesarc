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

#include "CoreMinimal.h"
#include "ArcMass/Config/ArcMassEntityTraitBase.h"
#include "Items/Loot/ArcItemGrantFragments.h"
#include "ArcItemGrantTrait.generated.h"

/** Emits FArcItemGrantFragment as a sparse fragment so item-grant source entities
 *  carry their authored Entries from the moment they are committed by UArcMassSpawnerSubsystem. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Item Grant", Category = "Items"))
class ARCMASSITEMSRUNTIME_API UArcItemGrantTrait : public UArcMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Grant")
	TArray<FArcItemGrantEntry> Entries;

	virtual void BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const override;
};
