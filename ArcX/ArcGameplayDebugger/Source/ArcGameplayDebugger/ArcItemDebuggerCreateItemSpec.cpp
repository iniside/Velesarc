#include "ArcItemDebuggerCreateItemSpec.h"

#include "GameplayAbilityBlueprint.h"
#include "SlateIM.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/AssetManager.h"
#include "Items/ArcItemDefinition.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "Items/Fragments/ArcItemFragment_ItemStats.h"
#include "imgui.h"
#include "Items/Fragments/ArcItemFragment_AbilityEffectsToApply.h"
#include "Items/Fragments/ArcItemFragment_GrantedPassiveAbilities.h"

void DrawEditableItemStats(const TArray<FGameplayAttribute>& inAttributes, FArcItemFragment_ItemInstanceBase* InFragment)
{
	if (!InFragment->GetScriptStruct()->IsChildOf(FArcItemFragment_ItemStats::StaticStruct()))
	{
		return;
	}
	
	FArcItemFragment_ItemStats* StatsFragment = static_cast<FArcItemFragment_ItemStats*>(InFragment);

	if (ImGui::BeginCombo("Add Attribute", "Add Attribute"))
	{
		for (int32 FragmentIdx = 0; FragmentIdx < inAttributes.Num(); FragmentIdx++)
		{
			FString AttributeName = FString::Printf(TEXT("%s.%s"), *GetNameSafe(inAttributes[FragmentIdx].GetAttributeSetClass()), *inAttributes[FragmentIdx].AttributeName);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*AttributeName)))
			{
				StatsFragment->DefaultStats.Add(FArcItemAttributeStat(inAttributes[FragmentIdx], 0.0f));
			}
		}
		ImGui::EndCombo();
	}

	for (int32 StatIdx = 0; StatIdx < StatsFragment->DefaultStats.Num(); StatIdx++)
	{
		FArcItemAttributeStat& Stat = StatsFragment->DefaultStats[StatIdx];
		if (ImGui::TreeNode(TCHAR_TO_ANSI(*Stat.Attribute.AttributeName)))
		{
			int32 SelectedAttributeIdx = inAttributes.IndexOfByPredicate([Stat](const FGameplayAttribute& Attr) {
					return Attr == Stat.Attribute;
				});

			FString PreviewValue = "Select Attribute";
			if (SelectedAttributeIdx != INDEX_NONE)
			{
				PreviewValue = FString::Printf(TEXT("%s.%s"), *GetNameSafe(inAttributes[SelectedAttributeIdx].GetAttributeSetClass()), *inAttributes[SelectedAttributeIdx].AttributeName);
			}
			
			if (ImGui::BeginCombo("Select Attribute", TCHAR_TO_ANSI(*PreviewValue)))
			{
				for (int32 AttrIdx = 0; AttrIdx < inAttributes.Num(); AttrIdx++)
				{
					FString AttributeName = FString::Printf(TEXT("%s.%s"), *GetNameSafe(inAttributes[AttrIdx].GetAttributeSetClass()), *inAttributes[AttrIdx].AttributeName);
					if (ImGui::Selectable(TCHAR_TO_ANSI(*AttributeName), SelectedAttributeIdx == AttrIdx))
					{
						Stat.Attribute = inAttributes[AttrIdx];
					}
				}
				ImGui::EndCombo();
			}

			ImGui::InputFloat("##StatValue", &Stat.Value.Value, 0.1f, 1.0f, "%.1f");	
			ImGui::TreePop();
		}
	}
}

