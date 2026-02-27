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

#include "ArcCraft/Shared/ArcCraftJsonUtils.h"

#include "Interfaces/IPluginManager.h"
#include "Chooser.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagsManager.h"
#include "ArcCraft/Recipe/ArcRecipeOutput.h"
#include "ArcCraft/Recipe/ArcRandomPoolDefinition.h"
#include "ArcCraft/Recipe/ArcRandomPoolSelectionMode.h"
#include "ArcCraft/MaterialCraft/ArcMaterialOutputModifier.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTable.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyRule.h"
#include "ArcCraft/Shared/ArcCraftModifier.h"
#include "StructUtils/InstancedStruct.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcCraftJsonUtils, Log, All);

// -------------------------------------------------------------------
// Schema Path Resolution
// -------------------------------------------------------------------

FString ArcCraftJsonUtils::GetSchemaFilePath(const FString& SchemaFileName)
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ArcCraft"));
	if (!Plugin.IsValid())
	{
		UE_LOG(LogArcCraftJsonUtils, Warning, TEXT("GetSchemaFilePath: ArcCraft plugin not found via IPluginManager"));
		return FString();
	}

	return Plugin->GetBaseDir() / TEXT("Schemas") / SchemaFileName;
}

// -------------------------------------------------------------------
// Internal helpers
// -------------------------------------------------------------------

namespace
{
	/** Map ExprType enum to string for serialization. */
	FString ExprTypeToString(EGameplayTagQueryExprType InType)
	{
		switch (InType)
		{
		case EGameplayTagQueryExprType::AnyTagsMatch:      return TEXT("AnyTagsMatch");
		case EGameplayTagQueryExprType::AllTagsMatch:      return TEXT("AllTagsMatch");
		case EGameplayTagQueryExprType::NoTagsMatch:       return TEXT("NoTagsMatch");
		case EGameplayTagQueryExprType::AnyExprMatch:      return TEXT("AnyExprMatch");
		case EGameplayTagQueryExprType::AllExprMatch:      return TEXT("AllExprMatch");
		case EGameplayTagQueryExprType::NoExprMatch:       return TEXT("NoExprMatch");
		case EGameplayTagQueryExprType::AnyTagsExactMatch: return TEXT("AnyTagsExactMatch");
		case EGameplayTagQueryExprType::AllTagsExactMatch: return TEXT("AllTagsExactMatch");
		default:                                           return TEXT("Undefined");
		}
	}

	/** Map string to ExprType enum for parsing. */
	EGameplayTagQueryExprType StringToExprType(const FString& TypeStr)
	{
		if (TypeStr == TEXT("AnyTagsMatch"))      return EGameplayTagQueryExprType::AnyTagsMatch;
		if (TypeStr == TEXT("AllTagsMatch"))      return EGameplayTagQueryExprType::AllTagsMatch;
		if (TypeStr == TEXT("NoTagsMatch"))       return EGameplayTagQueryExprType::NoTagsMatch;
		if (TypeStr == TEXT("AnyExprMatch"))      return EGameplayTagQueryExprType::AnyExprMatch;
		if (TypeStr == TEXT("AllExprMatch"))      return EGameplayTagQueryExprType::AllExprMatch;
		if (TypeStr == TEXT("NoExprMatch"))       return EGameplayTagQueryExprType::NoExprMatch;
		if (TypeStr == TEXT("AnyTagsExactMatch")) return EGameplayTagQueryExprType::AnyTagsExactMatch;
		if (TypeStr == TEXT("AllTagsExactMatch")) return EGameplayTagQueryExprType::AllTagsExactMatch;
		return EGameplayTagQueryExprType::Undefined;
	}

	/** Map EArcModType to string for serialization. */
	FString ModTypeToString(EArcModType InType)
	{
		switch (InType)
		{
		case EArcModType::Multiply:  return TEXT("Multiply");
		case EArcModType::Division:  return TEXT("Division");
		case EArcModType::Additive:
		default:                     return TEXT("Additive");
		}
	}

	/** Map string to EArcModType for parsing. */
	EArcModType StringToModType(const FString& TypeStr)
	{
		if (TypeStr == TEXT("Multiply"))  return EArcModType::Multiply;
		if (TypeStr == TEXT("Division"))  return EArcModType::Division;
		return EArcModType::Additive;
	}

	/** Safely read a string from a JSON object field. Returns empty FString if not present or not a string. */
	FString GetJsonString(const nlohmann::json& Obj, const char* Key)
	{
		if (Obj.contains(Key) && Obj[Key].is_string())
		{
			return UTF8_TO_TCHAR(Obj[Key].get<std::string>().c_str());
		}
		return FString();
	}

