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

#pragma once

#include "Engine/DataAsset.h"
#include "UObject/AssetRegistryTagsContext.h"

#include "ArcCore/Items/ArcItemTypes.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "UObject/ObjectSaveContext.h"

#include "ArcItemDefinition.generated.h"

struct FArcScalableCurveFloat;
struct FGameplayTagContainer;
struct FAssetData;
struct FGameplayTag;
class UScriptStruct;

/**
 * Definition of Item, which can be extend using Item Fragments and Scalable Float Item Fragments.
 *
 * This class should not be subclassed.
 */
UCLASS(BlueprintType, meta = (LoadBehavior = "LazyOnDemand"))
class ARCCORE_API UArcItemDefinition : public UDataAsset
{
	GENERATED_BODY()

	friend class UArcItemDefinitionTemplate;
protected:
	UPROPERTY(VisibleAnywhere)
	FGuid ItemId;
	
	/** Fragments are way to add custom data into Item Definition. */
	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcCore.ArcItemFragment", ShowTreeView, ExcludeBaseStruct), Category = "Item")
	TSet<FArcInstancedStruct> FragmentSet;

	/** Scalable Float Fragments are special data, which can be used in conjunction with Data Registry.
	 *
	 * These fragments also get merged in ItemData (ie. when item have attachments).
	 */
	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcCore.ArcScalableFloatItemFragment", ShowTreeView, ExcludeBaseStruct), Category = "Item")
	TSet<FArcInstancedStruct> ScalableFloatFragmentSet;

	/**
	 * @brief How item should stack. If not set all items will be treated as new instance and new ItemData will be created for each.
	 */
	UPROPERTY(EditAnywhere, Category = "Item", meta = (BaseStruct = "/Script/ArcCore.ArcItemStackMethod"))
	FInstancedStruct StackMethod;
	
	/** Type of item. Only useful when using items with Asset Manager. */
	UPROPERTY(EditAnywhere, Category = "Item")
	FPrimaryAssetType ItemType;

#if WITH_EDITORONLY_DATA
	/** Asset Bundle data computed at save time. In cooked builds this is accessible from AssetRegistry */
	UPROPERTY()
	FAssetBundleData AssetBundleData;

	/** Editor only fragments. Should be implemented in editor module. */
	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcCoreEditor.ArcEditorItemFragment", ShowTreeView, ExcludeBaseStruct), Category = "Editor")
	TSet<FArcInstancedStruct> EditorFragmentSet;
	
public:
	UFUNCTION(BlueprintCallable, CustomThunk, BlueprintInternalUseOnly, Category = "Arc Core|Items", meta = (CustomStructureParam = "OutFragment", ExpandBoolAsExecs = "ReturnValue"))
	bool BP_FindItemFragment(UPARAM(meta = (MetaStruct = "/Script/ArcCore.ArcItemFragment")) UScriptStruct* InFragmentType
						  , int32& OutFragment);
 
	DECLARE_FUNCTION(execBP_FindItemFragment);
	
	template <typename T>
	const T* FindEditorFragment() const
	{
		if (const FArcInstancedStruct* IS = EditorFragmentSet.FindByHash(GetTypeHash(T::StaticStruct()->GetFName()), T::StaticStruct()->GetFName()))
		{
			return IS->GetPtr<T>();
		}
		return nullptr;
	}

	template <typename T>
	const T* GetChildEditorFragment() const
	{
		for (const FArcInstancedStruct& InstancedStruct : EditorFragmentSet)
		{
			if (InstancedStruct.GetScriptStruct() && InstancedStruct.GetScriptStruct()->IsChildOf(T::StaticStruct()))
			{
				return InstancedStruct.GetPtr<T>();
			}
		}
		return nullptr;
	}
	template <typename T>
	const TArray<const T*> GetAllChildEditorFragment() const
	{
		TArray<const T*> OutArray;
		for (const FArcInstancedStruct& InstancedStruct : EditorFragmentSet)
		{
			if (InstancedStruct.GetScriptStruct() && InstancedStruct.GetScriptStruct()->IsChildOf(T::StaticStruct()))
			{
				OutArray.Add(InstancedStruct.GetPtr<T>());
			}
		}
		return OutArray;
	}
#endif
	
public:
	const TSet<FArcInstancedStruct>& GetScalableFloatFragments() const
	{
		return ScalableFloatFragmentSet;
	}
	const TSet<FArcInstancedStruct>& GetFragmentSet() const
	{
		return FragmentSet;
	}
public:
	UArcItemDefinition();

#if WITH_EDITOR
	/**
 	 * Called when a property on this object has been modified externally
 	 *
 	 * @param PropertyThatChanged the property that was modified
 	 */
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	
#if WITH_EDITORONLY_DATA
	/** This scans the class for AssetBundles metadata on asset properties and initializes the AssetBundleData with InitializeAssetBundlesFromMetadata */
	virtual void UpdateAssetBundleData();
#endif
	
	virtual void PostInitProperties() override;

	virtual void PreSave(FObjectPreSaveContext SaveContext) override;

	// Reverse index ItemTags (Tag Key, PropertyNAme = Value);
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;

	FPrimaryAssetType GetItemType() const
	{
		return ItemType;
	}

	float GetValue(const FArcScalableCurveFloat& InScalableFloat) const;
	
