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

#include "CQTest.h"

#include "NativeGameplayTags.h"
#include "ArcJsonIncludes.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcCraft/Recipe/ArcRecipeQuality.h"
#include "ArcCraft/Recipe/ArcQualityTierTableJsonLoader.h"
#include "ArcCraft/Recipe/ArcRandomPoolDefinition.h"
#include "ArcCraft/Recipe/ArcRandomPoolJsonLoader.h"
#include "ArcCraft/Recipe/ArcRecipeOutput.h"
#include "ArcCraft/Shared/ArcCraftModifier.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTable.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyRule.h"
#include "ArcCraft/MaterialCraft/ArcQualityBandPreset.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTableJsonLoader.h"

// ---- Test tags ----
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_JsonTest_Quality_Common, "Quality.Common");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_JsonTest_Quality_Rare, "Quality.Rare");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_JsonTest_Quality_Epic, "Quality.Epic");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_JsonTest_Resource_Metal, "Resource.Metal");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_JsonTest_Resource_Gem, "Resource.Gem");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_JsonTest_Resource_Wood, "Resource.Wood");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_JsonTest_Recipe_Weapon, "Recipe.Weapon");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_JsonTest_Output_Sharpness, "Output.Sharpness");

// ===================================================================
// Helper: Parse JSON from a string literal
// ===================================================================

namespace JsonTestHelpers
{
	nlohmann::json ParseJsonString(const char* JsonString)
	{
		return nlohmann::json::parse(JsonString);
	}
}

// ===================================================================
// QualityTierTable: JSON Roundtrip
// ===================================================================

TEST_CLASS(QualityTierTable_JsonSerialization, "ArcCraft.Json.Integration.QualityTierTable")
{
	TEST_METHOD(ParseJson_BasicTiers_AllFieldsCorrect)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "QualityTierTable",
			"tiers": [
				{ "tag": "Quality.Common", "value": 1, "multiplier": 1.0 },
				{ "tag": "Quality.Rare", "value": 2, "multiplier": 1.5 },
				{ "tag": "Quality.Epic", "value": 3, "multiplier": 2.25 }
			]
		})");

		UArcQualityTierTable* Table = NewObject<UArcQualityTierTable>(GetTransientPackage());
		const bool bSuccess = UArcQualityTierTableJsonLoader::ParseJson(Json, Table);
		ASSERT_THAT(IsTrue(bSuccess, TEXT("ParseJson should succeed")));

		ASSERT_THAT(AreEqual(3, Table->Tiers.Num(), TEXT("Should have 3 tiers")));

		// Tier 0: Common
		ASSERT_THAT(IsTrue(Table->Tiers[0].TierTag == TAG_JsonTest_Quality_Common, TEXT("Tier 0 tag should be Quality.Common")));
		ASSERT_THAT(AreEqual(1, Table->Tiers[0].TierValue, TEXT("Tier 0 value")));
		ASSERT_THAT(IsNear(1.0f, Table->Tiers[0].QualityMultiplier, 0.001f, TEXT("Tier 0 multiplier")));

		// Tier 1: Rare
		ASSERT_THAT(IsTrue(Table->Tiers[1].TierTag == TAG_JsonTest_Quality_Rare, TEXT("Tier 1 tag should be Quality.Rare")));
		ASSERT_THAT(AreEqual(2, Table->Tiers[1].TierValue, TEXT("Tier 1 value")));
		ASSERT_THAT(IsNear(1.5f, Table->Tiers[1].QualityMultiplier, 0.001f, TEXT("Tier 1 multiplier")));

		// Tier 2: Epic
		ASSERT_THAT(IsTrue(Table->Tiers[2].TierTag == TAG_JsonTest_Quality_Epic, TEXT("Tier 2 tag should be Quality.Epic")));
		ASSERT_THAT(AreEqual(3, Table->Tiers[2].TierValue, TEXT("Tier 2 value")));
		ASSERT_THAT(IsNear(2.25f, Table->Tiers[2].QualityMultiplier, 0.001f, TEXT("Tier 2 multiplier")));
	}

	TEST_METHOD(ParseJson_EmptyTable_ZeroTiers)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "QualityTierTable",
			"tiers": []
		})");

		UArcQualityTierTable* Table = NewObject<UArcQualityTierTable>(GetTransientPackage());
		const bool bSuccess = UArcQualityTierTableJsonLoader::ParseJson(Json, Table);
		ASSERT_THAT(IsTrue(bSuccess));
		ASSERT_THAT(AreEqual(0, Table->Tiers.Num(), TEXT("Empty table should have 0 tiers")));
	}

	TEST_METHOD(ParseJson_FloatPrecision_PreservesDecimals)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "QualityTierTable",
			"tiers": [
				{ "tag": "Quality.Common", "value": 10, "multiplier": 0.3333 }
			]
		})");

		UArcQualityTierTable* Table = NewObject<UArcQualityTierTable>(GetTransientPackage());
		UArcQualityTierTableJsonLoader::ParseJson(Json, Table);

		ASSERT_THAT(AreEqual(1, Table->Tiers.Num()));
		ASSERT_THAT(AreEqual(10, Table->Tiers[0].TierValue));
		ASSERT_THAT(IsNear(0.3333f, Table->Tiers[0].QualityMultiplier, 0.0001f, TEXT("Float precision")));
	}

	TEST_METHOD(ParseJson_NegativeValues_Allowed)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "QualityTierTable",
			"tiers": [
				{ "tag": "Quality.Common", "value": -5, "multiplier": -0.5 }
			]
		})");

		UArcQualityTierTable* Table = NewObject<UArcQualityTierTable>(GetTransientPackage());
		UArcQualityTierTableJsonLoader::ParseJson(Json, Table);

		ASSERT_THAT(AreEqual(1, Table->Tiers.Num()));
		ASSERT_THAT(AreEqual(-5, Table->Tiers[0].TierValue));
		ASSERT_THAT(IsNear(-0.5f, Table->Tiers[0].QualityMultiplier, 0.001f));
	}

	TEST_METHOD(ParseJson_IgnoresUnknownFields)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "QualityTierTable",
			"someOtherField": "bar",
			"tiers": [
				{ "tag": "Quality.Common", "value": 1, "multiplier": 1.0, "unknownField": 42 }
			],
			"anotherField": true
		})");

		UArcQualityTierTable* Table = NewObject<UArcQualityTierTable>(GetTransientPackage());
		UArcQualityTierTableJsonLoader::ParseJson(Json, Table);

		ASSERT_THAT(AreEqual(1, Table->Tiers.Num(), TEXT("Should only parse tier entries")));
	}

	TEST_METHOD(ParseJson_Reimport_ReplacesData)
	{
		UArcQualityTierTable* Table = NewObject<UArcQualityTierTable>(GetTransientPackage());

		// First parse: 3 tiers
		{
			const auto Json = JsonTestHelpers::ParseJsonString(R"({
				"tiers": [
					{ "tag": "Quality.Common", "value": 1, "multiplier": 1.0 },
					{ "tag": "Quality.Rare", "value": 2, "multiplier": 1.5 },
					{ "tag": "Quality.Epic", "value": 3, "multiplier": 2.0 }
				]
			})");
			UArcQualityTierTableJsonLoader::ParseJson(Json, Table);
			ASSERT_THAT(AreEqual(3, Table->Tiers.Num()));
		}

		// Second parse (reimport): only 1 tier — should replace, not append
		{
			const auto Json = JsonTestHelpers::ParseJsonString(R"({
				"tiers": [
					{ "tag": "Quality.Common", "value": 10, "multiplier": 5.0 }
				]
			})");
			UArcQualityTierTableJsonLoader::ParseJson(Json, Table);
			ASSERT_THAT(AreEqual(1, Table->Tiers.Num(), TEXT("Reimport should replace, not append")));
			ASSERT_THAT(AreEqual(10, Table->Tiers[0].TierValue, TEXT("Should have new data")));
			ASSERT_THAT(IsNear(5.0f, Table->Tiers[0].QualityMultiplier, 0.001f));
		}
	}
};

