// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBuilderTestHelpers.h"

#include "Mass/EntityManager.h"
#include "Mass/EntityFragments.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcItemTestRecordFiles.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Fragments/ArcMassItemStoreFragment.h"

#include "ArcBuildingDefinition.h"

#include "Engine/AssetManager.h"
#include "AssetRegistry/AssetBundleData.h"

namespace ArcBuilderTest::TestHelpers
{
	FPrimaryAssetId MakeTaggedItemDef(FArcItemTestRecordFiles& Records, const FGameplayTag& InTag)
	{
		FArcItemDefinitionData Def;

		FArcItemFragment_Tags TagsFrag;
		if (InTag.IsValid())
		{
			TagsFrag.AssetTags.AddTag(InTag);
		}
		ArcItemTestRecords::AddFragment(Def, TagsFrag);

		return Records.Write(Def);
	}

	FArcItemSpec MakeSpec(const FPrimaryAssetId& DefId, int32 Amount)
	{
		FArcItemSpec Spec;
		Spec.SetItemDefinition(DefId);
		Spec.SetItemId(FArcItemId::Generate());
		Spec.Amount = static_cast<uint16>(Amount);
		return Spec;
	}

	FMassEntityHandle CreateStoreEntity(FMassEntityManager& EntityManager)
	{
		TArray<FInstancedStruct> Frags;
		Frags.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle Entity = EntityManager.CreateEntity(Frags);
		EntityManager.AddSparseElementToEntity<FArcMassItemStoreFragment>(Entity);
		return Entity;
	}

	FMassEntityHandle CreateBareEntity(FMassEntityManager& EntityManager)
	{
		TArray<FInstancedStruct> Frags;
		Frags.Add(FInstancedStruct::Make(FTransformFragment()));
		return EntityManager.CreateEntity(Frags);
	}

	UArcBuildingDefinition* MakeTransientBuildingDef(const FName& InName)
	{
		return NewObject<UArcBuildingDefinition>(GetTransientPackage(), InName, RF_Transient);
	}

	FPrimaryAssetId RegisterBuildingDefDynamic(UArcBuildingDefinition* Def, const FPrimaryAssetType& InType)
	{
		if (Def == nullptr)
		{
			return FPrimaryAssetId();
		}

		Def->BuildingType = InType;

		const FPrimaryAssetId AssetId = Def->GetPrimaryAssetId();
		if (!AssetId.IsValid())
		{
			return FPrimaryAssetId();
		}

		const FSoftObjectPath AssetPath(Def);

		// AddDynamicAsset requires BundleData.Bundles.Num() > 0 to actually register the asset.
		FAssetBundleData BundleData;
		BundleData.Bundles.Emplace(FName(TEXT("ArcBuilderTest")));

		const bool bAdded = UAssetManager::Get().AddDynamicAsset(AssetId, AssetPath, BundleData);
		return bAdded ? AssetId : FPrimaryAssetId();
	}

	void UnregisterDynamicAsset(const FPrimaryAssetId& Id)
	{
		if (Id.IsValid())
		{
			UAssetManager::Get().AddDynamicAsset(Id, FSoftObjectPath(), FAssetBundleData());
		}
	}
} // namespace ArcBuilderTest::TestHelpers
