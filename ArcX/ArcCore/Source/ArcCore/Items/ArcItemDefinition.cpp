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



#include "Items/ArcItemDefinition.h"

#include "ArcItemScalableFloat.h"
#include "GameplayTagsManager.h"
#include "Engine/AssetManager.h"
#include "UObject/Object.h"
#include "UObject/UObjectBase.h"

#include "Items/ArcItemInstance.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "AssetRegistry/AssetData.h"


UArcItemDefinition::UArcItemDefinition()
{
	ItemType = TEXT("ArcItem");
	for (FArcInstancedStruct& IS : ScalableFloatFragmentSet)
	{
		IS.GetMutablePtr<FArcScalableFloatItemFragment>()->Initialize(IS.GetScriptStruct());
	}
}

#if WITH_EDITOR
void UArcItemDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	if(PropertyChangedEvent.Property->GetFName() == TEXT("ItemType")
		|| PropertyChangedEvent.Property->GetFName() == TEXT("ItemClass"))
	{
		UAssetManager::Get().RefreshAssetData(this);
	}

	if(PropertyChangedEvent.Property && PropertyChangedEvent.MemberProperty &&
		PropertyChangedEvent.Property->GetFName() == "InstancedStruct" &&
		PropertyChangedEvent.MemberProperty->GetFName() == "EditorFragmentSet")
	{
		for (FArcInstancedStruct& IS : EditorFragmentSet)
		{
			IS.PreSave();
		}
	}

	if(PropertyChangedEvent.Property && PropertyChangedEvent.MemberProperty &&
		PropertyChangedEvent.Property->GetFName() == "InstancedStruct" &&
		PropertyChangedEvent.MemberProperty->GetFName() == "FragmentSet")
	{
		for (FArcInstancedStruct& IS : FragmentSet)
		{
			IS.PreSave();
		}
	}
}

EDataValidationResult UArcItemDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	for (const FArcInstancedStruct& IS : FragmentSet)
	{
		if (IS.IsValid())
		{
			EDataValidationResult NewResult = IS.GetPtr<FArcItemFragment>()->IsDataValid(this, Context);

			// Only change result if it is invalid. We will change to Valid at the end of this function.
			if (Result != EDataValidationResult::Invalid)
			{
				Result = NewResult;
			}
		}
	}

	for (const FArcInstancedStruct& IS : EditorFragmentSet)
	{
		if (IS.IsValid())
		{
			IS.GetPtr<FArcItemFragment>()->IsDataValid(this, Context);
			EDataValidationResult NewResult = IS.GetPtr<FArcItemFragment>()->IsDataValid(this, Context);
			if (Result != EDataValidationResult::Invalid)
			{
				Result = NewResult;
			}
		}
	}

	// If it was not Invalid, change it to Valid, since we are now Validated.
	if (Result != EDataValidationResult::Invalid)
	{
		Result = EDataValidationResult::Valid;
	}
	return Result;
}

void UArcItemDefinition::UpdateAssetBundleData()
{
	if (UAssetManager::IsInitialized())
	{
		AssetBundleData.Reset();
		UAssetManager::Get().InitializeAssetBundlesFromMetadata(this, AssetBundleData);

		for (FArcInstancedStruct& IS : FragmentSet)
		{
			if (IS.IsValid() == false)
			{
				continue;
			}
			
			UAssetManager::Get().InitializeAssetBundlesFromMetadata(IS.GetScriptStruct(), IS.GetMemory(), AssetBundleData, IS.GetScriptStruct()->GetFName());
		}
	}
}
#endif

void UArcItemDefinition::PostInitProperties()
{
	Super::PostInitProperties();
}