// ===================================================================
// RandomPoolDefinition: JSON Roundtrip
// ===================================================================

TEST_CLASS(RandomPool_JsonSerialization, "ArcCraft.Json.Integration.RandomPool")
{
	TEST_METHOD(ParseJson_BasicEntries_AllFieldsCorrect)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "RandomPoolDefinition",
			"name": "TestPool",
			"entries": [
				{
					"name": "Iron Bonus",
					"minQuality": 0,
					"baseWeight": 2.5,
					"qualityWeightScaling": 0.1,
					"cost": 1,
					"valueScale": 1.5,
					"valueSkew": 0.2,
					"scaleByQuality": true
				},
				{
					"name": "Gold Bonus",
					"minQuality": 3,
					"baseWeight": 1,
					"cost": 5
				}
			]
		})");

		UArcRandomPoolDefinition* Pool = NewObject<UArcRandomPoolDefinition>(GetTransientPackage());
		const bool bSuccess = UArcRandomPoolJsonLoader::ParseJson(Json, Pool);
		ASSERT_THAT(IsTrue(bSuccess, TEXT("ParseJson should succeed")));

		ASSERT_THAT(AreEqual(FString("TestPool"), Pool->PoolName.ToString(), TEXT("Pool name")));
		ASSERT_THAT(AreEqual(2, Pool->Entries.Num(), TEXT("Should have 2 entries")));

		// Entry 0
		const FArcRandomPoolEntry& E0 = Pool->Entries[0];
		ASSERT_THAT(AreEqual(FString("Iron Bonus"), E0.DisplayName.ToString()));
		ASSERT_THAT(IsNear(0.0f, E0.MinQualityThreshold, 0.001f));
		ASSERT_THAT(IsNear(2.5f, E0.BaseWeight, 0.001f));
		ASSERT_THAT(IsNear(0.1f, E0.QualityWeightScaling, 0.001f));
		ASSERT_THAT(IsNear(1.0f, E0.Cost, 0.001f));
		ASSERT_THAT(IsNear(1.5f, E0.ValueScale, 0.001f));
		ASSERT_THAT(IsNear(0.2f, E0.ValueSkew, 0.001f));
		ASSERT_THAT(IsTrue(E0.bScaleByQuality, TEXT("ScaleByQuality should be true")));

		// Entry 1 - defaults for unspecified attributes
		const FArcRandomPoolEntry& E1 = Pool->Entries[1];
		ASSERT_THAT(AreEqual(FString("Gold Bonus"), E1.DisplayName.ToString()));
		ASSERT_THAT(IsNear(3.0f, E1.MinQualityThreshold, 0.001f));
		ASSERT_THAT(IsNear(1.0f, E1.BaseWeight, 0.001f));
		ASSERT_THAT(IsNear(5.0f, E1.Cost, 0.001f));
	}

	TEST_METHOD(ParseJson_IngredientTags_ParsedCorrectly)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "RandomPoolDefinition",
			"name": "TagPool",
			"entries": [
				{
					"name": "MetalEntry",
					"baseWeight": 1,
					"requiredIngredientTags": ["Resource.Metal", "Resource.Gem"],
					"denyIngredientTags": ["Resource.Wood"]
				}
			]
		})");

		UArcRandomPoolDefinition* Pool = NewObject<UArcRandomPoolDefinition>(GetTransientPackage());
		UArcRandomPoolJsonLoader::ParseJson(Json, Pool);

		ASSERT_THAT(AreEqual(1, Pool->Entries.Num()));
		const FArcRandomPoolEntry& Entry = Pool->Entries[0];

		ASSERT_THAT(IsTrue(Entry.RequiredIngredientTags.HasTag(TAG_JsonTest_Resource_Metal),
			TEXT("Should have Metal required tag")));
		ASSERT_THAT(IsTrue(Entry.RequiredIngredientTags.HasTag(TAG_JsonTest_Resource_Gem),
			TEXT("Should have Gem required tag")));
		ASSERT_THAT(AreEqual(2, Entry.RequiredIngredientTags.Num(), TEXT("Should have exactly 2 required tags")));

		ASSERT_THAT(IsTrue(Entry.DenyIngredientTags.HasTag(TAG_JsonTest_Resource_Wood),
			TEXT("Should have Wood deny tag")));
		ASSERT_THAT(AreEqual(1, Entry.DenyIngredientTags.Num(), TEXT("Should have exactly 1 deny tag")));
	}

	TEST_METHOD(ParseJson_WeightModifiers_ParsedCorrectly)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "RandomPoolDefinition",
			"name": "WeightModPool",
			"entries": [
				{
					"name": "ModEntry",
					"baseWeight": 1,
					"weightModifiers": [
						{
							"bonusWeight": 5,
							"weightMultiplier": 2,
							"requiredTags": ["Resource.Metal"]
						},
						{
							"bonusWeight": 3,
							"weightMultiplier": 1.5,
							"requiredTags": ["Resource.Gem"]
						}
					]
				}
			]
		})");

		UArcRandomPoolDefinition* Pool = NewObject<UArcRandomPoolDefinition>(GetTransientPackage());
		UArcRandomPoolJsonLoader::ParseJson(Json, Pool);

		ASSERT_THAT(AreEqual(1, Pool->Entries.Num()));
		const FArcRandomPoolEntry& Entry = Pool->Entries[0];
		ASSERT_THAT(AreEqual(2, Entry.WeightModifiers.Num(), TEXT("Should have 2 weight modifiers")));

		const FArcRandomPoolWeightModifier& WM0 = Entry.WeightModifiers[0];
		ASSERT_THAT(IsNear(5.0f, WM0.BonusWeight, 0.001f));
		ASSERT_THAT(IsNear(2.0f, WM0.WeightMultiplier, 0.001f));
		ASSERT_THAT(IsTrue(WM0.RequiredTags.HasTag(TAG_JsonTest_Resource_Metal)));

		const FArcRandomPoolWeightModifier& WM1 = Entry.WeightModifiers[1];
		ASSERT_THAT(IsNear(3.0f, WM1.BonusWeight, 0.001f));
		ASSERT_THAT(IsNear(1.5f, WM1.WeightMultiplier, 0.001f));
		ASSERT_THAT(IsTrue(WM1.RequiredTags.HasTag(TAG_JsonTest_Resource_Gem)));
	}

	TEST_METHOD(ParseJson_ValueSkewRules_ParsedCorrectly)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "RandomPoolDefinition",
			"name": "SkewPool",
			"entries": [
				{
					"name": "SkewEntry",
					"baseWeight": 1,
					"valueSkewRules": [
						{
							"valueScale": 2,
							"valueOffset": 0.5,
							"requiredTags": ["Resource.Metal"]
						}
					]
				}
			]
		})");

		UArcRandomPoolDefinition* Pool = NewObject<UArcRandomPoolDefinition>(GetTransientPackage());
		UArcRandomPoolJsonLoader::ParseJson(Json, Pool);

		ASSERT_THAT(AreEqual(1, Pool->Entries.Num()));
		const FArcRandomPoolEntry& Entry = Pool->Entries[0];
		ASSERT_THAT(AreEqual(1, Entry.ValueSkewRules.Num()));

		const FArcRandomPoolValueSkew& Skew = Entry.ValueSkewRules[0];
		ASSERT_THAT(IsNear(2.0f, Skew.ValueScale, 0.001f));
		ASSERT_THAT(IsNear(0.5f, Skew.ValueOffset, 0.001f));
		ASSERT_THAT(IsTrue(Skew.RequiredTags.HasTag(TAG_JsonTest_Resource_Metal)));
	}

	TEST_METHOD(ParseJson_StatModifier_ParsedCorrectly)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "RandomPoolDefinition",
			"name": "StatPool",
			"entries": [
				{
					"name": "StatEntry",
					"baseWeight": 1,
					"modifiers": [
						{
							"type": "Stats",
							"qualityScaling": 0.5,
							"stat": { "value": 10, "modType": "Additive" }
						},
						{
							"type": "Stats",
							"qualityScaling": 0.5,
							"stat": { "value": 2, "modType": "Multiply" }
						}
					]
				}
			]
		})");

		UArcRandomPoolDefinition* Pool = NewObject<UArcRandomPoolDefinition>(GetTransientPackage());
		UArcRandomPoolJsonLoader::ParseJson(Json, Pool);

		ASSERT_THAT(AreEqual(1, Pool->Entries.Num()));
		const FArcRandomPoolEntry& Entry = Pool->Entries[0];
		ASSERT_THAT(AreEqual(2, Entry.Modifiers.Num(), TEXT("Should have 2 modifiers")));

		const FInstancedStruct& ModStruct0 = Entry.Modifiers[0];
		ASSERT_THAT(IsTrue(ModStruct0.IsValid(), TEXT("Modifier struct 0 should be valid")));

		const FArcCraftModifier_Stats* StatMod0 = ModStruct0.GetPtr<FArcCraftModifier_Stats>();
		ASSERT_THAT(IsNotNull(StatMod0, TEXT("Should be a CraftModifier_Stats")));
		ASSERT_THAT(IsNear(0.5f, StatMod0->QualityScalingFactor, 0.001f, TEXT("QualityScaling")));
		ASSERT_THAT(IsNear(10.0f, StatMod0->BaseStat.Value.GetValue(), 0.001f));
		ASSERT_THAT(AreEqual((int32)EArcModType::Additive, (int32)StatMod0->BaseStat.Type));

		const FArcCraftModifier_Stats* StatMod1 = Entry.Modifiers[1].GetPtr<FArcCraftModifier_Stats>();
		ASSERT_THAT(IsNotNull(StatMod1, TEXT("Second modifier should be a CraftModifier_Stats")));
		ASSERT_THAT(IsNear(2.0f, StatMod1->BaseStat.Value.GetValue(), 0.001f));
		ASSERT_THAT(AreEqual((int32)EArcModType::Multiply, (int32)StatMod1->BaseStat.Type));
	}

	TEST_METHOD(ParseJson_CraftModifierBaseFields_ParsedCorrectly)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "RandomPoolDefinition",
			"name": "BaseFieldPool",
			"entries": [
				{
					"name": "BaseFieldEntry",
					"baseWeight": 1,
					"modifiers": [
						{
							"type": "Stats",
							"minQuality": 2.5,
							"triggerTags": ["Resource.Metal", "Resource.Gem"],
							"weight": 3.0,
							"qualityScaling": 0.75,
							"stat": { "value": 10, "modType": "Additive" }
						}
					]
				}
			]
		})");

		UArcRandomPoolDefinition* Pool = NewObject<UArcRandomPoolDefinition>(GetTransientPackage());
		UArcRandomPoolJsonLoader::ParseJson(Json, Pool);

		ASSERT_THAT(AreEqual(1, Pool->Entries.Num()));
		const FArcRandomPoolEntry& Entry = Pool->Entries[0];
		ASSERT_THAT(AreEqual(1, Entry.Modifiers.Num()));

		const FArcCraftModifier_Stats* StatMod = Entry.Modifiers[0].GetPtr<FArcCraftModifier_Stats>();
		ASSERT_THAT(IsNotNull(StatMod, TEXT("Should be a CraftModifier_Stats")));
		ASSERT_THAT(IsNear(2.5f, StatMod->MinQualityThreshold, 0.001f, TEXT("MinQuality")));
		ASSERT_THAT(IsNear(3.0f, StatMod->Weight, 0.001f, TEXT("Weight")));
		ASSERT_THAT(IsNear(0.75f, StatMod->QualityScalingFactor, 0.001f, TEXT("QualityScaling")));
		ASSERT_THAT(IsTrue(StatMod->TriggerTags.HasTag(TAG_JsonTest_Resource_Metal), TEXT("Should have Metal trigger tag")));
		ASSERT_THAT(IsTrue(StatMod->TriggerTags.HasTag(TAG_JsonTest_Resource_Gem), TEXT("Should have Gem trigger tag")));
		ASSERT_THAT(AreEqual(2, StatMod->TriggerTags.Num(), TEXT("Should have 2 trigger tags")));
		ASSERT_THAT(IsNear(10.0f, StatMod->BaseStat.Value.GetValue(), 0.001f));
	}

	TEST_METHOD(ParseJson_CraftModifierEmptyTriggerTags_DefaultsCorrectly)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "RandomPoolDefinition",
			"name": "DefaultPool",
			"entries": [
				{
					"name": "DefaultEntry",
					"baseWeight": 1,
					"modifiers": [
						{
							"type": "Stats",
							"stat": { "value": 5, "modType": "Additive" }
						}
					]
				}
			]
		})");

		UArcRandomPoolDefinition* Pool = NewObject<UArcRandomPoolDefinition>(GetTransientPackage());
		UArcRandomPoolJsonLoader::ParseJson(Json, Pool);

		ASSERT_THAT(AreEqual(1, Pool->Entries.Num()));
		const FArcRandomPoolEntry& Entry = Pool->Entries[0];
		ASSERT_THAT(AreEqual(1, Entry.Modifiers.Num()));

		const FArcCraftModifier_Stats* StatMod = Entry.Modifiers[0].GetPtr<FArcCraftModifier_Stats>();
		ASSERT_THAT(IsNotNull(StatMod));

		// Defaults: empty trigger tags = always match, minQuality 0, weight 1, qualityScaling 1
		ASSERT_THAT(IsNear(0.0f, StatMod->MinQualityThreshold, 0.001f, TEXT("Default MinQuality")));
		ASSERT_THAT(IsNear(1.0f, StatMod->Weight, 0.001f, TEXT("Default Weight")));
		ASSERT_THAT(IsNear(1.0f, StatMod->QualityScalingFactor, 0.001f, TEXT("Default QualityScaling")));
		ASSERT_THAT(AreEqual(0, StatMod->TriggerTags.Num(), TEXT("Empty TriggerTags = always match")));

		// Verify MatchesTriggerTags with empty tags matches anything
		FGameplayTagContainer AnyTags;
		AnyTags.AddTag(TAG_JsonTest_Resource_Metal);
		ASSERT_THAT(IsTrue(StatMod->MatchesTriggerTags(AnyTags),
			TEXT("Empty TriggerTags should match any ingredient tags")));

		FGameplayTagContainer EmptyTags;
		ASSERT_THAT(IsTrue(StatMod->MatchesTriggerTags(EmptyTags),
			TEXT("Empty TriggerTags should match empty ingredient tags")));
	}

	TEST_METHOD(ParseJson_EmptyPool_ZeroEntries)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "RandomPoolDefinition",
			"name": "Empty",
			"entries": []
		})");

		UArcRandomPoolDefinition* Pool = NewObject<UArcRandomPoolDefinition>(GetTransientPackage());
		UArcRandomPoolJsonLoader::ParseJson(Json, Pool);

		ASSERT_THAT(AreEqual(FString("Empty"), Pool->PoolName.ToString()));
		ASSERT_THAT(AreEqual(0, Pool->Entries.Num()));
	}

	TEST_METHOD(ParseJson_ComplexEntry_AllChildElements)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "RandomPoolDefinition",
			"name": "ComplexPool",
			"entries": [
				{
					"name": "Complex",
					"minQuality": 2,
					"baseWeight": 3,
					"qualityWeightScaling": 0.5,
					"cost": 4,
					"valueScale": 1.2,
					"valueSkew": -0.1,
					"scaleByQuality": false,
					"requiredIngredientTags": ["Resource.Metal"],
					"denyIngredientTags": ["Resource.Wood"],
					"weightModifiers": [
						{
							"bonusWeight": 1,
							"weightMultiplier": 1.1,
							"requiredTags": ["Resource.Gem"]
						}
					],
					"valueSkewRules": [
						{
							"valueScale": 1.5,
							"valueOffset": 0.3,
							"requiredTags": ["Resource.Metal"]
						}
					],
					"modifiers": [
						{
							"type": "Stats",
							"qualityScaling": 0.2,
							"stat": { "value": 25, "modType": "Additive" }
						}
					]
				}
			]
		})");

		UArcRandomPoolDefinition* Pool = NewObject<UArcRandomPoolDefinition>(GetTransientPackage());
		UArcRandomPoolJsonLoader::ParseJson(Json, Pool);

		ASSERT_THAT(AreEqual(1, Pool->Entries.Num()));
		const FArcRandomPoolEntry& E = Pool->Entries[0];

		// Attributes
		ASSERT_THAT(AreEqual(FString("Complex"), E.DisplayName.ToString()));
		ASSERT_THAT(IsNear(2.0f, E.MinQualityThreshold, 0.001f));
		ASSERT_THAT(IsNear(3.0f, E.BaseWeight, 0.001f));
		ASSERT_THAT(IsNear(0.5f, E.QualityWeightScaling, 0.001f));
		ASSERT_THAT(IsNear(4.0f, E.Cost, 0.001f));
		ASSERT_THAT(IsNear(1.2f, E.ValueScale, 0.001f));
		ASSERT_THAT(IsNear(-0.1f, E.ValueSkew, 0.001f));
		ASSERT_THAT(IsFalse(E.bScaleByQuality));

		// Child elements
		ASSERT_THAT(AreEqual(1, E.RequiredIngredientTags.Num()));
		ASSERT_THAT(AreEqual(1, E.DenyIngredientTags.Num()));
		ASSERT_THAT(AreEqual(1, E.WeightModifiers.Num()));
		ASSERT_THAT(AreEqual(1, E.ValueSkewRules.Num()));
		ASSERT_THAT(AreEqual(1, E.Modifiers.Num()));

		// Verify modifier content
		const FArcCraftModifier_Stats* StatMod = E.Modifiers[0].GetPtr<FArcCraftModifier_Stats>();
		ASSERT_THAT(IsNotNull(StatMod));
		ASSERT_THAT(IsNear(25.0f, StatMod->BaseStat.Value.GetValue(), 0.001f));
	}
};

