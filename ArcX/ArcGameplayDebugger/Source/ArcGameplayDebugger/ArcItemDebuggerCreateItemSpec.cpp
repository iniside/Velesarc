#include "ArcItemDebuggerCreateItemSpec.h"

#include "GameplayAbilityBlueprint.h"
#include "SlateIM.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/AssetManager.h"
#include "Items/ArcItemDefinition.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "Items/Fragments/ArcItemFragment_ItemStats.h"

void FArcItemDebuggerCreateItemSpec::Initialize()
{
	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	
	IAssetRegistry& AR = AssetManager->GetAssetRegistry();

	AR.GetAssetsByClass(UArcItemDefinition::StaticClass()->GetClassPathName(), ItemDefinitions);

	for (const FAssetData& ItemData : ItemDefinitions)
	{
		if (ItemData.IsValid())
		{
			ItemList.Add(ItemData.AssetName.ToString());	
		}
	}

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
				Attributes.Add(FString::Printf(TEXT("%s.%s"), *It->GetClass()->GetName(), *PropIt->GetName()));
				GameplayAttributes.Add(FGameplayAttribute(*PropIt));
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
}

void FArcItemDebuggerCreateItemSpec::Uninitialize()
{
	TempNewSpec = FArcItemSpec();
	ItemMakerFilterText.Reset();
	
	ItemDefinitions.Reset();
	ItemList.Reset();
	FragmentInstanceNames.Reset();
	FragmentInstanceTypes.Reset();
	Attributes.Reset();
	GameplayAttributes.Reset();
	GameplayAbilitiesAssets.Reset();
	GameplayAbilitiesNames.Reset();
}