	/** Safely read a float from a JSON object field. Returns Default if not present or not a number. */
	float GetJsonFloat(const nlohmann::json& Obj, const char* Key, float Default = 0.0f)
	{
		if (Obj.contains(Key) && Obj[Key].is_number())
		{
			return Obj[Key].get<float>();
		}
		return Default;
	}

	/** Safely read an int from a JSON object field. Returns Default if not present or not a number. */
	int32 GetJsonInt(const nlohmann::json& Obj, const char* Key, int32 Default = 0)
	{
		if (Obj.contains(Key) && Obj[Key].is_number())
		{
			return Obj[Key].get<int32>();
		}
		return Default;
	}

	/** Safely read a bool from a JSON object field. Returns Default if not present or not a boolean. */
	bool GetJsonBool(const nlohmann::json& Obj, const char* Key, bool Default = false)
	{
		if (Obj.contains(Key) && Obj[Key].is_boolean())
		{
			return Obj[Key].get<bool>();
		}
		return Default;
	}
}

// -------------------------------------------------------------------
// Parsing (JSON -> UE)
// -------------------------------------------------------------------

FGameplayTag ArcCraftJsonUtils::ParseGameplayTag(const FString& TagString)
{
	const FString Trimmed = TagString.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return FGameplayTag();
	}
	return FGameplayTag::RequestGameplayTag(FName(*Trimmed), /*bErrorIfNotFound=*/ false);
}

FGameplayTagContainer ArcCraftJsonUtils::ParseGameplayTags(const nlohmann::json& TagsArray)
{
	FGameplayTagContainer Container;

	if (!TagsArray.is_array())
	{
		return Container;
	}

	for (const auto& Element : TagsArray)
	{
		if (Element.is_string())
		{
			const FString TagStr = UTF8_TO_TCHAR(Element.get<std::string>().c_str());
			const FGameplayTag Tag = ParseGameplayTag(TagStr);
			if (Tag.IsValid())
			{
				Container.AddTag(Tag);
			}
		}
	}

	return Container;
}