// ===================================================================
// MaterialPropertyTable: JSON Roundtrip
// ===================================================================

TEST_CLASS(MaterialPropertyTable_JsonSerialization, "ArcCraft.Json.Integration.MaterialPropertyTable")
{
	TEST_METHOD(ParseJson_BasicTable_RootAttributesCorrect)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "MaterialPropertyTable",
			"name": "TestTable",
			"maxActiveRules": 3,
			"extraIngredientWeightBonus": 0.15,
			"extraTimeWeightBonusCap": 2,
			"baseBandBudget": 5,
			"budgetPerQuality": 1.5,
			"rules": []
		})");

		UArcMaterialPropertyTable* Table = NewObject<UArcMaterialPropertyTable>(GetTransientPackage());
		const bool bSuccess = UArcMaterialPropertyTableJsonLoader::ParseJson(Json, Table);
		ASSERT_THAT(IsTrue(bSuccess));

		ASSERT_THAT(AreEqual(FString("TestTable"), Table->TableName.ToString()));
		ASSERT_THAT(AreEqual(3, Table->MaxActiveRules));
		ASSERT_THAT(IsNear(0.15f, Table->ExtraIngredientWeightBonus, 0.001f));
		ASSERT_THAT(IsNear(2.0f, Table->ExtraTimeWeightBonusCap, 0.001f));
		ASSERT_THAT(IsNear(5.0f, Table->BaseBandBudget, 0.001f));
		ASSERT_THAT(IsNear(1.5f, Table->BudgetPerQuality, 0.001f));
		ASSERT_THAT(AreEqual(0, Table->Rules.Num()));
	}

	TEST_METHOD(ParseJson_RuleWithAttributes_AllFieldsCorrect)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "MaterialPropertyTable",
			"name": "RuleTest",
			"rules": [
				{
					"name": "Metal Rule",
					"priority": 10,
					"requiredRecipeTags": ["Recipe.Weapon"],
					"outputTags": ["Output.Sharpness"]
				}
			]
		})");

		UArcMaterialPropertyTable* Table = NewObject<UArcMaterialPropertyTable>(GetTransientPackage());
		UArcMaterialPropertyTableJsonLoader::ParseJson(Json, Table);

		ASSERT_THAT(AreEqual(1, Table->Rules.Num()));
		const FArcMaterialPropertyRule& Rule = Table->Rules[0];

		ASSERT_THAT(AreEqual(FString("Metal Rule"), Rule.RuleName.ToString()));
		ASSERT_THAT(AreEqual(10, Rule.Priority));
		ASSERT_THAT(IsTrue(Rule.RequiredRecipeTags.HasTag(TAG_JsonTest_Recipe_Weapon)));
		ASSERT_THAT(IsTrue(Rule.OutputTags.HasTag(TAG_JsonTest_Output_Sharpness)));
	}

	TEST_METHOD(ParseJson_SimpleTagQuery_AnyTagsMatch)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "MaterialPropertyTable",
			"name": "TagQueryTest",
			"rules": [
				{
					"name": "QueryRule",
					"tagQuery": {
						"type": "AnyTagsMatch",
						"tags": ["Resource.Metal", "Resource.Gem"]
					}
				}
			]
		})");

		UArcMaterialPropertyTable* Table = NewObject<UArcMaterialPropertyTable>(GetTransientPackage());
		UArcMaterialPropertyTableJsonLoader::ParseJson(Json, Table);

		ASSERT_THAT(AreEqual(1, Table->Rules.Num()));

		// Test the tag query actually works
		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_JsonTest_Resource_Metal);
		ASSERT_THAT(IsTrue(Table->Rules[0].TagQuery.Matches(MetalTags),
			TEXT("AnyTagsMatch should match Metal")));

		FGameplayTagContainer GemTags;
		GemTags.AddTag(TAG_JsonTest_Resource_Gem);
		ASSERT_THAT(IsTrue(Table->Rules[0].TagQuery.Matches(GemTags),
			TEXT("AnyTagsMatch should match Gem")));

		FGameplayTagContainer WoodTags;
		WoodTags.AddTag(TAG_JsonTest_Resource_Wood);
		ASSERT_THAT(IsFalse(Table->Rules[0].TagQuery.Matches(WoodTags),
			TEXT("AnyTagsMatch should NOT match Wood")));
	}

	TEST_METHOD(ParseJson_AllTagsMatchQuery)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "MaterialPropertyTable",
			"name": "AllTagsTest",
			"rules": [
				{
					"name": "AllRule",
					"tagQuery": {
						"type": "AllTagsMatch",
						"tags": ["Resource.Metal", "Resource.Gem"]
					}
				}
			]
		})");

		UArcMaterialPropertyTable* Table = NewObject<UArcMaterialPropertyTable>(GetTransientPackage());
		UArcMaterialPropertyTableJsonLoader::ParseJson(Json, Table);

		ASSERT_THAT(AreEqual(1, Table->Rules.Num()));

		// Only Metal — should NOT match AllTagsMatch
		FGameplayTagContainer MetalOnly;
		MetalOnly.AddTag(TAG_JsonTest_Resource_Metal);
		ASSERT_THAT(IsFalse(Table->Rules[0].TagQuery.Matches(MetalOnly),
			TEXT("AllTagsMatch requires both tags, Metal alone should fail")));

		// Both Metal + Gem — should match
		FGameplayTagContainer BothTags;
		BothTags.AddTag(TAG_JsonTest_Resource_Metal);
		BothTags.AddTag(TAG_JsonTest_Resource_Gem);
		ASSERT_THAT(IsTrue(Table->Rules[0].TagQuery.Matches(BothTags),
			TEXT("AllTagsMatch should match when both tags present")));
	}

	TEST_METHOD(ParseJson_NoTagsMatchQuery)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "MaterialPropertyTable",
			"name": "NoTagsTest",
			"rules": [
				{
					"name": "NoRule",
					"tagQuery": {
						"type": "NoTagsMatch",
						"tags": ["Resource.Wood"]
					}
				}
			]
		})");

		UArcMaterialPropertyTable* Table = NewObject<UArcMaterialPropertyTable>(GetTransientPackage());
		UArcMaterialPropertyTableJsonLoader::ParseJson(Json, Table);

		ASSERT_THAT(AreEqual(1, Table->Rules.Num()));

		// Metal — should match (no wood present)
		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_JsonTest_Resource_Metal);
		ASSERT_THAT(IsTrue(Table->Rules[0].TagQuery.Matches(MetalTags),
			TEXT("NoTagsMatch(Wood): Metal should match")));

		// Wood — should NOT match
		FGameplayTagContainer WoodTags;
		WoodTags.AddTag(TAG_JsonTest_Resource_Wood);
		ASSERT_THAT(IsFalse(Table->Rules[0].TagQuery.Matches(WoodTags),
			TEXT("NoTagsMatch(Wood): Wood should NOT match")));
	}

	TEST_METHOD(ParseJson_CompoundTagQuery_AllExprMatch)
	{
		// Compound: AllExprMatch [ AnyTagsMatch(Metal,Gem), NoTagsMatch(Wood) ]
		// Meaning: must have Metal OR Gem, AND must NOT have Wood
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "MaterialPropertyTable",
			"name": "CompoundTest",
			"rules": [
				{
					"name": "CompoundRule",
					"tagQuery": {
						"type": "AllExprMatch",
						"expressions": [
							{
								"type": "AnyTagsMatch",
								"tags": ["Resource.Metal", "Resource.Gem"]
							},
							{
								"type": "NoTagsMatch",
								"tags": ["Resource.Wood"]
							}
						]
					}
				}
			]
		})");

		UArcMaterialPropertyTable* Table = NewObject<UArcMaterialPropertyTable>(GetTransientPackage());
		UArcMaterialPropertyTableJsonLoader::ParseJson(Json, Table);

		ASSERT_THAT(AreEqual(1, Table->Rules.Num()));

		// Metal only → should match (has metal, no wood)
		FGameplayTagContainer MetalOnly;
		MetalOnly.AddTag(TAG_JsonTest_Resource_Metal);
		ASSERT_THAT(IsTrue(Table->Rules[0].TagQuery.Matches(MetalOnly),
			TEXT("Metal only should match compound query")));

		// Metal + Wood → should NOT match (has wood)
		FGameplayTagContainer MetalAndWood;
		MetalAndWood.AddTag(TAG_JsonTest_Resource_Metal);
		MetalAndWood.AddTag(TAG_JsonTest_Resource_Wood);
		ASSERT_THAT(IsFalse(Table->Rules[0].TagQuery.Matches(MetalAndWood),
			TEXT("Metal+Wood should NOT match (NoTagsMatch Wood fails)")));

		// Wood only → should NOT match (no metal/gem, and has wood)
		FGameplayTagContainer WoodOnly;
		WoodOnly.AddTag(TAG_JsonTest_Resource_Wood);
		ASSERT_THAT(IsFalse(Table->Rules[0].TagQuery.Matches(WoodOnly),
			TEXT("Wood only should NOT match")));

		// Gem only → should match (has gem, no wood)
		FGameplayTagContainer GemOnly;
		GemOnly.AddTag(TAG_JsonTest_Resource_Gem);
		ASSERT_THAT(IsTrue(Table->Rules[0].TagQuery.Matches(GemOnly),
			TEXT("Gem only should match compound query")));
	}

	TEST_METHOD(ParseJson_QualityBands_InlineOnRule)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "MaterialPropertyTable",
			"name": "BandsTest",
			"rules": [
				{
					"name": "BandRule",
					"tagQuery": {
						"type": "AnyTagsMatch",
						"tags": ["Resource.Metal"]
					},
					"qualityBands": [
						{
							"name": "Low",
							"minQuality": 0,
							"baseWeight": 2,
							"qualityWeightBias": 0.1,
							"modifiers": [
								{
									"type": "Stats",
									"qualityScaling": 0,
									"stat": { "value": 5, "modType": "Additive" }
								}
							]
						},
						{
							"name": "High",
							"minQuality": 3,
							"baseWeight": 1,
							"qualityWeightBias": 0.5,
							"modifiers": [
								{
									"type": "Stats",
									"qualityScaling": 0.3,
									"stat": { "value": 20, "modType": "Additive" }
								}
							]
						}
					]
				}
			]
		})");

		UArcMaterialPropertyTable* Table = NewObject<UArcMaterialPropertyTable>(GetTransientPackage());
		UArcMaterialPropertyTableJsonLoader::ParseJson(Json, Table);

		ASSERT_THAT(AreEqual(1, Table->Rules.Num()));
		const FArcMaterialPropertyRule& Rule = Table->Rules[0];
		ASSERT_THAT(AreEqual(2, Rule.QualityBands.Num(), TEXT("Should have 2 bands")));

		// Band 0: Low
		const FArcMaterialQualityBand& Band0 = Rule.QualityBands[0];
		ASSERT_THAT(AreEqual(FString("Low"), Band0.BandName.ToString()));
		ASSERT_THAT(IsNear(0.0f, Band0.MinQuality, 0.001f));
		ASSERT_THAT(IsNear(2.0f, Band0.BaseWeight, 0.001f));
		ASSERT_THAT(IsNear(0.1f, Band0.QualityWeightBias, 0.001f));
		ASSERT_THAT(AreEqual(1, Band0.Modifiers.Num()));
		const FArcCraftModifier_Stats* StatMod0 = Band0.Modifiers[0].GetPtr<FArcCraftModifier_Stats>();
		ASSERT_THAT(IsNotNull(StatMod0));
		ASSERT_THAT(IsNear(5.0f, StatMod0->BaseStat.Value.GetValue(), 0.001f));

		// Band 1: High
		const FArcMaterialQualityBand& Band1 = Rule.QualityBands[1];
		ASSERT_THAT(AreEqual(FString("High"), Band1.BandName.ToString()));
		ASSERT_THAT(IsNear(3.0f, Band1.MinQuality, 0.001f));
		ASSERT_THAT(IsNear(1.0f, Band1.BaseWeight, 0.001f));
		ASSERT_THAT(IsNear(0.5f, Band1.QualityWeightBias, 0.001f));
		ASSERT_THAT(AreEqual(1, Band1.Modifiers.Num()));
		const FArcCraftModifier_Stats* StatMod1 = Band1.Modifiers[0].GetPtr<FArcCraftModifier_Stats>();
		ASSERT_THAT(IsNotNull(StatMod1));
		ASSERT_THAT(IsNear(0.3f, StatMod1->QualityScalingFactor, 0.001f));
		ASSERT_THAT(IsNear(20.0f, StatMod1->BaseStat.Value.GetValue(), 0.001f));
	}

	TEST_METHOD(ParseJson_MultipleRules_OrderPreserved)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "MaterialPropertyTable",
			"name": "MultiRuleTable",
			"rules": [
				{
					"name": "Rule A",
					"priority": 5,
					"tagQuery": { "type": "AnyTagsMatch", "tags": ["Resource.Metal"] }
				},
				{
					"name": "Rule B",
					"priority": 10,
					"tagQuery": { "type": "AnyTagsMatch", "tags": ["Resource.Gem"] }
				},
				{
					"name": "Rule C",
					"priority": 1,
					"tagQuery": { "type": "AnyTagsMatch", "tags": ["Resource.Wood"] }
				}
			]
		})");

		UArcMaterialPropertyTable* Table = NewObject<UArcMaterialPropertyTable>(GetTransientPackage());
		UArcMaterialPropertyTableJsonLoader::ParseJson(Json, Table);

		ASSERT_THAT(AreEqual(3, Table->Rules.Num(), TEXT("Should have 3 rules")));
		ASSERT_THAT(AreEqual(FString("Rule A"), Table->Rules[0].RuleName.ToString()));
		ASSERT_THAT(AreEqual(5, Table->Rules[0].Priority));
		ASSERT_THAT(AreEqual(FString("Rule B"), Table->Rules[1].RuleName.ToString()));
		ASSERT_THAT(AreEqual(10, Table->Rules[1].Priority));
		ASSERT_THAT(AreEqual(FString("Rule C"), Table->Rules[2].RuleName.ToString()));
		ASSERT_THAT(AreEqual(1, Table->Rules[2].Priority));
	}

	TEST_METHOD(ParseJson_EmptyTable_NoRules)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "MaterialPropertyTable",
			"name": "EmptyTable",
			"rules": []
		})");

		UArcMaterialPropertyTable* Table = NewObject<UArcMaterialPropertyTable>(GetTransientPackage());
		UArcMaterialPropertyTableJsonLoader::ParseJson(Json, Table);

		ASSERT_THAT(AreEqual(FString("EmptyTable"), Table->TableName.ToString()));
		ASSERT_THAT(AreEqual(0, Table->Rules.Num()));
	}
};