	template<typename T>
	TArray<const T*> GetFragmentsOfType() const
	{
		TArray<const T*> Out;
		for (const FArcInstancedStruct& InstancedStruct : FragmentSet)
		{
			if (InstancedStruct.GetScriptStruct() && InstancedStruct.GetScriptStruct()->IsChildOf(T::StaticStruct()))
			{
				Out.Add(InstancedStruct.GetPtr<T>());
			}
		}

		return Out;
	}
	
	template <typename T>
	const T* FindFragment() const
	{
		if (const FArcInstancedStruct* IS = FragmentSet.FindByHash(GetTypeHash(T::StaticStruct()->GetFName()), T::StaticStruct()->GetFName()))
		{
			return IS->GetPtr<T>();
		}
		return nullptr;
	}

	const uint8* GetFragment(UScriptStruct* InStructType) const
	{
		if (const FArcInstancedStruct* IS = FragmentSet.FindByHash(GetTypeHash(InStructType->GetFName())
			, InStructType->GetFName()))
		{
			return IS->GetMemory();
		}

		return nullptr;
	}

	template <typename T>
	const T* GetScalableFloatFragment() const
	{
		if (const FArcInstancedStruct* IS = ScalableFloatFragmentSet.FindByHash(GetTypeHash(T::StaticStruct()->GetFName())
			, T::StaticStruct()->GetFName()))
		{
			return IS->GetPtr<T>();
		}

		return nullptr;
	}

	const FArcScalableFloatItemFragment* GetScalableFloatFragment(const UScriptStruct* InStructType) const
	{
		if (const FArcInstancedStruct* IS = ScalableFloatFragmentSet.FindByHash(GetTypeHash(InStructType->GetFName()), InStructType->GetFName()))
		{
			return IS->GetPtr<FArcScalableFloatItemFragment>();
		}

		return nullptr;
	}
	
	template<typename T>
	const T* GetStackMethod() const
	{
		return StackMethod.GetPtr<T>();
	}
public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

protected:
#if WITH_EDITORONLY_DATA
	/**
	 * @brief Source template from which this item definition has been created. Can be null.
	 */
	UPROPERTY(AssetRegistrySearchable)
	TObjectPtr<UArcItemDefinitionTemplate> SourceTemplate;

public:
	UArcItemDefinitionTemplate* GetSourceTemplate() const
	{
		return SourceTemplate;
	}
#endif
	
private:
	static FGameplayTagContainer GetTagsFromAssetData(const FAssetData& AssetData
													  , FName KeyName);

	static const FName ItemTagsName;
	static const FName AssetTagsName;
	static const FName GrantedTagsName;
	static const FName RequiredTagsName;
	static const FName DenyTagsName;

public:
	/** Get Item Tags from AssetData. */
	static FGameplayTagContainer GetItemTags(const FAssetData& AssetData);

	static FGameplayTagContainer GetAssetTags(const FAssetData& AssetData);

	static FGameplayTagContainer GetGrantedTags(const FAssetData& AssetData);

	static FGameplayTagContainer GetRequiredTags(const FAssetData& AssetData);

	static FGameplayTagContainer GetDenyTags(const FAssetData& AssetData);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Items")
	static FAssetData FindByItemTags(const FGameplayTagContainer& InTags
									 , const TArray<FAssetData>& InAssets);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Items")
	static TArray<FAssetData> FilterByItemTags(const FGameplayTagContainer& InItemTags
									 , const TArray<FAssetData>& InAssets);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Items")
	static TArray<FAssetData> FilterByTags(const FGameplayTagContainer& InItemTags
									 , const FGameplayTagContainer& InRequiredTags
									 , const FGameplayTagContainer& InDenyTags
									 , const TArray<FAssetData>& InAssets);
	
	/** Tried to load single item with tag. Only works if item have Tags Fragment. */
	static UArcItemDefinition* LoadItemData(const FGameplayTag& InItemTag);
};

UCLASS(meta = (LoadBehavior = "LazyOnDemand"))
class ARCCORE_API UArcItemDefinitionTemplate : public UArcItemDefinition
{
	GENERATED_BODY()
	
public:
	// Override again, we don't really care about AssetId for templates.
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(ItemType, GetFName());
	}

	/**
	 * Called during saving to determine if the object is forced to be editor only or not
	 *
	 * @return	true if this object should never be loaded outside the editor
	 */
	virtual bool IsEditorOnly() const override
	{
		return true;
	}
#if WITH_EDITORONLY_DATA
	/**
	 * @brief Sets Fragments/Properties to item Definition from this template.
	 * @param TargetItemDefinition Item definition to which properties will be copied from this template.
	 */
	void SetNewOrReplaceItemTemplate(UArcItemDefinition* TargetItemDefinition);

	/**
	 * @brief Sets Fragments/Properties to item Definition from this template.
	 * Preserves any existing Fragments.
	 * @param TargetItemDefinition Item definition to which properties will be copied from this template.
	 */
	void SetItemTemplate(UArcItemDefinition* TargetItemDefinition);
	
	/**
	 * @brief Update fragments/properties using this template in target item definition.
	 * Will remove any item fragments which are missing from template, add new ones, and left all that exists unchanged.
	 * @param TargetItemDefinition Item definition in which we will update fragments/properties. 
	 */
	void UpdateFromTemplate(UArcItemDefinition* TargetItemDefinition);
#endif
};