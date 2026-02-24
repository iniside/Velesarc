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
#include "ArcJsonIncludes.h"

struct FInstancedStruct;
struct FArcMaterialQualityBand;
struct FGameplayTagQueryExpression;

/**
 * Shared JSON parsing and serialization utilities for the ArcCraft plugin.
 * Replaces duplicated XML parsing/serialization code that was spread across
 * multiple loader classes (Recipe, MaterialPropertyTable, RandomPool, QualityTierTable).
 *
 * Uses nlohmann::json from the ArcJson module for all JSON operations.
 * All JSON keys use camelCase naming convention.
 */
namespace ArcCraftJsonUtils
{
	// -------------------------------------------------------------------
	// Parsing (JSON -> UE)
	// -------------------------------------------------------------------

	/** Parse a single gameplay tag from a trimmed string. Returns empty tag if not found. */
	ARCCRAFT_API FGameplayTag ParseGameplayTag(const FString& TagString);

	/** Parse a JSON array of tag name strings into a tag container. Returns empty container if input is not an array. */
	ARCCRAFT_API FGameplayTagContainer ParseGameplayTags(const nlohmann::json& TagsArray);

	/**
	 * Parse a JSON object into an instanced output modifier struct.
	 * Reads the "type" field to determine which modifier subtype to instantiate.
	 * Supports: Stats, Abilities, Effects, TransferStats, Random, RandomPool, MaterialProperties.
	 *
	 * @param ModObj       JSON object with "type" discriminator and type-specific fields.
	 * @param OutModifier  Output instanced struct, initialized to the appropriate modifier type.
	 * @return true if parsing succeeded.
	 */
	ARCCRAFT_API bool ParseOutputModifier(const nlohmann::json& ModObj, FInstancedStruct& OutModifier);

	/**
	 * Parse a JSON object representing a gameplay tag query.
	 * The object must have a "type" field (e.g., "AnyTagsMatch", "AllExprMatch").
	 * Tag-set types read a "tags" array; expression-set types read an "expressions" array recursively.
	 *
	 * @param QueryObj  JSON object with "type" and either "tags" or "expressions".
	 * @return Compiled FGameplayTagQuery.
	 */
	ARCCRAFT_API FGameplayTagQuery ParseTagQuery(const nlohmann::json& QueryObj);

	/**
	 * Recursive helper for ParseTagQuery. Parses a single tag query expression from JSON.
	 *
	 * @param ExprObj  JSON object with "type" and either "tags" or "expressions".
	 * @return Parsed expression node.
	 */
	ARCCRAFT_API FGameplayTagQueryExpression ParseTagQueryExpr(const nlohmann::json& ExprObj);

	/**
	 * Parse a JSON object into a material quality band.
	 *
	 * @param BandObj   JSON object with band fields (name, minQuality, baseWeight, etc.).
	 * @param OutBand   Output band struct.
	 * @return true if parsing succeeded.
	 */
	ARCCRAFT_API bool ParseQualityBand(const nlohmann::json& BandObj, FArcMaterialQualityBand& OutBand);

	// -------------------------------------------------------------------
	// Serialization (UE -> JSON)
	// -------------------------------------------------------------------

	/** Serialize a gameplay tag container to a JSON array of tag name strings. */
	ARCCRAFT_API nlohmann::json SerializeTagContainer(const FGameplayTagContainer& Tags);

	/** Alias for SerializeTagContainer — serializes a tag container to a JSON array. */
	ARCCRAFT_API nlohmann::json SerializeGameplayTags(const FGameplayTagContainer& Tags);

	/**
	 * Serialize an instanced output modifier struct to a JSON object.
	 * Writes a "type" discriminator field plus all type-specific fields.
	 *
	 * @param Modifier  Instanced struct containing a FArcRecipeOutputModifier subtype.
	 * @return JSON object, or null JSON if the modifier type is unrecognized.
	 */
	ARCCRAFT_API nlohmann::json SerializeOutputModifier(const FInstancedStruct& Modifier);

	/**
	 * Serialize a gameplay tag query to a JSON object.
	 * Decomposes the query via GetQueryExpr() and serializes recursively.
	 *
	 * @param Query  Compiled tag query.
	 * @return JSON object with "type" and either "tags" or "expressions".
	 */
	ARCCRAFT_API nlohmann::json SerializeTagQuery(const FGameplayTagQuery& Query);

	/**
	 * Serialize a single tag query expression node to JSON.
	 * If the expression uses a tag set, writes a "tags" array.
	 * If it uses an expression set, writes an "expressions" array with recursive calls.
	 *
	 * @param Expr  Expression node to serialize.
	 * @return JSON object.
	 */
	ARCCRAFT_API nlohmann::json SerializeTagQueryExpr(const FGameplayTagQueryExpression& Expr);

	/**
	 * Serialize a material quality band to a JSON object.
	 *
	 * @param Band  Quality band to serialize.
	 * @return JSON object with band fields and nested modifiers array.
	 */
	ARCCRAFT_API nlohmann::json SerializeQualityBand(const FArcMaterialQualityBand& Band);
}
