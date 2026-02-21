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

#include "ArcCraft/Recipe/ArcRecipeXmlLoader.h"

#include "Chooser.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagsManager.h"
#include "XmlFile.h"
#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Recipe/ArcRecipeOutput.h"
#include "ArcCraft/Recipe/ArcRecipeQuality.h"
#include "ArcCraft/Recipe/ArcRandomPoolDefinition.h"
#include "ArcCraft/Recipe/ArcRandomPoolSelectionMode.h"
#include "ArcCraft/MaterialCraft/ArcMaterialOutputModifier.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTable.h"
#include "HAL/FileManager.h"
#include "StructUtils/InstancedStruct.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcRecipeXml, Log, All);

// -------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------

FGameplayTag UArcRecipeXmlLoader::ParseGameplayTag(const FString& TagString)
{
	const FString Trimmed = TagString.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return FGameplayTag();
	}
	return FGameplayTag::RequestGameplayTag(FName(*Trimmed), false);
}

FGameplayTagContainer UArcRecipeXmlLoader::ParseGameplayTags(const FString& TagsString)
{
	FGameplayTagContainer Container;
	TArray<FString> TagStrings;
	TagsString.ParseIntoArray(TagStrings, TEXT(","));

	for (const FString& TagStr : TagStrings)
	{
		const FGameplayTag Tag = ParseGameplayTag(TagStr);
		if (Tag.IsValid())
		{
			Container.AddTag(Tag);
		}
	}

	return Container;
}

// -------------------------------------------------------------------
// LoadRecipesFromDirectory
// -------------------------------------------------------------------

TArray<UArcRecipeDefinition*> UArcRecipeXmlLoader::LoadRecipesFromDirectory(
	UObject* Outer,
	const FString& DirectoryPath)
{
	TArray<UArcRecipeDefinition*> AllRecipes;

	TArray<FString> XmlFiles;
	IFileManager::Get().FindFiles(XmlFiles, *FPaths::Combine(DirectoryPath, TEXT("*.xml")), true, false);

	for (const FString& FileName : XmlFiles)
	{
		const FString FullPath = FPaths::Combine(DirectoryPath, FileName);
		TArray<UArcRecipeDefinition*> FileRecipes = LoadRecipesFromFile(Outer, FullPath);
		AllRecipes.Append(FileRecipes);
	}

	UE_LOG(LogArcRecipeXml, Log, TEXT("Loaded %d recipes from directory: %s"), AllRecipes.Num(), *DirectoryPath);
	return AllRecipes;
}

// -------------------------------------------------------------------
// LoadRecipesFromFile
// -------------------------------------------------------------------

TArray<UArcRecipeDefinition*> UArcRecipeXmlLoader::LoadRecipesFromFile(
	UObject* Outer,
	const FString& FilePath)
{
	TArray<UArcRecipeDefinition*> Recipes;

	FXmlFile XmlFile(FilePath);
	if (!XmlFile.IsValid())
	{
		UE_LOG(LogArcRecipeXml, Error, TEXT("Failed to parse XML file: %s - %s"),
			*FilePath, *XmlFile.GetLastError());
		return Recipes;
	}

	const FXmlNode* RootNode = XmlFile.GetRootNode();
	if (!RootNode)
	{
		UE_LOG(LogArcRecipeXml, Error, TEXT("No root node in XML file: %s"), *FilePath);
		return Recipes;
	}

	// Support both <Recipes><Recipe>... and <Recipe>... as root
	TArray<const FXmlNode*> RecipeNodes;
	if (RootNode->GetTag() == TEXT("Recipes"))
	{
		for (const FXmlNode* Child : RootNode->GetChildrenNodes())
		{
			if (Child->GetTag() == TEXT("Recipe"))
			{
				RecipeNodes.Add(Child);
			}
		}
	}
	else if (RootNode->GetTag() == TEXT("Recipe"))
	{
		RecipeNodes.Add(RootNode);
	}

	for (const FXmlNode* RecipeNode : RecipeNodes)
	{
		UArcRecipeDefinition* Recipe = NewObject<UArcRecipeDefinition>(
			Outer ? Outer : GetTransientPackage());

		if (ParseRecipeNode(RecipeNode, Recipe))
		{
			Recipes.Add(Recipe);
		}
		else
		{
			UE_LOG(LogArcRecipeXml, Warning, TEXT("Failed to parse recipe node in: %s"), *FilePath);
		}
	}

	return Recipes;
}