bool ArcCraftJsonUtils::ParseOutputModifier(const nlohmann::json& ModObj, FInstancedStruct& OutModifier)
{
	if (!ModObj.is_object())
	{
		return false;
	}

	const FString TypeStr = GetJsonString(ModObj, "type");
	const FGameplayTag SlotTag = ParseGameplayTag(GetJsonString(ModObj, "slotTag"));
	const float MinQuality = GetJsonFloat(ModObj, "minQuality", 0.0f);
	const float ModWeight = GetJsonFloat(ModObj, "weight", 1.0f);
	const float QualityScaling = GetJsonFloat(ModObj, "qualityScaling", 1.0f);
	const FGameplayTagContainer BaseTriggerTags = ModObj.contains("triggerTags") ? ParseGameplayTags(ModObj["triggerTags"]) : FGameplayTagContainer();

	// ---- Stats ----
	if (TypeStr == TEXT("Stats"))
	{
		OutModifier.InitializeAs<FArcRecipeOutputModifier_Stats>();
		FArcRecipeOutputModifier_Stats& Modifier = OutModifier.GetMutable<FArcRecipeOutputModifier_Stats>();
		Modifier.SlotTag = SlotTag;
		Modifier.MinQualityThreshold = MinQuality;
		Modifier.TriggerTags = BaseTriggerTags;
		Modifier.Weight = ModWeight;
		Modifier.QualityScalingFactor = QualityScaling;

		if (ModObj.contains("stat") && ModObj["stat"].is_object())
		{
			const auto& StatObj = ModObj["stat"];
			Modifier.BaseStat.SetValue(GetJsonFloat(StatObj, "value", 0.0f));
			Modifier.BaseStat.Type = StringToModType(GetJsonString(StatObj, "modType"));
		}

		return true;
	}

	// ---- Abilities ----
	if (TypeStr == TEXT("Abilities"))
	{
		OutModifier.InitializeAs<FArcRecipeOutputModifier_Abilities>();
		FArcRecipeOutputModifier_Abilities& Modifier = OutModifier.GetMutable<FArcRecipeOutputModifier_Abilities>();
		Modifier.SlotTag = SlotTag;
		Modifier.MinQualityThreshold = MinQuality;
		Modifier.TriggerTags = BaseTriggerTags;
		Modifier.Weight = ModWeight;
		Modifier.QualityScalingFactor = QualityScaling;

		if (ModObj.contains("ability") && ModObj["ability"].is_object())
		{
			const auto& AbilityObj = ModObj["ability"];
			const FString ClassStr = GetJsonString(AbilityObj, "class");
			if (!ClassStr.IsEmpty())
			{
				Modifier.AbilityToGrant.GrantedAbility = LoadClass<UGameplayAbility>(nullptr, *ClassStr);
			}

			const FString InputTagStr = GetJsonString(AbilityObj, "inputTag");
			Modifier.AbilityToGrant.InputTag = ParseGameplayTag(InputTagStr);
			Modifier.AbilityToGrant.bAddInputTag = Modifier.AbilityToGrant.InputTag.IsValid();
		}

		return true;
	}

	// ---- Effects ----
	if (TypeStr == TEXT("Effects"))
	{
		OutModifier.InitializeAs<FArcRecipeOutputModifier_Effects>();
		FArcRecipeOutputModifier_Effects& Modifier = OutModifier.GetMutable<FArcRecipeOutputModifier_Effects>();
		Modifier.SlotTag = SlotTag;
		Modifier.MinQualityThreshold = MinQuality;
		Modifier.TriggerTags = BaseTriggerTags;
		Modifier.Weight = ModWeight;
		Modifier.QualityScalingFactor = QualityScaling;

		if (ModObj.contains("effect") && ModObj["effect"].is_object())
		{
			const FString ClassStr = GetJsonString(ModObj["effect"], "class");
			if (!ClassStr.IsEmpty())
			{
				Modifier.EffectToGrant = LoadClass<UGameplayEffect>(nullptr, *ClassStr);
			}
		}

		return true;
	}

	// ---- TransferStats ----
	if (TypeStr == TEXT("TransferStats"))
	{
		OutModifier.InitializeAs<FArcRecipeOutputModifier_TransferStats>();
		FArcRecipeOutputModifier_TransferStats& Modifier = OutModifier.GetMutable<FArcRecipeOutputModifier_TransferStats>();
		Modifier.SlotTag = SlotTag;
		Modifier.MinQualityThreshold = MinQuality;
		Modifier.TriggerTags = BaseTriggerTags;
		Modifier.Weight = ModWeight;
		Modifier.QualityScalingFactor = QualityScaling;
		Modifier.IngredientSlotIndex = GetJsonInt(ModObj, "ingredientSlot", 0);
		Modifier.TransferScale = GetJsonFloat(ModObj, "transferScale", 1.0f);
		Modifier.bScaleByQuality = GetJsonBool(ModObj, "scaleByQuality", true);

		return true;
	}

	// ---- Random (Chooser) ----
	if (TypeStr == TEXT("Random"))
	{
		OutModifier.InitializeAs<FArcRecipeOutputModifier_Random>();
		FArcRecipeOutputModifier_Random& Modifier = OutModifier.GetMutable<FArcRecipeOutputModifier_Random>();
		Modifier.SlotTag = SlotTag;
		Modifier.MinQualityThreshold = MinQuality;
		Modifier.TriggerTags = BaseTriggerTags;
		Modifier.Weight = ModWeight;
		Modifier.QualityScalingFactor = QualityScaling;

		const FString ChooserTableStr = GetJsonString(ModObj, "chooserTable");
		if (!ChooserTableStr.IsEmpty())
		{
			Modifier.ModifierChooserTable = TSoftObjectPtr<UChooserTable>(FSoftObjectPath(ChooserTableStr));
		}

		Modifier.MaxRolls = FMath::Max(1, GetJsonInt(ModObj, "maxRolls", 1));
		Modifier.bAllowDuplicates = GetJsonBool(ModObj, "allowDuplicates", false);
		Modifier.bQualityAffectsRolls = GetJsonBool(ModObj, "qualityAffectsRolls", false);
		Modifier.QualityBonusRollThreshold = GetJsonFloat(ModObj, "qualityBonusRollThreshold", 2.0f);

		return true;
	}

	// ---- RandomPool ----
	if (TypeStr == TEXT("RandomPool"))
	{
		OutModifier.InitializeAs<FArcRecipeOutputModifier_RandomPool>();
		FArcRecipeOutputModifier_RandomPool& Modifier = OutModifier.GetMutable<FArcRecipeOutputModifier_RandomPool>();
		Modifier.SlotTag = SlotTag;
		Modifier.MinQualityThreshold = MinQuality;
		Modifier.TriggerTags = BaseTriggerTags;
		Modifier.Weight = ModWeight;
		Modifier.QualityScalingFactor = QualityScaling;

		const FString PoolDefStr = GetJsonString(ModObj, "poolDefinition");
		if (!PoolDefStr.IsEmpty())
		{
			Modifier.PoolDefinition = TSoftObjectPtr<UArcRandomPoolDefinition>(FSoftObjectPath(PoolDefStr));
		}

		const bool bAllowDups = GetJsonBool(ModObj, "allowDuplicates", false);

		const FString SelectionModeStr = GetJsonString(ModObj, "selectionMode");
		if (SelectionModeStr == TEXT("Budget"))
		{
			Modifier.SelectionMode.InitializeAs<FArcRandomPoolSelection_Budget>();
			FArcRandomPoolSelection_Budget& BudgetMode = Modifier.SelectionMode.GetMutable<FArcRandomPoolSelection_Budget>();
			BudgetMode.bAllowDuplicates = bAllowDups;
			BudgetMode.BaseBudget = GetJsonFloat(ModObj, "baseBudget", 3.0f);
			BudgetMode.BudgetPerQuality = GetJsonFloat(ModObj, "budgetPerQuality", 1.0f);
			BudgetMode.MaxBudgetSelections = GetJsonInt(ModObj, "maxBudgetSelections", 0);
		}
		else // Default to SimpleRandom
		{
			Modifier.SelectionMode.InitializeAs<FArcRandomPoolSelection_SimpleRandom>();
			FArcRandomPoolSelection_SimpleRandom& SimpleMode = Modifier.SelectionMode.GetMutable<FArcRandomPoolSelection_SimpleRandom>();
			SimpleMode.bAllowDuplicates = bAllowDups;
			SimpleMode.MaxSelections = FMath::Max(1, GetJsonInt(ModObj, "maxSelections", 1));
			SimpleMode.bQualityAffectsSelections = GetJsonBool(ModObj, "qualityAffectsSelections", false);
			SimpleMode.QualityBonusThreshold = GetJsonFloat(ModObj, "qualityBonusThreshold", 2.0f);
		}

		return true;
	}

	// ---- MaterialProperties ----
	if (TypeStr == TEXT("MaterialProperties"))
	{
		OutModifier.InitializeAs<FArcRecipeOutputModifier_MaterialProperties>();
		FArcRecipeOutputModifier_MaterialProperties& Modifier = OutModifier.GetMutable<FArcRecipeOutputModifier_MaterialProperties>();
		Modifier.SlotTag = SlotTag;
		Modifier.MinQualityThreshold = MinQuality;
		Modifier.TriggerTags = BaseTriggerTags;
		Modifier.Weight = ModWeight;
		Modifier.QualityScalingFactor = QualityScaling;

		const FString PropertyTableStr = GetJsonString(ModObj, "propertyTable");
		if (!PropertyTableStr.IsEmpty())
		{
			Modifier.PropertyTable = TSoftObjectPtr<UArcMaterialPropertyTable>(FSoftObjectPath(PropertyTableStr));
		}

		Modifier.BaseIngredientCount = FMath::Max(0, GetJsonInt(ModObj, "baseIngredientCount", 0));
		Modifier.ExtraCraftTimeBonus = GetJsonFloat(ModObj, "extraCraftTimeBonus", 0.0f);
		Modifier.bUseRecipeTierTable = GetJsonBool(ModObj, "useRecipeTierTable", true);

		if (ModObj.contains("recipeTags") && ModObj["recipeTags"].is_array())
		{
			Modifier.RecipeTags = ParseGameplayTags(ModObj["recipeTags"]);
		}

		return true;
	}

	UE_LOG(LogArcCraftJsonUtils, Warning, TEXT("Unknown output modifier type: %s"), *TypeStr);
	return false;
}