void UArcItemDefinition::PreSave(FObjectPreSaveContext SaveContext)
{
	if (SaveContext.IsCooking())
	{
		Super::PreSave(SaveContext);
		return;
	}

	if (ItemId.IsValid() == false)
	{
		ItemId = FGuid::NewGuid();
	}
	
	for (FArcInstancedStruct& IS : ScalableFloatFragmentSet)
	{
		IS.GetMutablePtr<FArcScalableFloatItemFragment>()->Initialize(IS.GetScriptStruct());
		IS.PreSave();
	}

	for (FArcInstancedStruct& IS : FragmentSet)
	{
		IS.PreSave();
	}

#if WITH_EDITORONLY_DATA
	UpdateAssetBundleData();
	for (FArcInstancedStruct& IS : EditorFragmentSet)
	{
		IS.PreSave();
	}
	
	if (UAssetManager::IsInitialized())
	{
		// Bundles may have changed, refresh
		UAssetManager::Get().RefreshAssetData(this);
	}
#endif	
	Super::PreSave(SaveContext);
}

FPrimaryAssetId UArcItemDefinition::GetPrimaryAssetId() const
{
	//FPrimaryAssetId Id1 = FPrimaryAssetId(ItemType, GetFName());
	FPrimaryAssetId Id1 = FPrimaryAssetId(ItemType, *ItemId.ToString());
	return Id1;
}

void UArcItemDefinition::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);
	for (const FArcInstancedStruct& S : FragmentSet)
	{
		if (S.InstancedStruct.IsValid() == false)
		{
			continue;
		}
		S.GetPtr<FArcItemFragment>()->GetAssetRegistryTags(Context);
	}
}

DEFINE_FUNCTION(UArcItemDefinition::execBP_FindItemFragment)
{
	P_GET_OBJECT(UScriptStruct, InFragmentType);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	void* OutItemDataPtr = Stack.MostRecentPropertyAddress;
	FStructProperty* OutItemProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	
	P_FINISH;
	bool bSuccess = false;

	
	
	{
		P_NATIVE_BEGIN;
		const uint8* Fragment = P_THIS->GetFragment(InFragmentType);
		//ItemData->FindFragment(InFragmentType);
		UScriptStruct* OutputStruct = OutItemProp->Struct;
		// Make sure the type we are trying to get through the blueprint node matches the
		// type of the message payload received.
		if ((OutItemProp != nullptr) && (OutItemProp->Struct != nullptr) && (Fragment != nullptr) && (
				OutItemProp->Struct == InFragmentType))
		{
			OutItemProp->Struct->CopyScriptStruct(OutItemDataPtr, Fragment);
			bSuccess = true;
		}
		P_NATIVE_END;
	}

	*(bool*)RESULT_PARAM = bSuccess;
}


float UArcItemDefinition::GetValue(const FArcScalableCurveFloat& InScalableFloat) const
{
	if (const FArcScalableFloatItemFragment* Found = GetScalableFloatFragment(InScalableFloat.GetOwnerType()))
	{
		return InScalableFloat.GetValue(Found, 1.f);
	}
	return 0.f;
}

FGameplayTagContainer UArcItemDefinition::GetTagsFromAssetData(const FAssetData& AssetData
															   , FName KeyName)
{
	FString Tags;
	AssetData.GetTagValue<FString>(KeyName
		, Tags);

	TArray<FString> SplitTags;
	Tags.ParseIntoArray(SplitTags
		, TEXT(","));

	FGameplayTagContainer OutTags;
	UGameplayTagsManager::Get().RequestGameplayTagContainer(SplitTags
		, OutTags);

	return OutTags;
}

const FName UArcItemDefinition::ItemTagsName = TEXT("ItemTags");
const FName UArcItemDefinition::AssetTagsName = TEXT("AssetTags");
const FName UArcItemDefinition::GrantedTagsName = TEXT("GrantedTags");
const FName UArcItemDefinition::RequiredTagsName = TEXT("RequiredTags");
const FName UArcItemDefinition::DenyTagsName = TEXT("DenyTags");

FGameplayTagContainer UArcItemDefinition::GetItemTags(const FAssetData& AssetData)
{
	return GetTagsFromAssetData(AssetData
		, ItemTagsName);
}

FGameplayTagContainer UArcItemDefinition::GetAssetTags(const FAssetData& AssetData)
{
	return GetTagsFromAssetData(AssetData
		, AssetTagsName);
}

FGameplayTagContainer UArcItemDefinition::GetGrantedTags(const FAssetData& AssetData)
{
	return GetTagsFromAssetData(AssetData
		, GrantedTagsName);
}