// -------------------------------------------------------------------
// ParseRecipeNode
// -------------------------------------------------------------------

bool UArcRecipeXmlLoader::ParseRecipeNode(
	const FXmlNode* RecipeNode,
	UArcRecipeDefinition* OutRecipe)
{
	if (!RecipeNode || !OutRecipe)
	{
		return false;
	}

	// Attributes on <Recipe>
	const FString IdStr = RecipeNode->GetAttribute(TEXT("Id"));
	if (!IdStr.IsEmpty())
	{
		if (!FGuid::Parse(IdStr, OutRecipe->RecipeId))
		{
			// If the Id string is not a valid GUID, generate a deterministic one from the name
			OutRecipe->RecipeId = FGuid::NewDeterministicGuid(IdStr);
		}
	}
	else
	{
		OutRecipe->RecipeId = FGuid::NewGuid();
	}
	OutRecipe->RecipeName = FText::FromString(RecipeNode->GetAttribute(TEXT("Name")));

	const FString CraftTimeStr = RecipeNode->GetAttribute(TEXT("CraftTime"));
	if (!CraftTimeStr.IsEmpty())
	{
		OutRecipe->CraftTime = FCString::Atof(*CraftTimeStr);
	}

	// Child elements
	for (const FXmlNode* ChildNode : RecipeNode->GetChildrenNodes())
	{
		const FString& Tag = ChildNode->GetTag();

		if (Tag == TEXT("Tags"))
		{
			OutRecipe->RecipeTags = ParseGameplayTags(ChildNode->GetContent());
		}
		else if (Tag == TEXT("RequiredStationTags"))
		{
			OutRecipe->RequiredStationTags = ParseGameplayTags(ChildNode->GetContent());
		}
		else if (Tag == TEXT("RequiredInstigatorTags"))
		{
			OutRecipe->RequiredInstigatorTags = ParseGameplayTags(ChildNode->GetContent());
		}
		else if (Tag == TEXT("QualityTierTable"))
		{
			const FString Path = ChildNode->GetContent().TrimStartAndEnd();
			OutRecipe->QualityTierTable = TSoftObjectPtr<UArcQualityTierTable>(FSoftObjectPath(Path));
		}
		else if (Tag == TEXT("Ingredients"))
		{
			for (const FXmlNode* IngredientNode : ChildNode->GetChildrenNodes())
			{
				if (IngredientNode->GetTag() == TEXT("Ingredient"))
				{
					FInstancedStruct IngredientStruct;
					if (ParseIngredientNode(IngredientNode, IngredientStruct))
					{
						OutRecipe->Ingredients.Add(MoveTemp(IngredientStruct));
					}
				}
			}
		}
		else if (Tag == TEXT("Output"))
		{
			// Output attributes
			const FString ItemDefStr = ChildNode->GetAttribute(TEXT("ItemDefinition"));
			if (!ItemDefStr.IsEmpty())
			{
				OutRecipe->OutputItemDefinition = FPrimaryAssetId(ItemDefStr);
			}

			const FString AmountStr = ChildNode->GetAttribute(TEXT("Amount"));
			if (!AmountStr.IsEmpty())
			{
				OutRecipe->OutputAmount = FCString::Atoi(*AmountStr);
			}

			const FString LevelStr = ChildNode->GetAttribute(TEXT("Level"));
			if (!LevelStr.IsEmpty())
			{
				OutRecipe->OutputLevel = static_cast<uint8>(FCString::Atoi(*LevelStr));
			}

			const FString QualityAffectsLevelStr = ChildNode->GetAttribute(TEXT("QualityAffectsLevel"));
			if (QualityAffectsLevelStr.Equals(TEXT("true"), ESearchCase::IgnoreCase))
			{
				OutRecipe->bQualityAffectsLevel = true;
			}

			// Parse modifiers
			for (const FXmlNode* ModChild : ChildNode->GetChildrenNodes())
			{
				if (ModChild->GetTag() == TEXT("Modifiers"))
				{
					for (const FXmlNode* ModNode : ModChild->GetChildrenNodes())
					{
						if (ModNode->GetTag() == TEXT("Modifier"))
						{
							FInstancedStruct ModifierStruct;
							if (ParseOutputModifierNode(ModNode, ModifierStruct))
							{
								OutRecipe->OutputModifiers.Add(MoveTemp(ModifierStruct));
							}
						}
					}
				}
			}
		}
	}

	return OutRecipe->RecipeId.IsValid();
}