FGameplayTagQueryExpression ArcCraftJsonUtils::ParseTagQueryExpr(const nlohmann::json& ExprObj)
{
	FGameplayTagQueryExpression Expr;

	if (!ExprObj.is_object())
	{
		return Expr;
	}

	const FString TypeStr = GetJsonString(ExprObj, "type");
	Expr.ExprType = StringToExprType(TypeStr);

	if (Expr.UsesTagSet())
	{
		if (ExprObj.contains("tags") && ExprObj["tags"].is_array())
		{
			for (const auto& TagElement : ExprObj["tags"])
			{
				if (TagElement.is_string())
				{
					const FString TagStr = UTF8_TO_TCHAR(TagElement.get<std::string>().c_str());
					FGameplayTag Tag = ParseGameplayTag(TagStr);
					if (Tag.IsValid())
					{
						Expr.TagSet.Add(Tag);
					}
				}
			}
		}
	}
	else if (Expr.UsesExprSet())
	{
		if (ExprObj.contains("expressions") && ExprObj["expressions"].is_array())
		{
			for (const auto& SubExprObj : ExprObj["expressions"])
			{
				Expr.ExprSet.Add(ParseTagQueryExpr(SubExprObj));
			}
		}
	}

	return Expr;
}

FGameplayTagQuery ArcCraftJsonUtils::ParseTagQuery(const nlohmann::json& QueryObj)
{
	FGameplayTagQueryExpression Expr = ParseTagQueryExpr(QueryObj);
	return FGameplayTagQuery::BuildQuery(Expr);
}