FGameplayTagContainer UArcItemDefinition::GetRequiredTags(const FAssetData& AssetData)
{
	return GetTagsFromAssetData(AssetData
		, RequiredTagsName);
}

FGameplayTagContainer UArcItemDefinition::GetDenyTags(const FAssetData& AssetData)
{
	return GetTagsFromAssetData(AssetData
		, DenyTagsName);
}

FAssetData UArcItemDefinition::FindByItemTags(const FGameplayTagContainer& InTags
											, const TArray<FAssetData>& InAssets)
{
	FAssetData Asset;
	for (const FAssetData& AD : InAssets)
	{
		FGameplayTagContainer Tags = UArcItemDefinition::GetItemTags(AD);
		if (Tags.HasAll(InTags))
		{
			Asset = AD;
			break;
		}
	}
	return Asset;
}

TArray<FAssetData> UArcItemDefinition::FilterByItemTags(const FGameplayTagContainer& InTags, const TArray<FAssetData>& InAssets)
{
	TArray<FAssetData> Asset;
	for (const FAssetData& AD : InAssets)
	{
		FGameplayTagContainer Tags = UArcItemDefinition::GetItemTags(AD);
		if (Tags.HasAll(InTags))
		{
			Asset.Add(AD);
		}
	}
	return Asset;
}

TArray<FAssetData> UArcItemDefinition::FilterByTags(const FGameplayTagContainer& InItemTags, const FGameplayTagContainer& InRequiredTags, const FGameplayTagContainer& InDenyTags, const TArray<FAssetData>& InAssets)
{
	TArray<FAssetData> Asset;

	for (const FAssetData& AD : InAssets)
	{
		const bool bItemTagsEmpty = InItemTags.IsEmpty();
		const bool bRequiredTagsEmpty = InRequiredTags.IsEmpty();
		const bool bDenyTagsEmpty = InDenyTags.IsEmpty();
		
		FGameplayTagContainer ItemTags = bItemTagsEmpty ? UArcItemDefinition::GetItemTags(AD) : FGameplayTagContainer();
		FGameplayTagContainer RequiredTags = bRequiredTagsEmpty ? UArcItemDefinition::GetRequiredTags(AD) : FGameplayTagContainer();
		FGameplayTagContainer DenyTags = bDenyTagsEmpty ? UArcItemDefinition::GetDenyTags(AD) : FGameplayTagContainer();
		
		if ((bItemTagsEmpty || ItemTags.HasAll(InItemTags))
			&& (bRequiredTagsEmpty || RequiredTags.HasAll(InRequiredTags))
			&& (bDenyTagsEmpty || !DenyTags.HasAny(InDenyTags)))
		{
			Asset.Add(AD);
		}
	}
	
	return Asset;
}

UArcItemDefinition* UArcItemDefinition::LoadItemData(const FGameplayTag& InItemTag)
{
	IAssetRegistry& AR = UAssetManager::Get().GetAssetRegistry();
	FARFilter AssetFilter;
	AssetFilter.ClassPaths.Add(UArcItemDefinition::StaticClass()->GetClassPathName());

	TSet<FTopLevelAssetPath> DerivedClassNames;
	AR.GetDerivedClassNames(AssetFilter.ClassPaths
		, TSet<FTopLevelAssetPath>()
		, DerivedClassNames);

	AssetFilter.ClassPaths.Append(DerivedClassNames.Array());
	AssetFilter.bRecursiveClasses = true;

	TArray<FAssetData> OutAssets;
	AR.GetAssets(AssetFilter
		, OutAssets);

	FAssetData D = UArcItemDefinition::FindByItemTags(FGameplayTagContainer(InItemTag)
		, OutAssets);

	if (!D.IsValid())
	{
		return nullptr;
	}
	UArcItemDefinition* ItemBlueprint = nullptr;
	// unless somone start adding 8k textures hard references textures to items it should
	// be fast.

	ItemBlueprint = Cast<UArcItemDefinition>(D.GetAsset());

	return ItemBlueprint;
}