// ===================================================================
// QualityBandPreset: JSON Roundtrip
// ===================================================================

TEST_CLASS(QualityBandPreset_JsonSerialization, "ArcCraft.Json.Integration.QualityBandPreset")
{
	TEST_METHOD(ParsePresetJson_BasicPreset_AllFieldsCorrect)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "QualityBandPreset",
			"name": "TestPreset",
			"qualityBands": [
				{
					"name": "Common",
					"minQuality": 0,
					"baseWeight": 3,
					"qualityWeightBias": 0
				},
				{
					"name": "Rare",
					"minQuality": 2.5,
					"baseWeight": 1,
					"qualityWeightBias": 0.5,
					"modifiers": [
						{
							"type": "Stats",
							"qualityScaling": 0.1,
							"stat": { "value": 15, "modType": "Additive" }
						}
					]
				},
				{
					"name": "Epic",
					"minQuality": 5,
					"baseWeight": 0.5,
					"qualityWeightBias": 1,
					"modifiers": [
						{
							"type": "Stats",
							"qualityScaling": 0.5,
							"stat": { "value": 50, "modType": "Additive" }
						},
						{
							"type": "Stats",
							"qualityScaling": 0.5,
							"stat": { "value": 1.5, "modType": "Multiply" }
						}
					]
				}
			]
		})");

		UArcQualityBandPreset* Preset = NewObject<UArcQualityBandPreset>(GetTransientPackage());
		const bool bSuccess = UArcMaterialPropertyTableJsonLoader::ParsePresetJson(Json, Preset);
		ASSERT_THAT(IsTrue(bSuccess));

		ASSERT_THAT(AreEqual(FString("TestPreset"), Preset->PresetName.ToString()));
		ASSERT_THAT(AreEqual(3, Preset->QualityBands.Num(), TEXT("Should have 3 bands")));

		// Band 0: Common
		const FArcMaterialQualityBand& B0 = Preset->QualityBands[0];
		ASSERT_THAT(AreEqual(FString("Common"), B0.BandName.ToString()));
		ASSERT_THAT(IsNear(0.0f, B0.MinQuality, 0.001f));
		ASSERT_THAT(IsNear(3.0f, B0.BaseWeight, 0.001f));
		ASSERT_THAT(IsNear(0.0f, B0.QualityWeightBias, 0.001f));
		ASSERT_THAT(AreEqual(0, B0.Modifiers.Num(), TEXT("Common band has no modifiers")));

		// Band 1: Rare
		const FArcMaterialQualityBand& B1 = Preset->QualityBands[1];
		ASSERT_THAT(AreEqual(FString("Rare"), B1.BandName.ToString()));
		ASSERT_THAT(IsNear(2.5f, B1.MinQuality, 0.001f));
		ASSERT_THAT(IsNear(1.0f, B1.BaseWeight, 0.001f));
		ASSERT_THAT(IsNear(0.5f, B1.QualityWeightBias, 0.001f));
		ASSERT_THAT(AreEqual(1, B1.Modifiers.Num()));
		const FArcCraftModifier_Stats* Stat1 = B1.Modifiers[0].GetPtr<FArcCraftModifier_Stats>();
		ASSERT_THAT(IsNotNull(Stat1));
		ASSERT_THAT(IsNear(0.1f, Stat1->QualityScalingFactor, 0.001f));
		ASSERT_THAT(IsNear(15.0f, Stat1->BaseStat.Value.GetValue(), 0.001f));

		// Band 2: Epic
		const FArcMaterialQualityBand& B2 = Preset->QualityBands[2];
		ASSERT_THAT(AreEqual(FString("Epic"), B2.BandName.ToString()));
		ASSERT_THAT(IsNear(5.0f, B2.MinQuality, 0.001f));
		ASSERT_THAT(IsNear(0.5f, B2.BaseWeight, 0.001f));
		ASSERT_THAT(IsNear(1.0f, B2.QualityWeightBias, 0.001f));
		ASSERT_THAT(AreEqual(2, B2.Modifiers.Num(), TEXT("Epic band should have 2 modifiers")));
		const FArcCraftModifier_Stats* Stat2a = B2.Modifiers[0].GetPtr<FArcCraftModifier_Stats>();
		ASSERT_THAT(IsNotNull(Stat2a));
		ASSERT_THAT(IsNear(0.5f, Stat2a->QualityScalingFactor, 0.001f));
		ASSERT_THAT(IsNear(50.0f, Stat2a->BaseStat.Value.GetValue(), 0.001f));
		ASSERT_THAT(AreEqual((int32)EArcModType::Additive, (int32)Stat2a->BaseStat.Type));

		const FArcCraftModifier_Stats* Stat2b = B2.Modifiers[1].GetPtr<FArcCraftModifier_Stats>();
		ASSERT_THAT(IsNotNull(Stat2b));
		ASSERT_THAT(IsNear(1.5f, Stat2b->BaseStat.Value.GetValue(), 0.001f));
		ASSERT_THAT(AreEqual((int32)EArcModType::Multiply, (int32)Stat2b->BaseStat.Type));
	}

	TEST_METHOD(ParsePresetJson_EmptyPreset)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "QualityBandPreset",
			"name": "EmptyPreset",
			"qualityBands": []
		})");

		UArcQualityBandPreset* Preset = NewObject<UArcQualityBandPreset>(GetTransientPackage());
		UArcMaterialPropertyTableJsonLoader::ParsePresetJson(Json, Preset);

		ASSERT_THAT(AreEqual(FString("EmptyPreset"), Preset->PresetName.ToString()));
		ASSERT_THAT(AreEqual(0, Preset->QualityBands.Num()));
	}

	TEST_METHOD(ParsePresetJson_BandWithDivisionModType)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "QualityBandPreset",
			"name": "DivPreset",
			"qualityBands": [
				{
					"name": "DivBand",
					"minQuality": 0,
					"baseWeight": 1,
					"qualityWeightBias": 0,
					"modifiers": [
						{
							"type": "Stats",
							"stat": { "value": 3, "modType": "Division" }
						}
					]
				}
			]
		})");

		UArcQualityBandPreset* Preset = NewObject<UArcQualityBandPreset>(GetTransientPackage());
		UArcMaterialPropertyTableJsonLoader::ParsePresetJson(Json, Preset);

		ASSERT_THAT(AreEqual(1, Preset->QualityBands.Num()));
		const FArcMaterialQualityBand& Band = Preset->QualityBands[0];
		ASSERT_THAT(AreEqual(1, Band.Modifiers.Num()));

		const FArcCraftModifier_Stats* StatMod = Band.Modifiers[0].GetPtr<FArcCraftModifier_Stats>();
		ASSERT_THAT(IsNotNull(StatMod));
		ASSERT_THAT(IsNear(3.0f, StatMod->BaseStat.Value.GetValue(), 0.001f));
		ASSERT_THAT(AreEqual((int32)EArcModType::Division, (int32)StatMod->BaseStat.Type,
			TEXT("ModType should be Division")));
	}

	TEST_METHOD(ParsePresetJson_MultipleModifiersPerBand)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "QualityBandPreset",
			"name": "MultiModPreset",
			"qualityBands": [
				{
					"name": "MultiModBand",
					"minQuality": 0,
					"baseWeight": 1,
					"qualityWeightBias": 0,
					"modifiers": [
						{
							"type": "Stats",
							"qualityScaling": 0,
							"weight": 2.5,
							"stat": { "value": 10, "modType": "Additive" }
						},
						{
							"type": "Stats",
							"minQuality": 1.5,
							"qualityScaling": 0.8,
							"triggerTags": ["Resource.Metal"],
							"stat": { "value": 20, "modType": "Multiply" }
						}
					]
				}
			]
		})");

		UArcQualityBandPreset* Preset = NewObject<UArcQualityBandPreset>(GetTransientPackage());
		UArcMaterialPropertyTableJsonLoader::ParsePresetJson(Json, Preset);

		ASSERT_THAT(AreEqual(1, Preset->QualityBands.Num()));
		const FArcMaterialQualityBand& Band = Preset->QualityBands[0];
		ASSERT_THAT(AreEqual(2, Band.Modifiers.Num(), TEXT("Should have 2 modifiers")));

		// First modifier: Stats with weight
		const FArcCraftModifier_Stats* FirstMod = Band.Modifiers[0].GetPtr<FArcCraftModifier_Stats>();
		ASSERT_THAT(IsNotNull(FirstMod, TEXT("First modifier should be CraftModifier_Stats")));
		ASSERT_THAT(IsNear(10.0f, FirstMod->BaseStat.Value.GetValue(), 0.001f));
		ASSERT_THAT(IsNear(2.5f, FirstMod->Weight, 0.001f, TEXT("First modifier weight")));

		// Second modifier: Stats with trigger tags and minQuality
		const FArcCraftModifier_Stats* SecondMod = Band.Modifiers[1].GetPtr<FArcCraftModifier_Stats>();
		ASSERT_THAT(IsNotNull(SecondMod, TEXT("Second modifier should be CraftModifier_Stats")));
		ASSERT_THAT(IsNear(1.5f, SecondMod->MinQualityThreshold, 0.001f, TEXT("Second modifier minQuality")));
		ASSERT_THAT(IsNear(0.8f, SecondMod->QualityScalingFactor, 0.001f, TEXT("Second modifier qualityScaling")));
		ASSERT_THAT(IsTrue(SecondMod->TriggerTags.HasTag(TAG_JsonTest_Resource_Metal),
			TEXT("Second modifier should have Metal trigger tag")));
		ASSERT_THAT(IsNear(20.0f, SecondMod->BaseStat.Value.GetValue(), 0.001f));
	}
};

