// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcItemDebuggerCreateItemSpec.h"

#include "GameplayAbilityBlueprint.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/AssetManager.h"
#include "Items/ArcItemDefinition.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "Items/Fragments/ArcItemFragment_ItemStats.h"
#include "Items/Fragments/ArcItemFragment_AbilityEffectsToApply.h"
#include "Items/Fragments/ArcItemFragment_GrantedGameplayEffects.h"
#include "Items/Fragments/ArcItemFragment_GrantedPassiveAbilities.h"
#include "UObject/UObjectIterator.h"
#include "imgui.h"

// ---------------------------------------------------------------------------
// Static draw helpers for known fragment types
// ---------------------------------------------------------------------------

static void DrawEditableItemStats(const TArray<FGameplayAttribute>& InAttributes, FArcItemFragment_ItemInstanceBase* InFragment)
{
	if (!InFragment->GetScriptStruct()->IsChildOf(FArcItemFragment_ItemStats::StaticStruct()))
	{
		return;
	}

	FArcItemFragment_ItemStats* StatsFragment = static_cast<FArcItemFragment_ItemStats*>(InFragment);
	if (ImGui::TreeNode("Item Stats"))
	{
		if (ImGui::BeginCombo("Add Attribute", "Add Attribute"))
		{
			for (int32 FragmentIdx = 0; FragmentIdx < InAttributes.Num(); FragmentIdx++)
			{
				FString AttributeName = FString::Printf(TEXT("%s.%s"), *GetNameSafe(InAttributes[FragmentIdx].GetAttributeSetClass()), *InAttributes[FragmentIdx].AttributeName);
				if (ImGui::Selectable(TCHAR_TO_ANSI(*AttributeName)))
				{
					StatsFragment->DefaultStats.Add(FArcItemAttributeStat(InAttributes[FragmentIdx], 0.0f));
				}
			}
			ImGui::EndCombo();
		}

		for (int32 StatIdx = 0; StatIdx < StatsFragment->DefaultStats.Num(); StatIdx++)
		{
			FArcItemAttributeStat& Stat = StatsFragment->DefaultStats[StatIdx];
			if (ImGui::TreeNode(TCHAR_TO_ANSI(*Stat.Attribute.AttributeName)))
			{
				int32 SelectedAttributeIdx = InAttributes.IndexOfByPredicate([Stat](const FGameplayAttribute& Attr) {
					return Attr == Stat.Attribute;
				});

				FString PreviewValue = "Select Attribute";
				if (SelectedAttributeIdx != INDEX_NONE)
				{
					PreviewValue = FString::Printf(TEXT("%s.%s"), *GetNameSafe(InAttributes[SelectedAttributeIdx].GetAttributeSetClass()), *InAttributes[SelectedAttributeIdx].AttributeName);
				}

				if (ImGui::BeginCombo("Select Attribute", TCHAR_TO_ANSI(*PreviewValue)))
				{
					for (int32 AttrIdx = 0; AttrIdx < InAttributes.Num(); AttrIdx++)
					{
						FString AttributeName = FString::Printf(TEXT("%s.%s"), *GetNameSafe(InAttributes[AttrIdx].GetAttributeSetClass()), *InAttributes[AttrIdx].AttributeName);
						if (ImGui::Selectable(TCHAR_TO_ANSI(*AttributeName), SelectedAttributeIdx == AttrIdx))
						{
							Stat.Attribute = InAttributes[AttrIdx];
						}
					}
					ImGui::EndCombo();
				}

				ImGui::InputFloat("##StatValue", &Stat.Value.Value, 0.1f, 1.0f, "%.1f");
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
}

static void DrawEditableAbilities(const TArray<FAssetData>& AbilitiesAssets, FArcItemFragment_ItemInstanceBase* InFragment)
{
	if (!InFragment->GetScriptStruct()->IsChildOf(FArcItemFragment_GrantedAbilities::StaticStruct()))
	{
		return;
	}

	FArcItemFragment_GrantedAbilities* GrantedAbilities = static_cast<FArcItemFragment_GrantedAbilities*>(InFragment);

	if (ImGui::TreeNode("Activatable Abilities"))
	{
		if (ImGui::BeginCombo("Add Ability", "Add Ability"))
		{
			for (int32 AbilityIdx = 0; AbilityIdx < AbilitiesAssets.Num(); AbilityIdx++)
			{
				FString AbilityName = AbilitiesAssets[AbilityIdx].AssetName.ToString();
				if (ImGui::Selectable(TCHAR_TO_ANSI(*AbilityName)))
				{
					UBlueprint* AssetBP = Cast<UBlueprint>(AbilitiesAssets[AbilityIdx].GetAsset());
					UClass* AbilityClass = AssetBP ? AssetBP->GeneratedClass : nullptr;
					GrantedAbilities->Abilities.Add(FArcAbilityEntry(false, FGameplayTag::EmptyTag, AbilityClass));
				}
			}
			ImGui::EndCombo();
		}

		for (int32 StatIdx = 0; StatIdx < GrantedAbilities->Abilities.Num(); StatIdx++)
		{
			FArcAbilityEntry& AbilityEntry = GrantedAbilities->Abilities[StatIdx];
			if (ImGui::TreeNode(TCHAR_TO_ANSI(*AbilityEntry.GrantedAbility->GetName())))
			{
				FString AbilityName = AbilityEntry.GrantedAbility->GetName();
				AbilityName.RemoveFromEnd(TEXT("_C"));

				int32 SelectedAssetIdx = AbilitiesAssets.IndexOfByPredicate([AbilityName](const FAssetData& AssetData) {
					return AbilityName == AssetData.AssetName.ToString();
				});

				FString PreviewValue = "Select Ability";
				if (SelectedAssetIdx != INDEX_NONE)
				{
					PreviewValue = AbilitiesAssets[SelectedAssetIdx].AssetName.ToString();
				}

				if (ImGui::BeginCombo("Select Ability", TCHAR_TO_ANSI(*PreviewValue)))
				{
					for (int32 AbilityIdx = 0; AbilityIdx < AbilitiesAssets.Num(); AbilityIdx++)
					{
						FString ListAbilityName = AbilitiesAssets[AbilityIdx].AssetName.ToString();
						if (ImGui::Selectable(TCHAR_TO_ANSI(*ListAbilityName)))
						{
							UBlueprint* AssetBP = Cast<UBlueprint>(AbilitiesAssets[AbilityIdx].GetAsset());
							UClass* AbilityClass = AssetBP ? AssetBP->GeneratedClass : nullptr;
							AbilityEntry.GrantedAbility = AbilityClass;
						}
					}
					ImGui::EndCombo();
				}

				ImGui::Checkbox("##AddInputTag", &AbilityEntry.bAddInputTag);
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
}

static void DrawGrantedPassiveAbilities(const TArray<FAssetData>& AbilitiesAssets, FArcItemFragment_ItemInstanceBase* InFragment)
{
	if (!InFragment->GetScriptStruct()->IsChildOf(FArcItemFragment_GrantedPassiveAbilities::StaticStruct()))
	{
		return;
	}

	FArcItemFragment_GrantedPassiveAbilities* GrantedAbilities = static_cast<FArcItemFragment_GrantedPassiveAbilities*>(InFragment);

	if (ImGui::TreeNode("Passive Abilities"))
	{
		if (ImGui::BeginCombo("Add Ability", "Add Ability"))
		{
			for (int32 AbilityIdx = 0; AbilityIdx < AbilitiesAssets.Num(); AbilityIdx++)
			{
				FString AbilityName = AbilitiesAssets[AbilityIdx].AssetName.ToString();
				if (ImGui::Selectable(TCHAR_TO_ANSI(*AbilityName)))
				{
					UBlueprint* AssetBP = Cast<UBlueprint>(AbilitiesAssets[AbilityIdx].GetAsset());
					UClass* AbilityClass = AssetBP ? AssetBP->GeneratedClass : nullptr;
					GrantedAbilities->Abilities.Add(AbilityClass);
				}
			}
			ImGui::EndCombo();
		}

		for (int32 StatIdx = 0; StatIdx < GrantedAbilities->Abilities.Num(); StatIdx++)
		{
			TSubclassOf<UGameplayAbility>& AbilityEntry = GrantedAbilities->Abilities[StatIdx];
			if (ImGui::TreeNode(TCHAR_TO_ANSI(*AbilityEntry->GetName())))
			{
				FString AbilityName = AbilityEntry->GetName();
				AbilityName.RemoveFromEnd(TEXT("_C"));

				int32 SelectedAssetIdx = AbilitiesAssets.IndexOfByPredicate([AbilityName](const FAssetData& AssetData) {
					return AbilityName == AssetData.AssetName.ToString();
				});

				FString PreviewValue = "Select Ability";
				if (SelectedAssetIdx != INDEX_NONE)
				{
					PreviewValue = AbilitiesAssets[SelectedAssetIdx].AssetName.ToString();
				}

				if (ImGui::BeginCombo("Select Ability", TCHAR_TO_ANSI(*PreviewValue)))
				{
					for (int32 AbilityIdx = 0; AbilityIdx < AbilitiesAssets.Num(); AbilityIdx++)
					{
						FString ListAbilityName = AbilitiesAssets[AbilityIdx].AssetName.ToString();
						if (ImGui::Selectable(TCHAR_TO_ANSI(*ListAbilityName)))
						{
							UBlueprint* AssetBP = Cast<UBlueprint>(AbilitiesAssets[AbilityIdx].GetAsset());
							UClass* AbilityClass = AssetBP ? AssetBP->GeneratedClass : nullptr;
							AbilityEntry = AbilityClass;
						}
					}
					ImGui::EndCombo();
				}

				ImGui::TreePop();
			}
		}

		ImGui::TreePop();
	}
}

static void DrawGrantedGameplayEffects(const TArray<FAssetData>& EffectAssets, FArcItemFragment_ItemInstanceBase* InFragment)
{
	if (!InFragment->GetScriptStruct()->IsChildOf(FArcItemFragment_GrantedGameplayEffects::StaticStruct()))
	{
		return;
	}

	FArcItemFragment_GrantedGameplayEffects* GrantedGameplayEffects = static_cast<FArcItemFragment_GrantedGameplayEffects*>(InFragment);

	if (ImGui::TreeNode("Granted Effects"))
	{
		if (ImGui::BeginCombo("Add Effect", "Add Effect"))
		{
			for (int32 AbilityIdx = 0; AbilityIdx < EffectAssets.Num(); AbilityIdx++)
			{
				FString AbilityName = EffectAssets[AbilityIdx].AssetName.ToString();
				if (ImGui::Selectable(TCHAR_TO_ANSI(*AbilityName)))
				{
					UBlueprint* AssetBP = Cast<UBlueprint>(EffectAssets[AbilityIdx].GetAsset());
					UClass* EffectClass = AssetBP ? AssetBP->GeneratedClass : nullptr;
					GrantedGameplayEffects->Effects.Add(EffectClass);
				}
			}
			ImGui::EndCombo();
		}

		for (int32 StatIdx = 0; StatIdx < GrantedGameplayEffects->Effects.Num(); StatIdx++)
		{
			TSubclassOf<UGameplayEffect>& EffectEntry = GrantedGameplayEffects->Effects[StatIdx];
			if (ImGui::TreeNode(TCHAR_TO_ANSI(*EffectEntry->GetName())))
			{
				FString AbilityName = EffectEntry->GetName();
				AbilityName.RemoveFromEnd(TEXT("_C"));

				int32 SelectedAssetIdx = EffectAssets.IndexOfByPredicate([AbilityName](const FAssetData& AssetData) {
					return AbilityName == AssetData.AssetName.ToString();
				});

				FString PreviewValue = "Select Effect";
				if (SelectedAssetIdx != INDEX_NONE)
				{
					PreviewValue = EffectAssets[SelectedAssetIdx].AssetName.ToString();
				}

				if (ImGui::BeginCombo("Select Effect", TCHAR_TO_ANSI(*PreviewValue)))
				{
					for (int32 AbilityIdx = 0; AbilityIdx < EffectAssets.Num(); AbilityIdx++)
					{
						FString ListAbilityName = EffectAssets[AbilityIdx].AssetName.ToString();
						if (ImGui::Selectable(TCHAR_TO_ANSI(*ListAbilityName)))
						{
							UBlueprint* AssetBP = Cast<UBlueprint>(EffectAssets[AbilityIdx].GetAsset());
							UClass* AbilityClass = AssetBP ? AssetBP->GeneratedClass : nullptr;
							EffectEntry = AbilityClass;
						}
					}
					ImGui::EndCombo();
				}

				ImGui::TreePop();
			}
		}

		ImGui::TreePop();
	}
}

// ---------------------------------------------------------------------------
// FArcItemDebuggerItemSpecCreator
// ---------------------------------------------------------------------------

void FArcItemDebuggerItemSpecCreator::Initialize()
{
	TempNewSpec = FArcItemSpec();

	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	IAssetRegistry& AR = AssetManager->GetAssetRegistry();

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		if (It->IsChildOf(FArcItemFragment_ItemInstanceBase::StaticStruct()))
		{
			FragmentInstanceNames.Add(It->GetName());
			FragmentInstanceTypes.Add(*It);
		}
	}

	for (TObjectIterator<UAttributeSet> It; It; ++It)
	{
		for (TFieldIterator<FStructProperty> PropIt(It->GetClass()); PropIt; ++PropIt)
		{
			if (PropIt->Struct->IsChildOf(FGameplayAttributeData::StaticStruct()))
			{
				Attributes.AddUnique(FString::Printf(TEXT("%s.%s"), *It->GetClass()->GetName(), *PropIt->GetName()));
				GameplayAttributes.AddUnique(FGameplayAttribute(*PropIt));
			}
		}
	}

	AR.GetAssetsByClass(UGameplayAbilityBlueprint::StaticClass()->GetClassPathName(), GameplayAbilitiesAssets);

	GameplayAbilitiesNames.Reserve(GameplayAbilitiesAssets.Num());
	for (const FAssetData& AbilityAsset : GameplayAbilitiesAssets)
	{
		if (AbilityAsset.IsValid())
		{
			GameplayAbilitiesNames.Add(AbilityAsset.AssetName.ToString());
		}
	}

	TArray<FAssetData> GameplayEffectsAssetsUnfiltered;
	AR.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), GameplayEffectsAssetsUnfiltered);

	GameplayEffectsAssets.Reserve(2048);

	FString EffectExportPath = FObjectPropertyBase::GetExportPath(UGameplayEffect::StaticClass());
	for (const FAssetData& AbilityAsset : GameplayEffectsAssetsUnfiltered)
	{
		if (AbilityAsset.FindTag(FBlueprintTags::NativeParentClassPath))
		{
			FString ParentClassPath;
			AbilityAsset.GetTagValue(FBlueprintTags::NativeParentClassPath, ParentClassPath);

			if (ParentClassPath == EffectExportPath)
			{
				GameplayEffectsAssets.Add(AbilityAsset);
			}
		}
	}
}

void FArcItemDebuggerItemSpecCreator::Uninitialize()
{
	FragmentInstanceNames.Reset();
	FragmentInstanceTypes.Reset();
	Attributes.Reset();
	GameplayAttributes.Reset();
	GameplayAbilitiesAssets.Reset();
	GameplayAbilitiesNames.Reset();
	GameplayEffectsAssets.Reset();

	TempNewSpec = FArcItemSpec();
	CurrentDefinition.Reset();
	bFinished = false;
	bCancelled = false;
}

void FArcItemDebuggerItemSpecCreator::Begin(UArcItemDefinition* ItemDef)
{
	CurrentDefinition = ItemDef;
	bFinished = false;
	bCancelled = false;

	if (ItemDef)
	{
		TempNewSpec = FArcItemSpec::NewItem(ItemDef, 1, 1);
	}
	else
	{
		TempNewSpec = FArcItemSpec();
	}
}

void FArcItemDebuggerItemSpecCreator::Reset()
{
	bFinished = false;
	bCancelled = false;
	TempNewSpec = FArcItemSpec();
	CurrentDefinition.Reset();
}

void FArcItemDebuggerItemSpecCreator::Draw()
{
	if (!CurrentDefinition.IsValid())
	{
		ImGui::TextDisabled("No item definition selected");
		return;
	}

	UArcItemDefinition* ItemDef = CurrentDefinition.Get();
	const FPrimaryAssetId Id = ItemDef->GetPrimaryAssetId();

	ImGui::Text("Definition: %s", TCHAR_TO_ANSI(*Id.ToString()));
	ImGui::Separator();

	// Basic spec properties
	DrawSpecProperties();

	ImGui::Spacing();

	// Fragment instances
	DrawFragmentInstances();

	ImGui::Spacing();
	ImGui::Separator();

	// Confirm / Cancel buttons
	if (ImGui::Button("Confirm"))
	{
		bFinished = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
	{
		bCancelled = true;
	}
}

void FArcItemDebuggerItemSpecCreator::DrawSpecProperties()
{
	// Amount
	int Amount = TempNewSpec.Amount;
	ImGui::SetNextItemWidth(120.f);
	if (ImGui::InputInt("Amount", &Amount, 1, 10))
	{
		TempNewSpec.Amount = static_cast<uint16>(FMath::Max(1, Amount));
	}

	// Level
	int Level = TempNewSpec.Level;
	ImGui::SetNextItemWidth(120.f);
	if (ImGui::InputInt("Level", &Level, 1, 5))
	{
		TempNewSpec.Level = static_cast<uint8>(FMath::Clamp(Level, 1, 255));
	}
}

void FArcItemDebuggerItemSpecCreator::DrawFragmentInstances()
{
	if (ImGui::BeginCombo("Add Fragment Instance", "Select Fragment Instance"))
	{
		for (int32 FragmentIdx = 0; FragmentIdx < FragmentInstanceNames.Num(); FragmentIdx++)
		{
			if (ImGui::Selectable(TCHAR_TO_ANSI(*FragmentInstanceNames[FragmentIdx])))
			{
				UScriptStruct* SelectedFragmentType = FragmentInstanceTypes[FragmentIdx];
				const uint8* ExistingFragment = TempNewSpec.FindFragment(SelectedFragmentType);
				if (!ExistingFragment)
				{
					void* Allocated = FMemory::Malloc(SelectedFragmentType->GetCppStructOps()->GetSize(),
						SelectedFragmentType->GetCppStructOps()->GetAlignment());
					SelectedFragmentType->GetCppStructOps()->Construct(Allocated);

					FArcItemFragment_ItemInstanceBase* NewFragment = static_cast<FArcItemFragment_ItemInstanceBase*>(Allocated);
					TempNewSpec.AddInstanceData(NewFragment);
				}
			}
		}
		ImGui::EndCombo();
	}

	// Draw editors for known fragment types
	if (FArcItemFragment_ItemStats* ItemStats = TempNewSpec.FindFragmentMutable<FArcItemFragment_ItemStats>())
	{
		DrawEditableItemStats(GameplayAttributes, ItemStats);
	}
	if (FArcItemFragment_GrantedAbilities* GrantedAbilities = TempNewSpec.FindFragmentMutable<FArcItemFragment_GrantedAbilities>())
	{
		DrawEditableAbilities(GameplayAbilitiesAssets, GrantedAbilities);
	}
	if (FArcItemFragment_GrantedPassiveAbilities* GrantedAbilities = TempNewSpec.FindFragmentMutable<FArcItemFragment_GrantedPassiveAbilities>())
	{
		DrawGrantedPassiveAbilities(GameplayAbilitiesAssets, GrantedAbilities);
	}
	if (FArcItemFragment_GrantedGameplayEffects* GrantedGameplayEffects = TempNewSpec.FindFragmentMutable<FArcItemFragment_GrantedGameplayEffects>())
	{
		DrawGrantedGameplayEffects(GameplayEffectsAssets, GrantedGameplayEffects);
	}
}