void FArcItemDebuggerCreateItemSpec::Draw()
{
	SlateIM::HAlign(HAlign_Fill);
	SlateIM::VAlign(VAlign_Fill);
	SlateIM::MaxHeight(500.f);
	SlateIM::BeginHorizontalStack();

	TArray<FAssetData> FilteredItemDefinitions;
	FilteredItemDefinitions.Reserve(ItemDefinitions.Num());
	
	TArray<FString> FilteredItemList;
	FilteredItemList.Reserve(ItemDefinitions.Num());

	
	SlateIM::BeginVerticalStack();
		bool bForceRefresh = false;
		if (SlateIM::Button(TEXT("Refresh Item Definitions")))
		{
			if (ItemMakerFilterText.Len() > 0)
			{
				for (int32 Idx = 0; Idx < ItemDefinitions.Num(); Idx++)
				{
					if (ItemList[Idx].Contains(ItemMakerFilterText))
					{
						FilteredItemList.Add(ItemList[Idx]);
						FilteredItemDefinitions.Add(ItemDefinitions[Idx]);
					}
				}
			}
			bForceRefresh = true;
		}
		if (ItemMakerFilterText.Len() == 0)
		{
			FilteredItemList.Append(ItemList);
			FilteredItemDefinitions.Append(ItemDefinitions);
		}
	      	
		SlateIM::EditableText(ItemMakerFilterText);
		
		int32 SelectedItemDefIdx = -1;
		
		SlateIM::SelectionList(FilteredItemList, SelectedItemDefIdx, bForceRefresh);
	SlateIM::EndVerticalStack();

	SlateIM::BeginVerticalStack();

	SlateIM::EndVerticalStack();

	FArcItemFragment_ItemStats* ItemStats = TempNewSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();

	SlateIM::BeginTable();
		SlateIM::AddTableColumn(TEXT(""));
		if (SlateIM::NextTableCell())
		{
			SlateIM::Text(TEXT("Add Item Fragments"));
			if (SlateIM::Button(TEXT("Reset")))
			{
				TempNewSpec = FArcItemSpec();
			}

			int32 SelectedFragmentInstanceIdx = -1;
			if (SlateIM::ComboBox(FragmentInstanceNames, SelectedFragmentInstanceIdx))
			{
				UScriptStruct* SelectedFragmentType = FragmentInstanceTypes[SelectedFragmentInstanceIdx];
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
		if (SlateIM::NextTableCell())
		{
			SlateIM::Text(TEXT("Instance Fragments"));
		}
		if (SlateIM::BeginTableRowChildren())
		{
			TArray<const FArcItemFragment_ItemInstanceBase*> Fragments = TempNewSpec.GetSpecFragments();

			
			for (const FArcItemFragment_ItemInstanceBase* Fragment : Fragments)
			{
				if (!Fragment)
				{
					continue;
				}
				
				if (SlateIM::NextTableCell())
				{
					FString Name = Fragment->GetScriptStruct()->GetName();
					SlateIM::Text(Name);
				}
				if (SlateIM::BeginTableRowChildren())
				{
					if (Fragment->GetScriptStruct()->IsChildOf(FArcItemFragment_ItemStats::StaticStruct()))
					{
						FArcItemFragment_ItemStats* StatsFragment = const_cast<FArcItemFragment_ItemStats*>(static_cast<const FArcItemFragment_ItemStats*>(Fragment));
						TArray<FArcItemAttributeStat>& DefaultStats = StatsFragment->DefaultStats;
						
						if (SlateIM::NextTableCell())
						{
							int32 SelectedAttributeIdx = -1;
							if (SlateIM::ComboBox(Attributes, SelectedAttributeIdx))
							{
								FArcItemAttributeStat NewStat;
								NewStat.Attribute = GameplayAttributes[SelectedAttributeIdx];
								StatsFragment->DefaultStats.Add(NewStat);
							}
						}
						
						if (SlateIM::NextTableCell())
						{
							SlateIM::Text(TEXT("Default Stats"));
						}
						if (SlateIM::BeginTableRowChildren())
						{
							for (FArcItemAttributeStat& Stat : DefaultStats)
							{
								if (SlateIM::NextTableCell())
								{
									SlateIM::Text(Stat.Attribute.AttributeName);
									SlateIM::SpinBox(Stat.Value.Value, 0.f, 100.f);
								}
							}
						}
						SlateIM::EndTableRowChildren();
					}
					else if (Fragment->GetScriptStruct()->IsChildOf(FArcItemFragment_GrantedAbilities::StaticStruct()))
					{
						FArcItemFragment_GrantedAbilities* GrantedAbilities = const_cast<FArcItemFragment_GrantedAbilities*>(static_cast<const FArcItemFragment_GrantedAbilities*>(Fragment));
						TArray<FArcAbilityEntry>& DefaultAbilities = GrantedAbilities->Abilities;

						
						if (SlateIM::NextTableCell())
						{
							int32 SelectedAttributeIdx = -1;
							if (SlateIM::ComboBox(GameplayAbilitiesNames, SelectedAttributeIdx))
							{
								FArcAbilityEntry Ability;
								UBlueprint* AbilityBP = Cast<UBlueprint>(GameplayAbilitiesAssets[SelectedAttributeIdx].GetAsset());
								Ability.GrantedAbility = AbilityBP->GeneratedClass;
								
								GrantedAbilities->Abilities.Add(Ability);
							}
						}
						
						if (SlateIM::NextTableCell())
						{
							SlateIM::Text(TEXT("Granted Abilities"));
						}
						if (SlateIM::BeginTableRowChildren())
						{
							for (FArcAbilityEntry& Ability : DefaultAbilities)
							{
								if (!Ability.GrantedAbility)
								{
									SlateIM::Text(TEXT("Invalid Ability"));
									continue;
								}
								
								if (SlateIM::NextTableCell())
								{
									SlateIM::Text(GetNameSafe(Ability.GrantedAbility->GetDefaultObject()));
								}
							}
						}
						SlateIM::EndTableRowChildren();
					}
				}
				SlateIM::EndTableRowChildren();
			}
		}
		SlateIM::EndTableRowChildren();
	
	SlateIM::EndTable();
	
	if (SlateIM::Button(TEXT("Make Item")))
	{
		FArcItemSpec NewSpec;	
	}
	
	SlateIM::EndHorizontalStack();
}