void DrawEditableAbilities(const TArray<FAssetData>& AbilitiesAssets, FArcItemFragment_ItemInstanceBase* InFragment)
{
	if (!InFragment->GetScriptStruct()->IsChildOf(FArcItemFragment_GrantedAbilities::StaticStruct()))
	{
		return;
	}
	
	FArcItemFragment_GrantedAbilities* GrantedAbilities = static_cast<FArcItemFragment_GrantedAbilities*>(InFragment);

	if (ImGui::BeginCombo("Add Ability", "Add Ability"))
	{
		for (int32 AbilityIdx = 0; AbilityIdx < AbilitiesAssets.Num(); AbilityIdx++)
		{
			FString AbilityName = FString::Printf(TEXT("%s"), *AbilitiesAssets[AbilityIdx].AssetName.ToString());
			if (ImGui::Selectable(TCHAR_TO_ANSI(*AbilityName)))
			{
				UBlueprint* AssetBP = Cast<UBlueprint>(AbilitiesAssets[AbilityIdx].GetAsset());
				UClass* AbilityClass = AssetBP ? AssetBP->GeneratedClass : nullptr;
				GrantedAbilities->Abilities.Add(FArcAbilityEntry(false, FGameplayTag::EmptyTag,AbilityClass));
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
			//AbilityName.LeftChopInline(2);
			AbilityName.RemoveFromEnd(TEXT("_C"));
			
			int32 SelectedAssetIdx = AbilitiesAssets.IndexOfByPredicate([AbilityName](const FAssetData& AssetData) {
					return AbilityName == AssetData.AssetName.ToString();
				});

			FString PreviewValue = "Select Ability";
			if (SelectedAssetIdx != INDEX_NONE)
			{
				PreviewValue = FString::Printf(TEXT("%s"), *AbilitiesAssets[SelectedAssetIdx].AssetName.ToString());
			}
			
			if (ImGui::BeginCombo("Select Ability", TCHAR_TO_ANSI(*PreviewValue)))
			{
				for (int32 AbilityIdx = 0; AbilityIdx < AbilitiesAssets.Num(); AbilityIdx++)
				{
					FString ListAbilityName = FString::Printf(TEXT("%s"), *AbilitiesAssets[AbilityIdx].AssetName.ToString());
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
}

void DrawGrantedPassiveAbilities(const TArray<FAssetData>& AbilitiesAssets, FArcItemFragment_ItemInstanceBase* InFragment)
{
	if (!InFragment->GetScriptStruct()->IsChildOf(FArcItemFragment_GrantedPassiveAbilities::StaticStruct()))
	{
		return;
	}
	
	FArcItemFragment_GrantedPassiveAbilities* GrantedAbilities = static_cast<FArcItemFragment_GrantedPassiveAbilities*>(InFragment);

	if (ImGui::BeginCombo("Add Ability", "Add Ability"))
	{
		for (int32 AbilityIdx = 0; AbilityIdx < AbilitiesAssets.Num(); AbilityIdx++)
		{
			FString AbilityName = FString::Printf(TEXT("%s"), *AbilitiesAssets[AbilityIdx].AssetName.ToString());
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
			//AbilityName.LeftChopInline(2);
			AbilityName.RemoveFromEnd(TEXT("_C"));
			
			int32 SelectedAssetIdx = AbilitiesAssets.IndexOfByPredicate([AbilityName](const FAssetData& AssetData) {
					return AbilityName == AssetData.AssetName.ToString();
				});

			FString PreviewValue = "Select Ability";
			if (SelectedAssetIdx != INDEX_NONE)
			{
				PreviewValue = FString::Printf(TEXT("%s"), *AbilitiesAssets[SelectedAssetIdx].AssetName.ToString());
			}
			
			if (ImGui::BeginCombo("Select Ability", TCHAR_TO_ANSI(*PreviewValue)))
			{
				for (int32 AbilityIdx = 0; AbilityIdx < AbilitiesAssets.Num(); AbilityIdx++)
				{
					FString ListAbilityName = FString::Printf(TEXT("%s"), *AbilitiesAssets[AbilityIdx].AssetName.ToString());
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
}
#if 0
void DrawAbilityEffectsToApply(const TArray<FAssetData>& AbilitiesAssets, FArcItemFragment_ItemInstanceBase* InFragment)
{
	if (!InFragment->GetScriptStruct()->IsChildOf(FArcItemFragment_AbilityEffectsToApply::StaticStruct()))
	{
		return;
	}
	
	FArcItemFragment_AbilityEffectsToApply* GrantedAbilities = static_cast<FArcItemFragment_AbilityEffectsToApply*>(InFragment);

	if (ImGui::BeginCombo("Add Ability", "Add Ability"))
	{
		for (int32 AbilityIdx = 0; AbilityIdx < AbilitiesAssets.Num(); AbilityIdx++)
		{
			FString AbilityName = FString::Printf(TEXT("%s"), *AbilitiesAssets[AbilityIdx].AssetName.ToString());
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
			//AbilityName.LeftChopInline(2);
			AbilityName.RemoveFromEnd(TEXT("_C"));
			
			int32 SelectedAssetIdx = AbilitiesAssets.IndexOfByPredicate([AbilityName](const FAssetData& AssetData) {
					return AbilityName == AssetData.AssetName.ToString();
				});

			FString PreviewValue = "Select Ability";
			if (SelectedAssetIdx != INDEX_NONE)
			{
				PreviewValue = FString::Printf(TEXT("%s"), *AbilitiesAssets[SelectedAssetIdx].AssetName.ToString());
			}
			
			if (ImGui::BeginCombo("Select Ability", TCHAR_TO_ANSI(*PreviewValue)))
			{
				for (int32 AbilityIdx = 0; AbilityIdx < AbilitiesAssets.Num(); AbilityIdx++)
				{
					FString ListAbilityName = FString::Printf(TEXT("%s"), *AbilitiesAssets[AbilityIdx].AssetName.ToString());
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
}

void DrawGrantedGameplayEffects(const TArray<FAssetData>& AbilitiesAssets, FArcItemFragment_ItemInstanceBase* InFragment)
{
	if (!InFragment->GetScriptStruct()->IsChildOf(FArcItemFragment_GrantedAbilities::StaticStruct()))
	{
		return;
	}
	
	FArcItemFragment_GrantedPassiveAbilities* GrantedAbilities = static_cast<FArcItemFragment_GrantedPassiveAbilities*>(InFragment);

	if (ImGui::BeginCombo("Add Ability", "Add Ability"))
	{
		for (int32 AbilityIdx = 0; AbilityIdx < AbilitiesAssets.Num(); AbilityIdx++)
		{
			FString AbilityName = FString::Printf(TEXT("%s"), *AbilitiesAssets[AbilityIdx].AssetName.ToString());
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
			//AbilityName.LeftChopInline(2);
			AbilityName.RemoveFromEnd(TEXT("_C"));
			
			int32 SelectedAssetIdx = AbilitiesAssets.IndexOfByPredicate([AbilityName](const FAssetData& AssetData) {
					return AbilityName == AssetData.AssetName.ToString();
				});

			FString PreviewValue = "Select Ability";
			if (SelectedAssetIdx != INDEX_NONE)
			{
				PreviewValue = FString::Printf(TEXT("%s"), *AbilitiesAssets[SelectedAssetIdx].AssetName.ToString());
			}
			
			if (ImGui::BeginCombo("Select Ability", TCHAR_TO_ANSI(*PreviewValue)))
			{
				for (int32 AbilityIdx = 0; AbilityIdx < AbilitiesAssets.Num(); AbilityIdx++)
				{
					FString ListAbilityName = FString::Printf(TEXT("%s"), *AbilitiesAssets[AbilityIdx].AssetName.ToString());
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
}
#endif

void FArcItemDebuggerItemSpecCreator::CreateItemSpec(UArcItemsStoreComponent* ItemStore, UArcItemDefinition* ItemDef)
{
	ImGui::Text("Item Spec Creator");

	const FPrimaryAssetId Id = ItemDef->GetPrimaryAssetId();
	
	TempNewSpec.SetItemDefinition(Id);
	
	static ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_ContextMenuInBody;
	//PushStyleCompact();
	//ImGui::CheckboxFlags("ImGuiTableFlags_Resizable", &flags, ImGuiTableFlags_Resizable);
	//ImGui::CheckboxFlags("ImGuiTableFlags_BordersV", &flags, ImGuiTableFlags_BordersV);
	//ImGui::SameLine(); HelpMarker(
	//	"Using the _Resizable flag automatically enables the _BordersInnerV flag as well, "
	//	"this is why the resize borders are still showing when unchecking this.");
	//PopStyleCompact();

	if (ImGui::BeginTable("table1", 2, flags))
	{
		ImGui::TableNextRow();
		if (ImGui::TableSetColumnIndex(0))
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
							void* Allocated = FMemory::Malloc(SelectedFragmentType->GetCppStructOps()->GetSize()
								, SelectedFragmentType->GetCppStructOps()->GetAlignment());
							SelectedFragmentType->GetCppStructOps()->Construct(Allocated);

							FArcItemFragment_ItemInstanceBase* NewFragment = static_cast<FArcItemFragment_ItemInstanceBase*>(Allocated);
							TempNewSpec.AddInstanceData(NewFragment);
						}
					}
				}
				ImGui::EndCombo();
			}		
		}
		
		if (ImGui::TableSetColumnIndex(1))
		{
			FString ItemDef = FString::Printf(TEXT("%s"), *Id.ToString());
			ImGui::Text(TCHAR_TO_ANSI(*ItemDef));
			// Granted Abilities
			// Granted Passive Abilities
			// Granted Effects
			// Grant Attributes
			// Ability Effects To Apply
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
		}
		
		ImGui::EndTable();
	}
	
}

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

	// Gods know, how many blueprints we have vs gameplay effects.
	GameplayEffectsAssets.Reserve(2048);

	FString EffectClassPath = UGameplayEffect::StaticClass()->GetClassPathName().ToString();
	FString EffectExportPath =FObjectPropertyBase::GetExportPath(UGameplayEffect::StaticClass());
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

	TempNewSpec = FArcItemSpec();
}