bool ArcCraftJsonUtils::ParseQualityBand(const nlohmann::json& BandObj, FArcMaterialQualityBand& OutBand)
{
	if (!BandObj.is_object())
	{
		return false;
	}

	const FString NameStr = GetJsonString(BandObj, "name");
	if (!NameStr.IsEmpty())
	{
		OutBand.BandName = FText::FromString(NameStr);
	}

	OutBand.MinQuality = GetJsonFloat(BandObj, "minQuality", 0.0f);
	OutBand.BaseWeight = GetJsonFloat(BandObj, "baseWeight", 1.0f);
	OutBand.QualityWeightBias = GetJsonFloat(BandObj, "qualityWeightBias", 0.0f);

	if (BandObj.contains("modifiers") && BandObj["modifiers"].is_array())
	{
		for (const auto& ModObj : BandObj["modifiers"])
		{
			FInstancedStruct ModifierStruct;
			if (ParseCraftModifier(ModObj, ModifierStruct))
			{
				OutBand.Modifiers.Add(MoveTemp(ModifierStruct));
			}
		}
	}

	return true;
}

bool ArcCraftJsonUtils::ParseCraftModifier(const nlohmann::json& ModObj, FInstancedStruct& OutModifier)
{
	if (!ModObj.is_object()) return false;

	const FString TypeStr = GetJsonString(ModObj, "type");

	// Base fields shared by all craft modifiers
	const float MinQuality = GetJsonFloat(ModObj, "minQuality", 0.0f);
	const FGameplayTagContainer TriggerTags = ModObj.contains("triggerTags") ? ParseGameplayTags(ModObj["triggerTags"]) : FGameplayTagContainer();
	const float ModWeight = GetJsonFloat(ModObj, "weight", 1.0f);
	const float QualityScaling = GetJsonFloat(ModObj, "qualityScaling", 1.0f);

	if (TypeStr == TEXT("Stats"))
	{
		OutModifier.InitializeAs<FArcCraftModifier_Stats>();
		FArcCraftModifier_Stats& Mod = OutModifier.GetMutable<FArcCraftModifier_Stats>();
		Mod.MinQualityThreshold = MinQuality;
		Mod.TriggerTags = TriggerTags;
		Mod.Weight = ModWeight;
		Mod.QualityScalingFactor = QualityScaling;

		if (ModObj.contains("stat") && ModObj["stat"].is_object())
		{
			const auto& StatObj = ModObj["stat"];
			Mod.BaseStat.SetValue(GetJsonFloat(StatObj, "value", 0.0f));
			Mod.BaseStat.Type = StringToModType(GetJsonString(StatObj, "modType"));
		}
		return true;
	}

	if (TypeStr == TEXT("Abilities"))
	{
		OutModifier.InitializeAs<FArcCraftModifier_Abilities>();
		FArcCraftModifier_Abilities& Mod = OutModifier.GetMutable<FArcCraftModifier_Abilities>();
		Mod.MinQualityThreshold = MinQuality;
		Mod.TriggerTags = TriggerTags;
		Mod.Weight = ModWeight;
		Mod.QualityScalingFactor = QualityScaling;

		if (ModObj.contains("ability") && ModObj["ability"].is_object())
		{
			const auto& AbilityObj = ModObj["ability"];
			const FString ClassStr = GetJsonString(AbilityObj, "class");
			if (!ClassStr.IsEmpty())
			{
				Mod.AbilityToGrant.GrantedAbility = LoadClass<UGameplayAbility>(nullptr, *ClassStr);
			}
			const FString InputTagStr = GetJsonString(AbilityObj, "inputTag");
			Mod.AbilityToGrant.InputTag = ParseGameplayTag(InputTagStr);
			Mod.AbilityToGrant.bAddInputTag = Mod.AbilityToGrant.InputTag.IsValid();
		}
		return true;
	}

	if (TypeStr == TEXT("Effects"))
	{
		OutModifier.InitializeAs<FArcCraftModifier_Effects>();
		FArcCraftModifier_Effects& Mod = OutModifier.GetMutable<FArcCraftModifier_Effects>();
		Mod.MinQualityThreshold = MinQuality;
		Mod.TriggerTags = TriggerTags;
		Mod.Weight = ModWeight;
		Mod.QualityScalingFactor = QualityScaling;

		if (ModObj.contains("effect") && ModObj["effect"].is_object())
		{
			const FString ClassStr = GetJsonString(ModObj["effect"], "class");
			if (!ClassStr.IsEmpty())
			{
				Mod.EffectToGrant = LoadClass<UGameplayEffect>(nullptr, *ClassStr);
			}
		}
		return true;
	}

	UE_LOG(LogArcCraftJsonUtils, Warning, TEXT("Unknown craft modifier type: %s"), *TypeStr);
	return false;
}

