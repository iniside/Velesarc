/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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
#include "UObject/Object.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyRule.h"
#include "ArcCraft/Recipe/ArcRandomPoolDefinitionData.h"
#include "ArcCraft/Shared/ArcCraftModifier.h"

#include "ArcCraftRowDetailsProxies.generated.h"

/**
 * Transient UObject proxy used to display an FArcMaterialPropertyRule row
 * inside an IStructureDetailsView detail panel. Never serialized.
 */
UCLASS(Transient, EditInlineNew)
class UArcCraftRuleRowProxy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Row", meta = (ShowOnlyInnerProperties))
	FArcMaterialPropertyRule Data;
};

/**
 * Transient UObject proxy used to display an FArcRandomPoolEntry row
 * inside an IStructureDetailsView detail panel. Never serialized.
 */
UCLASS(Transient, EditInlineNew)
class UArcCraftPoolEntryRowProxy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Row", meta = (ShowOnlyInnerProperties))
	FArcRandomPoolEntry Data;
};

/**
 * Transient UObject proxy used to display a modifier (FInstancedStruct whose
 * inner type derives from FArcCraftModifier) inside an IStructureDetailsView
 * detail panel. Never serialized.
 */
UCLASS(Transient, EditInlineNew)
class UArcCraftModifierRowProxy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Row",
		meta = (BaseStruct = "/Script/ArcCraft.ArcCraftModifier", ExcludeBaseStruct, ShowOnlyInnerProperties))
	FInstancedStruct Data;
};
