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
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcCraft/Recipe/ArcRandomPoolDefinitionData.h"

#include "ArcMaterialPropertyRule.generated.h"

UENUM(BlueprintType)
enum class EArcMaterialRuleMatch : uint8
{
	Union UMETA(Tooltip = "TagQuery matches the union of all slot tags; the share counts every slot holding a required tag."),
	SingleIngredient UMETA(Tooltip = "TagQuery must match one ingredient slot's own tags; MatchedTags and the share count only those slots.")
};

/**
 * Maps a tag pattern to a set of modifiers that all fire when the rule matches
 * and the R gate is cleared.
 *
 * When the per-slot ingredient tags match the TagQuery and AverageR >= MinAverageR,
 * every modifier in Modifiers[] whose MinR is also met fires.
 * Use multiple rules (same TagQuery, different MinAverageR, different Modifiers[]) to
 * express "at R>=1.5 add damage; at R>=2 also add proc chance".
 *
 * Supports full boolean tag expressions via FGameplayTagQuery:
 *   - "Resource.Metal" -- fires if any ingredient slot has metal tags
 *   - "Resource.Metal AND Resource.Gem" -- fires if metal + gem tags exist across slots
 *   - "Resource.Metal.Iron OR Resource.Metal.Steel" -- fires for either
 * With Match = SingleIngredient the whole query must hold on one slot:
 *   - "Material.Property.Magic AND Enchant.Element.Fire" -- fires only when one ingredient carries both
 */
USTRUCT(BlueprintType, meta = (Tooltip = "Maps a tag pattern to a set of modifiers that all fire when the rule's TagQuery matches and the AverageR >= MinAverageR gate is cleared."))
struct ARCCRAFT_API FArcMaterialPropertyRule
{
	GENERATED_BODY()

public:
	/** Display name for editor and debugging. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule")
	FText RuleName;

	/** Tag query matched against the ingredient slot tags as Match decides.
	 *  Supports AND, OR, NOT, and hierarchical tag matching. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule|Matching")
	FGameplayTagQuery TagQuery;

	/** Whether TagQuery matches the union of the slot tags or one slot's own tags. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule|Matching")
	EArcMaterialRuleMatch Match = EArcMaterialRuleMatch::Union;

	/** Minimum average R required for this rule to fire (inclusive).
	 *  R below this threshold means the rule is skipped entirely.
	 *  Default 0 = always fires when TagQuery matches. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule|Matching", meta = (ClampMin = "0.0"))
	float MinAverageR = 0.0f;

	/** Base weight of the rule among the matched rules competing for chairs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule|Weight", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	/** Tag-conditional weight modifiers matched against the combined ingredient tags, as on pool entries. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule|Weight")
	TArray<FArcRandomPoolWeightModifier> WeightModifiers;

	/** How much the average R scales the weight: Weight *= (1 + (R - 1) * RWeightScaling). 0 = R has no effect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule|Weight")
	float RWeightScaling = 0.0f;

	/** Terminal modifiers to apply when this rule fires.
	 *  All modifiers whose MinR is cleared by AverageR fire simultaneously.
	 *  Only Stats, Abilities, Effects, and AttributeModifier subtypes are allowed here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule|Modifiers",
		meta = (BaseStruct = "/Script/ArcCraft.ArcCraftModifier", ExcludeBaseStruct))
	TArray<FInstancedStruct> Modifiers;
};