// -------------------------------------------------------------------
// Serialization (UE -> JSON)
// -------------------------------------------------------------------

nlohmann::json ArcCraftJsonUtils::SerializeTagContainer(const FGameplayTagContainer& Tags)
{
	nlohmann::json Array = nlohmann::json::array();

	for (const FGameplayTag& Tag : Tags)
	{
		Array.push_back(TCHAR_TO_UTF8(*Tag.ToString()));
	}

	return Array;
}

nlohmann::json ArcCraftJsonUtils::SerializeGameplayTags(const FGameplayTagContainer& Tags)
{
	return SerializeTagContainer(Tags);
}

nlohmann::json ArcCraftJsonUtils::SerializeOutputModifier(const FInstancedStruct& Modifier)
{
	nlohmann::json Obj = nlohmann::json::object();

	auto SerializeBaseFields = [](nlohmann::json& Obj, const FArcRecipeOutputModifier* Base)
	{
		if (Base->SlotTag.IsValid()) { Obj["slotTag"] = TCHAR_TO_UTF8(*Base->SlotTag.ToString()); }
		if (Base->MinQualityThreshold != 0.0f) { Obj["minQuality"] = Base->MinQualityThreshold; }
		if (!Base->TriggerTags.IsEmpty()) { Obj["triggerTags"] = SerializeTagContainer(Base->TriggerTags); }
		if (Base->Weight != 1.0f) { Obj["weight"] = Base->Weight; }
		if (Base->QualityScalingFactor != 1.0f) { Obj["qualityScaling"] = Base->QualityScalingFactor; }
	};

	// ---- Stats ----
	if (const FArcRecipeOutputModifier_Stats* Stats = Modifier.GetPtr<FArcRecipeOutputModifier_Stats>())
	{
		Obj["type"] = "Stats";
		SerializeBaseFields(Obj, Stats);

		nlohmann::json StatObj = nlohmann::json::object();
		StatObj["value"] = Stats->BaseStat.Value.GetValueAtLevel(0);
		StatObj["modType"] = TCHAR_TO_UTF8(*ModTypeToString(Stats->BaseStat.Type));
		if (Stats->BaseStat.Attribute.IsValid())
		{
			StatObj["attribute"] = TCHAR_TO_UTF8(*Stats->BaseStat.Attribute.GetName());
		}
		Obj["stat"] = StatObj;

		return Obj;
	}

	// ---- Abilities ----
	if (const FArcRecipeOutputModifier_Abilities* Abilities = Modifier.GetPtr<FArcRecipeOutputModifier_Abilities>())
	{
		Obj["type"] = "Abilities";
		SerializeBaseFields(Obj, Abilities);

		nlohmann::json AbilityObj = nlohmann::json::object();
		if (Abilities->AbilityToGrant.GrantedAbility)
		{
			AbilityObj["class"] = TCHAR_TO_UTF8(*Abilities->AbilityToGrant.GrantedAbility->GetPathName());
		}
		if (Abilities->AbilityToGrant.InputTag.IsValid())
		{
			AbilityObj["inputTag"] = TCHAR_TO_UTF8(*Abilities->AbilityToGrant.InputTag.ToString());
		}
		Obj["ability"] = AbilityObj;

		return Obj;
	}

	// ---- Effects ----
	if (const FArcRecipeOutputModifier_Effects* Effects = Modifier.GetPtr<FArcRecipeOutputModifier_Effects>())
	{
		Obj["type"] = "Effects";
		SerializeBaseFields(Obj, Effects);

		nlohmann::json EffectObj = nlohmann::json::object();
		if (Effects->EffectToGrant)
		{
			EffectObj["class"] = TCHAR_TO_UTF8(*Effects->EffectToGrant->GetPathName());
		}
		Obj["effect"] = EffectObj;

		return Obj;
	}

	// ---- TransferStats ----
	if (const FArcRecipeOutputModifier_TransferStats* Transfer = Modifier.GetPtr<FArcRecipeOutputModifier_TransferStats>())
	{
		Obj["type"] = "TransferStats";
		SerializeBaseFields(Obj, Transfer);

		Obj["ingredientSlot"] = Transfer->IngredientSlotIndex;
		Obj["transferScale"] = Transfer->TransferScale;
		Obj["scaleByQuality"] = Transfer->bScaleByQuality;

		return Obj;
	}

	// ---- Random (Chooser) ----
	if (const FArcRecipeOutputModifier_Random* Random = Modifier.GetPtr<FArcRecipeOutputModifier_Random>())
	{
		Obj["type"] = "Random";
		SerializeBaseFields(Obj, Random);

		if (!Random->ModifierChooserTable.IsNull())
		{
			Obj["chooserTable"] = TCHAR_TO_UTF8(*Random->ModifierChooserTable.ToString());
		}

		Obj["maxRolls"] = Random->MaxRolls;
		Obj["allowDuplicates"] = Random->bAllowDuplicates;
		Obj["qualityAffectsRolls"] = Random->bQualityAffectsRolls;
		Obj["qualityBonusRollThreshold"] = Random->QualityBonusRollThreshold;

		return Obj;
	}

	// ---- RandomPool ----
	if (const FArcRecipeOutputModifier_RandomPool* Pool = Modifier.GetPtr<FArcRecipeOutputModifier_RandomPool>())
	{
		Obj["type"] = "RandomPool";
		SerializeBaseFields(Obj, Pool);

		if (!Pool->PoolDefinition.IsNull())
		{
			Obj["poolDefinition"] = TCHAR_TO_UTF8(*Pool->PoolDefinition.ToString());
		}

		// Serialize selection mode
		if (const FArcRandomPoolSelection_Budget* BudgetMode = Pool->SelectionMode.GetPtr<FArcRandomPoolSelection_Budget>())
		{
			Obj["selectionMode"] = "Budget";
			Obj["allowDuplicates"] = BudgetMode->bAllowDuplicates;
			Obj["baseBudget"] = BudgetMode->BaseBudget;
			Obj["budgetPerQuality"] = BudgetMode->BudgetPerQuality;
			Obj["maxBudgetSelections"] = BudgetMode->MaxBudgetSelections;
		}
		else if (const FArcRandomPoolSelection_SimpleRandom* SimpleMode = Pool->SelectionMode.GetPtr<FArcRandomPoolSelection_SimpleRandom>())
		{
			Obj["selectionMode"] = "SimpleRandom";
			Obj["allowDuplicates"] = SimpleMode->bAllowDuplicates;
			Obj["maxSelections"] = SimpleMode->MaxSelections;
			Obj["qualityAffectsSelections"] = SimpleMode->bQualityAffectsSelections;
			Obj["qualityBonusThreshold"] = SimpleMode->QualityBonusThreshold;
		}

		return Obj;
	}

	// ---- MaterialProperties ----
	if (const FArcRecipeOutputModifier_MaterialProperties* MatProps = Modifier.GetPtr<FArcRecipeOutputModifier_MaterialProperties>())
	{
		Obj["type"] = "MaterialProperties";
		SerializeBaseFields(Obj, MatProps);

		if (!MatProps->PropertyTable.IsNull())
		{
			Obj["propertyTable"] = TCHAR_TO_UTF8(*MatProps->PropertyTable.ToString());
		}

		Obj["baseIngredientCount"] = MatProps->BaseIngredientCount;
		Obj["extraCraftTimeBonus"] = MatProps->ExtraCraftTimeBonus;
		Obj["useRecipeTierTable"] = MatProps->bUseRecipeTierTable;

		if (!MatProps->RecipeTags.IsEmpty())
		{
			Obj["recipeTags"] = SerializeTagContainer(MatProps->RecipeTags);
		}

		return Obj;
	}

	UE_LOG(LogArcCraftJsonUtils, Warning, TEXT("SerializeOutputModifier: Unrecognized modifier type."));
	return nlohmann::json();
}