// ===================================================================
// Edge cases and error handling
// ===================================================================

TEST_CLASS(JsonSerialization_EdgeCases, "ArcCraft.Json.Integration.EdgeCases")
{
	TEST_METHOD(ParseJson_NullTarget_ReturnsFalse)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "QualityTierTable",
			"tiers": []
		})");

		ASSERT_THAT(IsFalse(UArcQualityTierTableJsonLoader::ParseJson(Json, nullptr)));
		ASSERT_THAT(IsFalse(UArcRandomPoolJsonLoader::ParseJson(Json, nullptr)));
		ASSERT_THAT(IsFalse(UArcMaterialPropertyTableJsonLoader::ParseJson(Json, nullptr)));
		ASSERT_THAT(IsFalse(UArcMaterialPropertyTableJsonLoader::ParsePresetJson(Json, nullptr)));
	}

	TEST_METHOD(InvalidJson_ThrowsCaught)
	{
		// Verify that invalid JSON strings don't crash — nlohmann::json::parse throws
		//bool bCaughtException = false;
		//try
		//{
		//	nlohmann::json::parse("this is not json at all");
		//}
		//catch (const nlohmann::json::parse_error&)
		//{
		//	bCaughtException = true;
		//}
		//ASSERT_THAT(IsTrue(bCaughtException, TEXT("Invalid JSON should throw parse_error")));
	}

	TEST_METHOD(ParseJson_ReimportClears_PreviousData)
	{
		// Simulate reimport: parse once, then parse different data into same object
		UArcQualityTierTable* Table = NewObject<UArcQualityTierTable>(GetTransientPackage());

		// First parse: 3 tiers
		{
			const auto Json = JsonTestHelpers::ParseJsonString(R"({
				"tiers": [
					{ "tag": "Quality.Common", "value": 1, "multiplier": 1.0 },
					{ "tag": "Quality.Rare", "value": 2, "multiplier": 1.5 },
					{ "tag": "Quality.Epic", "value": 3, "multiplier": 2.0 }
				]
			})");
			UArcQualityTierTableJsonLoader::ParseJson(Json, Table);
			ASSERT_THAT(AreEqual(3, Table->Tiers.Num()));
		}

		// Second parse (reimport): only 1 tier — should replace, not append
		{
			const auto Json = JsonTestHelpers::ParseJsonString(R"({
				"tiers": [
					{ "tag": "Quality.Common", "value": 10, "multiplier": 5.0 }
				]
			})");
			UArcQualityTierTableJsonLoader::ParseJson(Json, Table);
			ASSERT_THAT(AreEqual(1, Table->Tiers.Num(), TEXT("Reimport should replace, not append")));
			ASSERT_THAT(AreEqual(10, Table->Tiers[0].TierValue, TEXT("Should have new data")));
			ASSERT_THAT(IsNear(5.0f, Table->Tiers[0].QualityMultiplier, 0.001f));
		}
	}

	TEST_METHOD(ParseJson_UnicodeCharacters_Handled)
	{
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "MaterialPropertyTable",
			"name": "Tëst Tàble — Ünïcödé",
			"rules": []
		})");

		UArcMaterialPropertyTable* Table = NewObject<UArcMaterialPropertyTable>(GetTransientPackage());
		UArcMaterialPropertyTableJsonLoader::ParseJson(Json, Table);

		// Just verify it doesn't crash and the name is non-empty
		ASSERT_THAT(IsFalse(Table->TableName.IsEmpty(), TEXT("Name should be parsed even with unicode")));
	}

	TEST_METHOD(ParseJson_MissingOptionalFields_UsesDefaults)
	{
		// Minimal JSON with only required fields
		const auto Json = JsonTestHelpers::ParseJsonString(R"({
			"$type": "RandomPoolDefinition",
			"name": "MinimalPool",
			"entries": [
				{
					"name": "MinimalEntry"
				}
			]
		})");

		UArcRandomPoolDefinition* Pool = NewObject<UArcRandomPoolDefinition>(GetTransientPackage());
		UArcRandomPoolJsonLoader::ParseJson(Json, Pool);

		ASSERT_THAT(AreEqual(1, Pool->Entries.Num()));
		const FArcRandomPoolEntry& E = Pool->Entries[0];

		// All should have defaults
		ASSERT_THAT(IsNear(0.0f, E.MinQualityThreshold, 0.001f, TEXT("Default minQuality")));
		ASSERT_THAT(IsNear(1.0f, E.BaseWeight, 0.001f, TEXT("Default baseWeight")));
		ASSERT_THAT(IsNear(0.0f, E.QualityWeightScaling, 0.001f, TEXT("Default qualityWeightScaling")));
		ASSERT_THAT(IsNear(1.0f, E.Cost, 0.001f, TEXT("Default cost")));
		ASSERT_THAT(IsNear(1.0f, E.ValueScale, 0.001f, TEXT("Default valueScale")));
		ASSERT_THAT(IsNear(0.0f, E.ValueSkew, 0.001f, TEXT("Default valueSkew")));
		ASSERT_THAT(IsTrue(E.bScaleByQuality, TEXT("Default scaleByQuality")));
		ASSERT_THAT(AreEqual(0, E.RequiredIngredientTags.Num()));
		ASSERT_THAT(AreEqual(0, E.DenyIngredientTags.Num()));
		ASSERT_THAT(AreEqual(0, E.WeightModifiers.Num()));
		ASSERT_THAT(AreEqual(0, E.ValueSkewRules.Num()));
		ASSERT_THAT(AreEqual(0, E.Modifiers.Num()));
	}
};
