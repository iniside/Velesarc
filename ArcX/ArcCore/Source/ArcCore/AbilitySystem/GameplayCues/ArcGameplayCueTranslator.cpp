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

#include "ArcCore/AbilitySystem/GameplayCues/ArcGameplayCueTranslator.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "AbilitySystem/ArcGameplayCueManager.h"
#include "ArcCore/AbilitySystem/GameplayCues/ArcCueTranslatorSettings.h"
#include "ArcCueTranslatorTypes.h"
#include "ArcPhysicalMaterialBase.h"
#include "Items/ArcItemDefinition.h"

namespace Cues
{
	struct TreeItem
	{
		FString Name;
		TreeItem* Parent = nullptr;
		TArray<TreeItem> Children;
	};

	void fuc(int32 NextIdx
			 , struct TreeItem* Parent
			 , const TArray<FArcTranslationUnit>& Units
			 , TArray<int32> UnitTranslations)
	{
		for (const FName& To : Units[UnitTranslations[NextIdx]].To)
		{
			TreeItem Item;
			Item.Name = To.ToString();
			Item.Parent = Parent;
			int32 Idx = Parent->Children.Add(Item);
			int32 NewIdx = NextIdx + 1;
			if (NewIdx < UnitTranslations.Num())
			{
				fuc(NewIdx
					, &Parent->Children[Idx]
					, Units
					, UnitTranslations);
			}
		}
	}

	void Combine(const TreeItem& Root
				 , TArray<FName>& Out
				 , FString& Combined)
	{
		bool bUseSeparator = Root.Children.Num() > 0;
		FString Separator = bUseSeparator ? "-" : "";
		FString CopyCombined = Combined + Root.Name + Separator;
		for (const TreeItem& TI : Root.Children)
		{
			Combine(TI
				, Out
				, CopyCombined);
		}
		if (Root.Children.Num() == 0)
		{
			Out.Add(*CopyCombined);
		}
	}
} // namespace Cues

void UArcGameplayCueTranslator::GetTranslationNameSpawns(TArray<FGameplayCueTranslationNameSwap>& SwapList) const
{
	const TArray<FArcTranslatorUnit>& Translators = GetDefault<UArcCueTranslatorSettings>()->TranslatorUnit;
	const TArray<FArcTranslationUnit>& Units = GetDefault<UArcCueTranslatorSettings>()->TranslationUnits;

	for (const FArcTranslatorUnit& T : Translators)
	{
		UClass* C = T.TranslatorClass.LoadSynchronous();
		if (C == GetClass())
		{
			TArray<FName> ToUnits;

			TArray<int32> UnitTranslations;
			for (int32 Idx = 0; Idx < T.Units.Num(); Idx++)
			{
				for (int32 Udx = 0; Udx < Units.Num(); Udx++)
				{
					if (Units[Udx].From.ToString() == T.Units[Idx])
					{
						UnitTranslations.Add(Udx);
					}
				}
			}

			int32 AllocationSize = 1;
			for (int32 I : UnitTranslations)
			{
				AllocationSize *= Units[I].To.Num();
			}

			using namespace Cues;
			TArray<TreeItem> Roots;

			// make roots
			for (const FName& To : Units[UnitTranslations[0]].To)
			{
				TreeItem Item;
				Item.Name = To.ToString();
				Roots.Add(Item);
			}

			for (TreeItem& I : Roots)
			{
				fuc(1
					, &I
					, Units
					, UnitTranslations);
			}

			TArray<FName> CombinedNames;
			for (const TreeItem& TI : Roots)
			{
				FString Comb;
				Combine(TI
					, CombinedNames
					, Comb);
			}
			for (int32 Idx = 0; Idx < CombinedNames.Num(); Idx++)
			{
				FName To = CombinedNames[Idx];
				FGameplayCueTranslationNameSwap Swap;
				Swap.FromName = T.GetCombinedUnit();
				Swap.ToNames.Add(To);
#if WITH_EDITOR
				Swap.EditorData.Enabled = true;
				Swap.EditorData.EditorDescription = Swap.FromName;
				Swap.EditorData.ToolTip = Swap.FromName.ToString();
				Swap.EditorData.UniqueID = 0;
#endif

				int32 ItemIdx = SwapList.Add(Swap);
				TranslationIndexes.FindOrAdd(To) = ItemIdx;
			}
		}
	}
}

int32 UArcGameplayCueTranslator_WeaponSurface::GameplayCueToTranslationIndex(const FName& TagName
																			 , AActor* TargetActor
																			 , const FGameplayCueParameters& Parameters) const
{
	if (TagName.IsValid())
	{
		UPhysicalMaterial* PM = Parameters.EffectContext.GetHitResult()->PhysMaterial.Get();
		FString PhysMat;
		if (UArcPhysicalMaterialBase* ArcPM = Cast<UArcPhysicalMaterialBase>(PM))
		{
			PhysMat = ArcPM->GetMaterial();
		}
		// gone wild!
		TArray<FString> Parsed;
		TagName.ToString().ParseIntoArray(Parsed
			, TEXT("."));

		FString Combo = Parsed.Last();
		const UArcCoreGameplayAbility* ArcGA = Cast<UArcCoreGameplayAbility>(
			Parameters.EffectContext.GetAbilityInstance_NotReplicated());

		const UArcItemDefinition* ID = ArcGA->GetSourceItemData();

		FString FullName;
		if (ID)
		{
			const FArcItemFragment_CueName* CueName = ID->FindFragment<FArcItemFragment_CueName>();
			if (CueName)
			{
				FullName = CueName->CueName + "-" + PhysMat;
			}
		}

		int32* Idx = TranslationIndexes.Find(*FullName);
		if (Idx)
		{
			return *Idx;
		}

		// const UArcItemDefinition* Item =
		// Cast<UArcItemDefinition>(Parameters.GetSourceObject());

		// Ok. Now need to split by -
		// somehow match results with TargetActor/Params
		// how ?
		// Weapon-Surface
		// Weapon from Params
		// Surface from Parameters (Physical Material).
	}

	return INDEX_NONE;
}