nlohmann::json ArcCraftJsonUtils::SerializeCraftModifier(const FInstancedStruct& Modifier)
{
	nlohmann::json Obj = nlohmann::json::object();

	auto SerializeCraftBaseFields = [](nlohmann::json& Out, const FArcCraftModifier* Base)
	{
		if (Base->MinQualityThreshold != 0.0f) { Out["minQuality"] = Base->MinQualityThreshold; }
		if (!Base->TriggerTags.IsEmpty()) { Out["triggerTags"] = SerializeTagContainer(Base->TriggerTags); }
		if (Base->Weight != 1.0f) { Out["weight"] = Base->Weight; }
		if (Base->QualityScalingFactor != 1.0f) { Out["qualityScaling"] = Base->QualityScalingFactor; }
	};

	if (const FArcCraftModifier_Stats* Stats = Modifier.GetPtr<FArcCraftModifier_Stats>())
	{
		Obj["type"] = "Stats";
		SerializeCraftBaseFields(Obj, Stats);

		nlohmann::json StatObj = nlohmann::json::object();
		StatObj["value"] = Stats->BaseStat.Value.GetValueAtLevel(0);
		StatObj["modType"] = TCHAR_TO_UTF8(*ModTypeToString(Stats->BaseStat.Type));
		if (Stats->BaseStat.Attribute.IsValid())
		{
			StatObj["attribute"] = TCHAR_TO_UTF8(*Stats->BaseStat.Attribute.GetName());
		}
		Obj["stat"] = StatObj;
		return Obj;
	}

	if (const FArcCraftModifier_Abilities* Abilities = Modifier.GetPtr<FArcCraftModifier_Abilities>())
	{
		Obj["type"] = "Abilities";
		SerializeCraftBaseFields(Obj, Abilities);

		nlohmann::json AbilityObj = nlohmann::json::object();
		if (Abilities->AbilityToGrant.GrantedAbility) { AbilityObj["class"] = TCHAR_TO_UTF8(*Abilities->AbilityToGrant.GrantedAbility->GetPathName()); }
		if (Abilities->AbilityToGrant.InputTag.IsValid()) { AbilityObj["inputTag"] = TCHAR_TO_UTF8(*Abilities->AbilityToGrant.InputTag.ToString()); }
		Obj["ability"] = AbilityObj;
		return Obj;
	}

	if (const FArcCraftModifier_Effects* Effects = Modifier.GetPtr<FArcCraftModifier_Effects>())
	{
		Obj["type"] = "Effects";
		SerializeCraftBaseFields(Obj, Effects);

		nlohmann::json EffectObj = nlohmann::json::object();
		if (Effects->EffectToGrant) { EffectObj["class"] = TCHAR_TO_UTF8(*Effects->EffectToGrant->GetPathName()); }
		Obj["effect"] = EffectObj;
		return Obj;
	}

	UE_LOG(LogArcCraftJsonUtils, Warning, TEXT("SerializeCraftModifier: Unrecognized modifier type."));
	return nlohmann::json();
}