#if WITH_EDITORONLY_DATA
void UArcItemDefinitionTemplate::SetNewOrReplaceItemTemplate(UArcItemDefinition* TargetItemDefinition)
{
	{
		TSet<FArcInstancedStruct> ExistingFragments;
		ExistingFragments.Reserve(TargetItemDefinition->FragmentSet.Num());
	
		// Add all non template ones, so we can preserve their properties.
		for (const FArcInstancedStruct& ItemFragment : TargetItemDefinition->FragmentSet)
		{
			if (ItemFragment.bFromTemplate == false)
			{
				ExistingFragments.Add(ItemFragment);
			}
		}
	
		// Empty first, since that function can be used to replace source template.
		TargetItemDefinition->FragmentSet.Empty();
		TargetItemDefinition->FragmentSet.Reserve(FragmentSet.Num());
	
		for (const FArcInstancedStruct& ItemFragment : FragmentSet)
		{
			// We try to preserve properties if the fragment types match.
			FArcInstancedStruct NewInstance;
			if (const FArcInstancedStruct* Found = ExistingFragments.Find(ItemFragment))
			{
				NewInstance.InstancedStruct = Found->InstancedStruct;
				NewInstance.StructName = Found->InstancedStruct.GetScriptStruct()->GetFName();
			}
			else
			{
				NewInstance.InstancedStruct = ItemFragment.InstancedStruct;
				NewInstance.StructName = ItemFragment.InstancedStruct.GetScriptStruct()->GetFName();	
			}
		
			NewInstance.bFromTemplate = true;
		
			TargetItemDefinition->FragmentSet.Add(NewInstance);
		}
	}
	{
		TSet<FArcInstancedStruct> ExistingFragments;
		ExistingFragments.Reserve(TargetItemDefinition->FragmentSet.Num());
	
		// Add all non template ones, so we can preserve their properties.
		for (const FArcInstancedStruct& ItemFragment : TargetItemDefinition->ScalableFloatFragmentSet)
		{
			if (ItemFragment.bFromTemplate == false)
			{
				ExistingFragments.Add(ItemFragment);
			}
		}
		
		for (const FArcInstancedStruct& ItemFragment : ScalableFloatFragmentSet)
		{
			// We try to preserve properties if the fragment types match.
			FArcInstancedStruct NewInstance;
			if (const FArcInstancedStruct* Found = ExistingFragments.Find(ItemFragment))
			{
				NewInstance.InstancedStruct = Found->InstancedStruct;
				NewInstance.StructName = Found->InstancedStruct.GetScriptStruct()->GetFName();
			}
			else
			{
				NewInstance.InstancedStruct = ItemFragment.InstancedStruct;
				NewInstance.StructName = ItemFragment.InstancedStruct.GetScriptStruct()->GetFName();	
			}
		
			NewInstance.bFromTemplate = true;
		
			TargetItemDefinition->ScalableFloatFragmentSet.Add(NewInstance);
		}
	}
	TargetItemDefinition->SourceTemplate = this;
	TargetItemDefinition->ItemType = ItemType;
}

