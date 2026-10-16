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

#include "ArcCraftSlotSelector.generated.h"

struct FRandomStream;

/** One candidate the selector orders: a modifier group for bonus chairs, a penalty result for penalty chairs. */
struct FArcCraftChairCandidate
{
	/** The resolver's index of the candidate. */
	int32 Index = INDEX_NONE;

	/** Source index of the group in Recipe.ModifierSources. */
	int32 ModifierIndex = INDEX_NONE;

	/** Group within the source. */
	int32 GroupIndex = INDEX_NONE;

	/** The group weight. */
	float Weight = 0.0f;

	/** The group's ingredient share. */
	float IngredientShare = 0.0f;

	/** The group's required ingredient tags that the consumed ingredients hold. */
	FGameplayTagContainer MatchedTags;
};

/**
 * Base struct for the strategy that orders candidates for the recipe's chairs, best first.
 * Candidates arrive in the stable order (ModifierIndex, GroupIndex, Index), and ties keep it.
 */
USTRUCT(BlueprintType, meta = (Tooltip = "Base struct for the strategy that orders slot candidates for the recipe's chairs, best first."))
struct ARCCRAFT_API FArcCraftSlotSelector
{
	GENERATED_BODY()

public:
	virtual ~FArcCraftSlotSelector() = default;

	virtual void Order(TArray<FArcCraftChairCandidate>& Candidates, const FRandomStream& Stream) const;
};

/** Highest group weight first. */
USTRUCT(BlueprintType, meta = (DisplayName = "Highest Weight", Tooltip = "Orders candidates by group weight, highest first."))
struct ARCCRAFT_API FArcCraftSlotSelector_HighestWeight : public FArcCraftSlotSelector
{
	GENERATED_BODY()

public:
	virtual void Order(TArray<FArcCraftChairCandidate>& Candidates, const FRandomStream& Stream) const override;
};

/** Weighted draw without replacement: one stream draw per pick while a positive weight remains;
 *  zero-weight candidates follow in the stable order. */
USTRUCT(BlueprintType, meta = (DisplayName = "Weighted Random", Tooltip = "Orders candidates by a weighted draw without replacement from the craft stream."))
struct ARCCRAFT_API FArcCraftSlotSelector_WeightedRandom : public FArcCraftSlotSelector
{
	GENERATED_BODY()

public:
	virtual void Order(TArray<FArcCraftChairCandidate>& Candidates, const FRandomStream& Stream) const override;
};

/** Highest ingredient share first. */
USTRUCT(BlueprintType, meta = (DisplayName = "Ingredient Share", Tooltip = "Orders candidates by ingredient share, highest first."))
struct ARCCRAFT_API FArcCraftSlotSelector_IngredientShare : public FArcCraftSlotSelector
{
	GENERATED_BODY()

public:
	virtual void Order(TArray<FArcCraftChairCandidate>& Candidates, const FRandomStream& Stream) const override;
};

/** A candidate ranks by the first Tags entry found in its MatchedTags; candidates matching none come last. */
USTRUCT(BlueprintType, meta = (DisplayName = "Tag Priority", Tooltip = "Orders candidates by the first listed tag found in their matched ingredient tags."))
struct ARCCRAFT_API FArcCraftSlotSelector_TagPriority : public FArcCraftSlotSelector
{
	GENERATED_BODY()

public:
	/** Ordered tags, highest priority first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection")
	TArray<FGameplayTag> Tags;

	virtual void Order(TArray<FArcCraftChairCandidate>& Candidates, const FRandomStream& Stream) const override;
};