nlohmann::json ArcCraftJsonUtils::SerializeTagQueryExpr(const FGameplayTagQueryExpression& Expr)
{
	nlohmann::json Obj = nlohmann::json::object();

	Obj["type"] = TCHAR_TO_UTF8(*ExprTypeToString(Expr.ExprType));

	if (Expr.UsesTagSet())
	{
		nlohmann::json TagsArray = nlohmann::json::array();
		for (const FGameplayTag& Tag : Expr.TagSet)
		{
			TagsArray.push_back(TCHAR_TO_UTF8(*Tag.ToString()));
		}
		Obj["tags"] = TagsArray;
	}
	else if (Expr.UsesExprSet())
	{
		nlohmann::json ExpressionsArray = nlohmann::json::array();
		for (const FGameplayTagQueryExpression& SubExpr : Expr.ExprSet)
		{
			ExpressionsArray.push_back(SerializeTagQueryExpr(SubExpr));
		}
		Obj["expressions"] = ExpressionsArray;
	}

	return Obj;
}

nlohmann::json ArcCraftJsonUtils::SerializeTagQuery(const FGameplayTagQuery& Query)
{
	FGameplayTagQueryExpression Expr;
	Query.GetQueryExpr(Expr);
	return SerializeTagQueryExpr(Expr);
}

nlohmann::json ArcCraftJsonUtils::SerializeQualityBand(const FArcMaterialQualityBand& Band)
{
	nlohmann::json Obj = nlohmann::json::object();

	Obj["name"] = TCHAR_TO_UTF8(*Band.BandName.ToString());
	Obj["minQuality"] = Band.MinQuality;
	Obj["baseWeight"] = Band.BaseWeight;
	Obj["qualityWeightBias"] = Band.QualityWeightBias;

	nlohmann::json ModifiersArray = nlohmann::json::array();
	for (const FInstancedStruct& ModifierStruct : Band.Modifiers)
	{
		nlohmann::json ModJson = SerializeCraftModifier(ModifierStruct);
		if (!ModJson.is_null())
		{
			ModifiersArray.push_back(ModJson);
		}
	}
	Obj["modifiers"] = ModifiersArray;

	return Obj;
}