// -------------------------------------------------------------------
// ParseIngredientNode
// -------------------------------------------------------------------

bool UArcRecipeXmlLoader::ParseIngredientNode(
	const FXmlNode* IngredientNode,
	FInstancedStruct& OutIngredient)
{
	const FString TypeStr = IngredientNode->GetAttribute(TEXT("Type"));
	const FString AmountStr = IngredientNode->GetAttribute(TEXT("Amount"));
	const FString ConsumeStr = IngredientNode->GetAttribute(TEXT("Consume"));
	const FString SlotNameStr = IngredientNode->GetAttribute(TEXT("SlotName"));

	const int32 Amount = AmountStr.IsEmpty() ? 1 : FCString::Atoi(*AmountStr);
	const bool bConsume = ConsumeStr.IsEmpty() || ConsumeStr.Equals(TEXT("true"), ESearchCase::IgnoreCase);

	if (TypeStr == TEXT("ItemDef"))
	{
		OutIngredient.InitializeAs<FArcRecipeIngredient_ItemDef>();
		FArcRecipeIngredient_ItemDef& Ingredient = OutIngredient.GetMutable<FArcRecipeIngredient_ItemDef>();
		Ingredient.Amount = Amount;
		Ingredient.bConsumeOnCraft = bConsume;
		Ingredient.SlotName = FText::FromString(SlotNameStr);

		for (const FXmlNode* Child : IngredientNode->GetChildrenNodes())
		{
			if (Child->GetTag() == TEXT("ItemDefinition"))
			{
				Ingredient.ItemDefinitionId = FPrimaryAssetId(Child->GetContent().TrimStartAndEnd());
			}
		}
		return true;
	}
	else if (TypeStr == TEXT("Tags"))
	{
		OutIngredient.InitializeAs<FArcRecipeIngredient_Tags>();
		FArcRecipeIngredient_Tags& Ingredient = OutIngredient.GetMutable<FArcRecipeIngredient_Tags>();
		Ingredient.Amount = Amount;
		Ingredient.bConsumeOnCraft = bConsume;
		Ingredient.SlotName = FText::FromString(SlotNameStr);

		for (const FXmlNode* Child : IngredientNode->GetChildrenNodes())
		{
			const FString& ChildTag = Child->GetTag();
			if (ChildTag == TEXT("RequiredTags"))
			{
				Ingredient.RequiredTags = ParseGameplayTags(Child->GetContent());
			}
			else if (ChildTag == TEXT("DenyTags"))
			{
				Ingredient.DenyTags = ParseGameplayTags(Child->GetContent());
			}
			else if (ChildTag == TEXT("MinimumTier"))
			{
				Ingredient.MinimumTierTag = ParseGameplayTag(Child->GetContent());
			}
		}
		return true;
	}

	UE_LOG(LogArcRecipeXml, Warning, TEXT("Unknown ingredient type: %s"), *TypeStr);
	return false;
}

