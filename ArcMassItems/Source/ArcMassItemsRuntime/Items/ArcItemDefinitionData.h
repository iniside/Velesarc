// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "StructUtils/InstancedStruct.h"
#include "Items/Fragments/ArcItemFragment.h"

#include "ArcItemDefinitionData.generated.h"

struct FArcJsonAssetTypeInfo;
struct FArcScalableCurveFloat;
struct FAssetData;

USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcItemDefinitionData
{
	GENERATED_BODY()

public:
	static FPrimaryAssetType JsonPrimaryAssetType()
	{
		return FPrimaryAssetType(TEXT("ArcItem"));
	}

	static const TCHAR* JsonExtension()
	{
		return TEXT("arcitem");
	}

	UPROPERTY(VisibleAnywhere)
	FGuid ItemId;

	UPROPERTY(EditAnywhere, Category = "Item", meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcItemFragment", ShowTreeView, ExcludeBaseStruct))
	TSet<FArcInstancedStruct> FragmentSet;

	UPROPERTY(EditAnywhere, Category = "Item", meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcScalableFloatItemFragment", ShowTreeView, ExcludeBaseStruct))
	TSet<FArcInstancedStruct> ScalableFloatFragmentSet;

	UPROPERTY(EditAnywhere, Category = "Item", meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcItemStackMethod"))
	FInstancedStruct StackMethod;

	/** Type of item. Only useful when using items with Asset Manager. */
	UPROPERTY(EditAnywhere, Category = "Item")
	FPrimaryAssetType ItemType = JsonPrimaryAssetType();

	static const FArcJsonAssetTypeInfo& GetJsonTypeInfo();

	static const FArcItemDefinitionData* Find(const FPrimaryAssetId& Id);

	FPrimaryAssetId GetPrimaryAssetId() const;

	FPrimaryAssetType GetItemType() const
	{
		return ItemType;
	}

	const TSet<FArcInstancedStruct>& GetFragmentSet() const
	{
		return FragmentSet;
	}

	const TSet<FArcInstancedStruct>& GetScalableFloatFragments() const
	{
		return ScalableFloatFragmentSet;
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

	template <typename T>
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

	const uint8* GetFragment(const UScriptStruct* InStructType) const;

	template <typename T>
	const T* GetScalableFloatFragment() const
	{
		if (const FArcInstancedStruct* IS = ScalableFloatFragmentSet.FindByHash(GetTypeHash(T::StaticStruct()->GetFName()), T::StaticStruct()->GetFName()))
		{
			return IS->GetPtr<T>();
		}
		return nullptr;
	}

	const FArcScalableFloatItemFragment* GetScalableFloatFragment(const UScriptStruct* InStructType) const;

	template <typename T>
	const T* GetStackMethod() const
	{
		return StackMethod.GetPtr<T>();
	}

	float GetValue(const FArcScalableCurveFloat& InScalableFloat) const;

	static FGameplayTagContainer GetItemTags(const FAssetData& AssetData);
	static FGameplayTagContainer GetAssetTags(const FAssetData& AssetData);
	static FGameplayTagContainer GetGrantedTags(const FAssetData& AssetData);
	static FGameplayTagContainer GetRequiredTags(const FAssetData& AssetData);
	static FGameplayTagContainer GetDenyTags(const FAssetData& AssetData);
};