void UArcItemDefinitionTemplate::SetItemTemplate(UArcItemDefinition* TargetItemDefinition)
{
	{
		TSet<FArcInstancedStruct> ExistingFragments;
		ExistingFragments.Reserve(TargetItemDefinition->FragmentSet.Num());
	
		// Add all non template ones.
		for (const FArcInstancedStruct& ItemFragment : TargetItemDefinition->FragmentSet)
		{
			if (ItemFragment.bFromTemplate == false)
			{
				ExistingFragments.Add(ItemFragment);
			}
		}
	
		// Empty first, since that function can be used to replace source template.
		TargetItemDefinition->FragmentSet.Empty();
		TargetItemDefinition->FragmentSet.Reserve(FragmentSet.Num());
	
		for (const FArcInstancedStruct& ItemFragment : FragmentSet)
		{
			// Skip existing ones.
			if (ExistingFragments.Contains(ItemFragment))
			{
				continue;
			}
		
			FArcInstancedStruct NewInstance;

			// It should call InitializeAs, making new instance in memory.
			NewInstance.InstancedStruct = ItemFragment.InstancedStruct;
			NewInstance.StructName = ItemFragment.InstancedStruct.GetScriptStruct()->GetFName();
			NewInstance.bFromTemplate = true;
		
			TargetItemDefinition->FragmentSet.Add(NewInstance);
		}
	
		for (const FArcInstancedStruct& ItemFragment : ExistingFragments)
		{
			FArcInstancedStruct NewInstance;

			// It should call InitializeAs, making new instance in memory.
			NewInstance.InstancedStruct = ItemFragment.InstancedStruct;
			NewInstance.StructName = ItemFragment.InstancedStruct.GetScriptStruct()->GetFName();
			NewInstance.bFromTemplate = false;
		
			TargetItemDefinition->FragmentSet.Add(NewInstance);
		}
	}
	
	{
		TSet<FArcInstancedStruct> ExistingFragments;
		ExistingFragments.Reserve(TargetItemDefinition->ScalableFloatFragmentSet.Num());
	
		// Add all non template ones.
		for (const FArcInstancedStruct& ItemFragment : TargetItemDefinition->ScalableFloatFragmentSet)
		{
			if (ItemFragment.bFromTemplate == false)
			{
				ExistingFragments.Add(ItemFragment);
			}
		}
	
		// Empty first, since that function can be used to replace source template.
		TargetItemDefinition->ScalableFloatFragmentSet.Empty();
		TargetItemDefinition->ScalableFloatFragmentSet.Reserve(ScalableFloatFragmentSet.Num());
	
		for (const FArcInstancedStruct& ItemFragment : ScalableFloatFragmentSet)
		{
			// Skip existing ones.
			if (ExistingFragments.Contains(ItemFragment))
			{
				continue;
			}
		
			FArcInstancedStruct NewInstance;

			// It should call InitializeAs, making new instance in memory.
			NewInstance.InstancedStruct = ItemFragment.InstancedStruct;
			NewInstance.StructName = ItemFragment.InstancedStruct.GetScriptStruct()->GetFName();
			NewInstance.bFromTemplate = true;
		
			TargetItemDefinition->ScalableFloatFragmentSet.Add(NewInstance);
		}
	
		for (const FArcInstancedStruct& ItemFragment : ExistingFragments)
		{
			FArcInstancedStruct NewInstance;

			// It should call InitializeAs, making new instance in memory.
			NewInstance.InstancedStruct = ItemFragment.InstancedStruct;
			NewInstance.StructName = ItemFragment.InstancedStruct.GetScriptStruct()->GetFName();
			NewInstance.bFromTemplate = false;
		
			TargetItemDefinition->ScalableFloatFragmentSet.Add(NewInstance);
		}
	}
	
	TargetItemDefinition->SourceTemplate = this;
	TargetItemDefinition->ItemType = ItemType;
}