// -------------------------------------------------------------------
// ParseOutputModifierNode
// -------------------------------------------------------------------

bool UArcRecipeXmlLoader::ParseOutputModifierNode(
	const FXmlNode* ModifierNode,
	FInstancedStruct& OutModifier)
{
	const FString TypeStr = ModifierNode->GetAttribute(TEXT("Type"));

	if (TypeStr == TEXT("Stats"))
	{
		OutModifier.InitializeAs<FArcRecipeOutputModifier_Stats>();
		FArcRecipeOutputModifier_Stats& Modifier = OutModifier.GetMutable<FArcRecipeOutputModifier_Stats>();

		const FString QualityScalingStr = ModifierNode->GetAttribute(TEXT("QualityScaling"));
		if (!QualityScalingStr.IsEmpty())
		{
			Modifier.QualityScalingFactor = FCString::Atof(*QualityScalingStr);
		}

		for (const FXmlNode* StatNode : ModifierNode->GetChildrenNodes())
		{
			if (StatNode->GetTag() == TEXT("Stat"))
			{
				FArcItemAttributeStat Stat;

				const FString AttrStr = StatNode->GetAttribute(TEXT("Attribute"));
				const FString ValueStr = StatNode->GetAttribute(TEXT("Value"));
				const FString ModTypeStr = StatNode->GetAttribute(TEXT("ModType"));

				Stat.SetValue(ValueStr.IsEmpty() ? 0.0f : FCString::Atof(*ValueStr));

				if (ModTypeStr == TEXT("Multiply"))
				{
					Stat.Type = EArcModType::Multiply;
				}
				else if (ModTypeStr == TEXT("Division"))
				{
					Stat.Type = EArcModType::Division;
				}
				else
				{
					Stat.Type = EArcModType::Additive;
				}

				// Note: Attribute is set by name. The FGameplayAttribute reference
				// will need to be resolved at runtime by the stat system.
				// For XML import, we store the name and it gets matched during item initialization.

				Modifier.BaseStats.Add(Stat);
			}
		}
		return true;
	}
	else if (TypeStr == TEXT("Abilities"))
	{
		OutModifier.InitializeAs<FArcRecipeOutputModifier_Abilities>();
		FArcRecipeOutputModifier_Abilities& Modifier = OutModifier.GetMutable<FArcRecipeOutputModifier_Abilities>();

		// Support both new TriggerTags (comma-separated) and legacy TriggerTag (single)
		const FString TriggerTagsStr = ModifierNode->GetAttribute(TEXT("TriggerTags"));
		const FString TriggerTagStr = ModifierNode->GetAttribute(TEXT("TriggerTag"));
		if (!TriggerTagsStr.IsEmpty())
		{
			Modifier.TriggerTags = ParseGameplayTags(TriggerTagsStr);
		}
		else if (!TriggerTagStr.IsEmpty())
		{
			const FGameplayTag Tag = ParseGameplayTag(TriggerTagStr);
			if (Tag.IsValid())
			{
				Modifier.TriggerTags.AddTag(Tag);
			}
		}

		for (const FXmlNode* AbilityNode : ModifierNode->GetChildrenNodes())
		{
			if (AbilityNode->GetTag() == TEXT("Ability"))
			{
				FArcAbilityEntry Entry;
				const FString ClassStr = AbilityNode->GetAttribute(TEXT("Class"));
				const FString InputTagStr = AbilityNode->GetAttribute(TEXT("InputTag"));

				if (!ClassStr.IsEmpty())
				{
					Entry.GrantedAbility = LoadClass<UGameplayAbility>(nullptr, *ClassStr);
				}
				Entry.InputTag = ParseGameplayTag(InputTagStr);
				Entry.bAddInputTag = Entry.InputTag.IsValid();

				Modifier.AbilitiesToGrant.Add(Entry);
			}
		}
		return true;
	}
	else if (TypeStr == TEXT("Effects"))
	{
		OutModifier.InitializeAs<FArcRecipeOutputModifier_Effects>();
		FArcRecipeOutputModifier_Effects& Modifier = OutModifier.GetMutable<FArcRecipeOutputModifier_Effects>();

		// Support both new TriggerTags (comma-separated) and legacy TriggerTag (single)
		const FString TriggerTagsStr = ModifierNode->GetAttribute(TEXT("TriggerTags"));
		const FString TriggerTagStr = ModifierNode->GetAttribute(TEXT("TriggerTag"));
		if (!TriggerTagsStr.IsEmpty())
		{
			Modifier.TriggerTags = ParseGameplayTags(TriggerTagsStr);
		}
		else if (!TriggerTagStr.IsEmpty())
		{
			const FGameplayTag Tag = ParseGameplayTag(TriggerTagStr);
			if (Tag.IsValid())
			{
				Modifier.TriggerTags.AddTag(Tag);
			}
		}

		const FString MinQualityStr = ModifierNode->GetAttribute(TEXT("MinQuality"));
		if (!MinQualityStr.IsEmpty())
		{
			Modifier.MinQualityThreshold = FCString::Atof(*MinQualityStr);
		}

		for (const FXmlNode* EffectNode : ModifierNode->GetChildrenNodes())
		{
			if (EffectNode->GetTag() == TEXT("Effect"))
			{
				const FString ClassStr = EffectNode->GetAttribute(TEXT("Class"));
				if (!ClassStr.IsEmpty())
				{
					TSubclassOf<UGameplayEffect> EffectClass = LoadClass<UGameplayEffect>(nullptr, *ClassStr);
					if (EffectClass)
					{
						Modifier.EffectsToGrant.Add(EffectClass);
					}
				}
			}
		}
		return true;
	}
	else if (TypeStr == TEXT("TransferStats"))
	{
		OutModifier.InitializeAs<FArcRecipeOutputModifier_TransferStats>();
		FArcRecipeOutputModifier_TransferStats& Modifier = OutModifier.GetMutable<FArcRecipeOutputModifier_TransferStats>();

		const FString SlotStr = ModifierNode->GetAttribute(TEXT("IngredientSlot"));
		if (!SlotStr.IsEmpty())
		{
			Modifier.IngredientSlotIndex = FCString::Atoi(*SlotStr);
		}

		const FString ScaleStr = ModifierNode->GetAttribute(TEXT("TransferScale"));
		if (!ScaleStr.IsEmpty())
		{
			Modifier.TransferScale = FCString::Atof(*ScaleStr);
		}

		const FString ScaleByQualityStr = ModifierNode->GetAttribute(TEXT("ScaleByQuality"));
		Modifier.bScaleByQuality = ScaleByQualityStr.IsEmpty() || ScaleByQualityStr.Equals(TEXT("true"), ESearchCase::IgnoreCase);

		return true;
	}
	else if (TypeStr == TEXT("Random"))
	{
		OutModifier.InitializeAs<FArcRecipeOutputModifier_Random>();
		FArcRecipeOutputModifier_Random& Modifier = OutModifier.GetMutable<FArcRecipeOutputModifier_Random>();

		const FString ChooserTableStr = ModifierNode->GetAttribute(TEXT("ChooserTable"));
		if (!ChooserTableStr.IsEmpty())
		{
			Modifier.ModifierChooserTable = TSoftObjectPtr<UChooserTable>(FSoftObjectPath(ChooserTableStr));
		}

		const FString MaxRollsStr = ModifierNode->GetAttribute(TEXT("MaxRolls"));
		if (!MaxRollsStr.IsEmpty())
		{
			Modifier.MaxRolls = FMath::Max(1, FCString::Atoi(*MaxRollsStr));
		}

		const FString AllowDuplicatesStr = ModifierNode->GetAttribute(TEXT("AllowDuplicates"));
		Modifier.bAllowDuplicates = AllowDuplicatesStr.Equals(TEXT("true"), ESearchCase::IgnoreCase);

		const FString QualityAffectsRollsStr = ModifierNode->GetAttribute(TEXT("QualityAffectsRolls"));
		Modifier.bQualityAffectsRolls = QualityAffectsRollsStr.Equals(TEXT("true"), ESearchCase::IgnoreCase);

		const FString QualityBonusThresholdStr = ModifierNode->GetAttribute(TEXT("QualityBonusRollThreshold"));
		if (!QualityBonusThresholdStr.IsEmpty())
		{
			Modifier.QualityBonusRollThreshold = FCString::Atof(*QualityBonusThresholdStr);
		}

		return true;
	}
	else if (TypeStr == TEXT("RandomPool"))
	{
		OutModifier.InitializeAs<FArcRecipeOutputModifier_RandomPool>();
		FArcRecipeOutputModifier_RandomPool& Modifier = OutModifier.GetMutable<FArcRecipeOutputModifier_RandomPool>();

		// Pool asset path
		const FString PoolDefStr = ModifierNode->GetAttribute(TEXT("PoolDefinition"));
		if (!PoolDefStr.IsEmpty())
		{
			Modifier.PoolDefinition = TSoftObjectPtr<UArcRandomPoolDefinition>(FSoftObjectPath(PoolDefStr));
		}

		// Common
		const FString AllowDuplicatesStr = ModifierNode->GetAttribute(TEXT("AllowDuplicates"));
		const bool bAllowDups = AllowDuplicatesStr.Equals(TEXT("true"), ESearchCase::IgnoreCase);

		// Selection mode
		const FString SelectionModeStr = ModifierNode->GetAttribute(TEXT("SelectionMode"));
		if (SelectionModeStr == TEXT("Budget"))
		{
			Modifier.SelectionMode.InitializeAs<FArcRandomPoolSelection_Budget>();
			FArcRandomPoolSelection_Budget& BudgetMode = Modifier.SelectionMode.GetMutable<FArcRandomPoolSelection_Budget>();
			BudgetMode.bAllowDuplicates = bAllowDups;

			const FString BaseBudgetStr = ModifierNode->GetAttribute(TEXT("BaseBudget"));
			if (!BaseBudgetStr.IsEmpty())
			{
				BudgetMode.BaseBudget = FCString::Atof(*BaseBudgetStr);
			}

			const FString BudgetPerQualityStr = ModifierNode->GetAttribute(TEXT("BudgetPerQuality"));
			if (!BudgetPerQualityStr.IsEmpty())
			{
				BudgetMode.BudgetPerQuality = FCString::Atof(*BudgetPerQualityStr);
			}

			const FString MaxBudgetSelectionsStr = ModifierNode->GetAttribute(TEXT("MaxBudgetSelections"));
			if (!MaxBudgetSelectionsStr.IsEmpty())
			{
				BudgetMode.MaxBudgetSelections = FCString::Atoi(*MaxBudgetSelectionsStr);
			}
		}
		else // Default to SimpleRandom
		{
			Modifier.SelectionMode.InitializeAs<FArcRandomPoolSelection_SimpleRandom>();
			FArcRandomPoolSelection_SimpleRandom& SimpleMode = Modifier.SelectionMode.GetMutable<FArcRandomPoolSelection_SimpleRandom>();
			SimpleMode.bAllowDuplicates = bAllowDups;

			const FString MaxSelectionsStr = ModifierNode->GetAttribute(TEXT("MaxSelections"));
			if (!MaxSelectionsStr.IsEmpty())
			{
				SimpleMode.MaxSelections = FMath::Max(1, FCString::Atoi(*MaxSelectionsStr));
			}

			const FString QualityAffectsStr = ModifierNode->GetAttribute(TEXT("QualityAffectsSelections"));
			SimpleMode.bQualityAffectsSelections = QualityAffectsStr.Equals(TEXT("true"), ESearchCase::IgnoreCase);

			const FString QualityBonusStr = ModifierNode->GetAttribute(TEXT("QualityBonusThreshold"));
			if (!QualityBonusStr.IsEmpty())
			{
				SimpleMode.QualityBonusThreshold = FCString::Atof(*QualityBonusStr);
			}
		}

		return true;
	}
	else if (TypeStr == TEXT("MaterialProperties"))
	{
		OutModifier.InitializeAs<FArcRecipeOutputModifier_MaterialProperties>();
		FArcRecipeOutputModifier_MaterialProperties& Modifier = OutModifier.GetMutable<FArcRecipeOutputModifier_MaterialProperties>();

		// Property table asset path
		const FString PropertyTableStr = ModifierNode->GetAttribute(TEXT("PropertyTable"));
		if (!PropertyTableStr.IsEmpty())
		{
			Modifier.PropertyTable = TSoftObjectPtr<UArcMaterialPropertyTable>(FSoftObjectPath(PropertyTableStr));
		}

		// Base ingredient count for extra-material remedy
		const FString BaseIngredientCountStr = ModifierNode->GetAttribute(TEXT("BaseIngredientCount"));
		if (!BaseIngredientCountStr.IsEmpty())
		{
			Modifier.BaseIngredientCount = FMath::Max(0, FCString::Atoi(*BaseIngredientCountStr));
		}

		// Extra craft time bonus
		const FString ExtraCraftTimeBonusStr = ModifierNode->GetAttribute(TEXT("ExtraCraftTimeBonus"));
		if (!ExtraCraftTimeBonusStr.IsEmpty())
		{
			Modifier.ExtraCraftTimeBonus = FCString::Atof(*ExtraCraftTimeBonusStr);
		}

		// Recipe tier table override
		const FString UseRecipeTierTableStr = ModifierNode->GetAttribute(TEXT("UseRecipeTierTable"));
		if (!UseRecipeTierTableStr.IsEmpty())
		{
			Modifier.bUseRecipeTierTable = UseRecipeTierTableStr.Equals(TEXT("true"), ESearchCase::IgnoreCase);
		}

		// Recipe tags for rule matching
		const FString RecipeTagsStr = ModifierNode->GetAttribute(TEXT("RecipeTags"));
		if (!RecipeTagsStr.IsEmpty())
		{
			Modifier.RecipeTags = ParseGameplayTags(RecipeTagsStr);
		}

		// Parse modifier slot configs from child elements
		for (const FXmlNode* SlotNode : ModifierNode->GetChildrenNodes())
		{
			if (SlotNode->GetTag() == TEXT("ModifierSlot"))
			{
				FArcMaterialModifierSlotConfig SlotConfig;

				const FString SlotTagStr = SlotNode->GetAttribute(TEXT("Tag"));
				if (!SlotTagStr.IsEmpty())
				{
					SlotConfig.SlotTag = FGameplayTag::RequestGameplayTag(FName(*SlotTagStr), false);
				}

				const FString MaxCountStr = SlotNode->GetAttribute(TEXT("MaxCount"));
				if (!MaxCountStr.IsEmpty())
				{
					SlotConfig.MaxCount = FMath::Max(0, FCString::Atoi(*MaxCountStr));
				}

				const FString SelectionStr = SlotNode->GetAttribute(TEXT("Selection"));
				if (SelectionStr.Equals(TEXT("Random"), ESearchCase::IgnoreCase))
				{
					SlotConfig.SelectionMode = EArcModifierSlotSelection::Random;
				}
				else if (SelectionStr.Equals(TEXT("All"), ESearchCase::IgnoreCase))
				{
					SlotConfig.SelectionMode = EArcModifierSlotSelection::All;
				}
				else
				{
					SlotConfig.SelectionMode = EArcModifierSlotSelection::HighestWeight;
				}

				if (SlotConfig.SlotTag.IsValid())
				{
					Modifier.ModifierSlotConfigs.Add(SlotConfig);
				}
			}
		}

		return true;
	}

	UE_LOG(LogArcRecipeXml, Warning, TEXT("Unknown output modifier type: %s"), *TypeStr);
	return false;
}