void UArcItemDefinitionTemplate::UpdateFromTemplate(UArcItemDefinition* TargetItemDefinition)
{
	{
		TSet<FArcInstancedStruct> ExistingFragments;

		// Reserve memory for worst case.
		int32 ElementsNum = 0;
		if (TargetItemDefinition->FragmentSet.Num() > FragmentSet.Num())
		{
			ElementsNum = TargetItemDefinition->FragmentSet.Num();
		}
		else
		{
			ElementsNum = FragmentSet.Num();
		}
		ExistingFragments.Reserve(ElementsNum);
	
		// First copy all non templated.
		for (const FArcInstancedStruct& ItemFragment : TargetItemDefinition->FragmentSet)
		{
			if (ItemFragment.IsValid() == false)
			{
				continue;
			}
			
			if (ItemFragment.bFromTemplate == false)
			{
				ExistingFragments.Add(ItemFragment);
			}
		}
	
		// First try to find existing fragments and just copy them over.
		for (const FArcInstancedStruct& ItemFragment : FragmentSet)
		{
			if (ItemFragment.IsValid() == false)
			{
				continue;
			}
			
			if (ExistingFragments.Contains(ItemFragment))
			{
				continue;
			}
		
			if (const FArcInstancedStruct* Found = TargetItemDefinition->FragmentSet.Find(ItemFragment))
			{
				ExistingFragments.Add(*Found);
			}
		}

		// Now add new Fragments
		for (const FArcInstancedStruct& ItemFragment : FragmentSet)
		{
			if (ItemFragment.IsValid() == false)
			{
				continue;
			}
			
			if (TargetItemDefinition->FragmentSet.Contains(ItemFragment) == false)
			{
				FArcInstancedStruct NewInstance;

				// It should call InitializeAs, making new instance in memory.
				NewInstance.InstancedStruct = ItemFragment.InstancedStruct;
				NewInstance.StructName = ItemFragment.InstancedStruct.GetScriptStruct()->GetFName();
				NewInstance.bFromTemplate = true;
			
				ExistingFragments.Add(NewInstance);
			}
		}

		// Finally copy everything back to original item definition:
		TargetItemDefinition->FragmentSet.Empty();
		TargetItemDefinition->FragmentSet.Reserve(ExistingFragments.Num());

		for (const FArcInstancedStruct& ItemFragment : ExistingFragments)
		{
			FArcInstancedStruct NewInstance;
			// It should call InitializeAs, making new instance in memory.
			NewInstance.InstancedStruct = ItemFragment.InstancedStruct;
			NewInstance.StructName = ItemFragment.InstancedStruct.GetScriptStruct()->GetFName();
			TargetItemDefinition->FragmentSet.Add(NewInstance);
		}
	}
	{
		TSet<FArcInstancedStruct> ExistingFragments;

		// Reserve memory for worst case.
		int32 ElementsNum = 0;
		if (TargetItemDefinition->ScalableFloatFragmentSet.Num() > ScalableFloatFragmentSet.Num())
		{
			ElementsNum = TargetItemDefinition->ScalableFloatFragmentSet.Num();
		}
		else
		{
			ElementsNum = ScalableFloatFragmentSet.Num();
		}
		ExistingFragments.Reserve(ElementsNum);
	
		// First copy all non templated.
		for (const FArcInstancedStruct& ItemFragment : TargetItemDefinition->ScalableFloatFragmentSet)
		{
			if (ItemFragment.IsValid() == false)
			{
				continue;
			}
			if (ItemFragment.bFromTemplate == false)
			{
				ExistingFragments.Add(ItemFragment);
			}
		}
	
		// First try to find existing fragments and just copy them over.
		for (const FArcInstancedStruct& ItemFragment : ScalableFloatFragmentSet)
		{
			if (ItemFragment.IsValid() == false)
			{
				continue;
			}
			
			if (ExistingFragments.Contains(ItemFragment))
			{
				continue;
			}
		
			if (const FArcInstancedStruct* Found = TargetItemDefinition->ScalableFloatFragmentSet.Find(ItemFragment))
			{
				ExistingFragments.Add(*Found);
			}
		}

		// Now add new Fragments
		for (const FArcInstancedStruct& ItemFragment : ScalableFloatFragmentSet)
		{
			if (ItemFragment.IsValid() == false)
			{
				continue;
			}
			
			if (TargetItemDefinition->ScalableFloatFragmentSet.Contains(ItemFragment) == false)
			{
				FArcInstancedStruct NewInstance;

				// It should call InitializeAs, making new instance in memory.
				NewInstance.InstancedStruct = ItemFragment.InstancedStruct;
				NewInstance.StructName = ItemFragment.InstancedStruct.GetScriptStruct()->GetFName();
				NewInstance.bFromTemplate = true;
			
				ExistingFragments.Add(NewInstance);
			}
		}

		// Finally copy everything back to original item definition:
		TargetItemDefinition->ScalableFloatFragmentSet.Empty();
		TargetItemDefinition->ScalableFloatFragmentSet.Reserve(ExistingFragments.Num());

		for (const FArcInstancedStruct& ItemFragment : ExistingFragments)
		{
			if (ItemFragment.IsValid() == false)
			{
				continue;
			}
			
			FArcInstancedStruct NewInstance;

			// It should call InitializeAs, making new instance in memory.
			NewInstance.InstancedStruct = ItemFragment.InstancedStruct;
			NewInstance.StructName = ItemFragment.InstancedStruct.GetScriptStruct()->GetFName();
			TargetItemDefinition->ScalableFloatFragmentSet.Add(NewInstance);
		}
	}
}
#endif