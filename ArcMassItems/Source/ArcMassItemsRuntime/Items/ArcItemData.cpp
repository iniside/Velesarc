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

#include "Items/ArcItemData.h"

#include "Engine/World.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemScalableFloat.h"
#include "Items/ArcItemSpec.h"

#include "Items/Fragments/ArcItemFragment_Tags.h"

#include "ArcItemsHelpers.h"
#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY(LogItemEntry);


FArcItemData::FArcItemData()
	: ItemId()
	, Level(0)
{
}

FArcItemData::~FArcItemData()
{
}

UScriptStruct* FArcItemData::GetScriptStruct() const
{
	return FArcItemData::StaticStruct();
}

float FArcItemData::GetValue(const FArcScalableCurveFloat& InScalableFloat) const
{
	if (const FArcScalableFloatItemFragment* const* ScalableFragment = ScalableFloatFragments.Find(InScalableFloat.GetOwnerType()))
	{
		return InScalableFloat.GetValue(*ScalableFragment
			, Level, ScalableFloatContext);
	}
	return 0.f;
}

float FArcItemData::GetValueWithLevel(const FArcScalableCurveFloat& InScalableFloat
									   , float InLevel) const
{
	if (const FArcScalableFloatItemFragment* const* ScalableFragment = ScalableFloatFragments.Find(InScalableFloat.GetOwnerType()))
	{
		return InScalableFloat.GetValue(*ScalableFragment
			, InLevel, ScalableFloatContext);
	}
	return 0.f;
}

const TArray<const FArcItemData*>& FArcItemData::GetItemsInSockets() const
{
	// In Mass-native context, ItemsInSockets is populated externally by the
	// Mass attachment processor / store fragment. Actor-side lazy fill via
	// UArcItemsStoreComponent retired with the store.
	return ItemsInSockets;
}

const FArcItemSpec* FArcItemData::GetSpecPtr() const
{
	return &Spec;
}

const FArcItemData* FArcItemData::GetOwnerPtr() const
{
	// Owner pointer caching now relies on the Mass store fragment to resolve
	// OwnerId; the actor-side lookup retired with UArcItemsStoreComponent.
	return OwnerItemCachedPtr;
}

void FArcItemData::SetItemSpec(const FArcItemSpec& InSpec)
{
	Spec = InSpec;
}

FInstancedStruct FArcItemData::NewFromSpec(const FArcItemSpec& InSpec)
{
	FInstancedStruct Result = FInstancedStruct::Make<FArcItemData>();
	FArcItemData* NewEntry = Result.GetMutablePtr<FArcItemData>();

	if (InSpec.ItemId.IsValid() == false)
	{
		NewEntry->ItemId = FArcItemId::Generate();
	}
	else
	{
		NewEntry->ItemId = InSpec.ItemId;
	}

	NewEntry->ItemDefinition = InSpec.GetItemDefinition();
	NewEntry->Level = InSpec.Level;

	return Result;
}

void FArcItemData::InitializeMass(UWorld* InWorld)
{
	World = InWorld;

	// Build ItemInstances from definition fragments
	TArray<const FArcItemFragment_ItemInstanceBase*> DefInstances;
	if (ItemDefinition != nullptr)
	{
		DefInstances = ItemDefinition->GetFragmentsOfType<FArcItemFragment_ItemInstanceBase>();
	}
	for (const FArcItemFragment_ItemInstanceBase* IIB : DefInstances)
	{
		if (!IIB->GetCreateItemInstance())
		{
			continue;
		}
		if (UScriptStruct* InstanceType = IIB->GetItemInstanceType())
		{
			FInstancedStruct NewInstance;
			NewInstance.InitializeAs(InstanceType, nullptr);

			int32 InitialDataIdx = Spec.InitialInstanceData.IndexOfByPredicate([InstanceType](const FInstancedStruct& Other)
			{
				return Other.GetScriptStruct() == InstanceType;
			});
			if (InitialDataIdx != INDEX_NONE)
			{
				NewInstance = Spec.InitialInstanceData[InitialDataIdx];
			}

			ItemInstances.Data.Add(MoveTemp(NewInstance));
		}
	}

	// Build ItemInstances from spec fragments
	TArray<const FArcItemFragment_ItemInstanceBase*> SpecFragments = Spec.GetSpecFragments();
	for (const FArcItemFragment_ItemInstanceBase* Fragment : SpecFragments)
	{
		if (!Fragment->GetCreateItemInstance())
		{
			continue;
		}
		if (UScriptStruct* InstanceType = Fragment->GetItemInstanceType())
		{
			FInstancedStruct NewInstance;
			NewInstance.InitializeAs(InstanceType, nullptr);

			int32 InitialDataIdx = Spec.InitialInstanceData.IndexOfByPredicate([InstanceType](const FInstancedStruct& Other)
			{
				return Other.GetScriptStruct() == InstanceType;
			});
			if (InitialDataIdx != INDEX_NONE)
			{
				NewInstance = Spec.InitialInstanceData[InitialDataIdx];
			}

			ItemInstances.Data.Add(MoveTemp(NewInstance));
		}
	}

	SetItemInstances(ItemInstances);

	// Build ScalableFloatFragments (ItemsInSockets is empty at init, so this is
	// the definition-only build).
	RebuildScalableFloatFragments();

	// Build ScalableFloatContext (Mass: no owner actor / ASC)
	ScalableFloatContext.ItemData = this;
	ScalableFloatContext.ItemDefinition = ItemDefinition;

	// Build ItemAggregatedTags
	RebuildAggregatedTags();

	// Fire OnItemInitialize on all fragments
	ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(this
		, [](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
		{
			InFragment->OnItemInitialize(ItemData);
		});
}

void FArcItemData::RebuildAggregatedTags()
{
	ItemAggregatedTags.Reset();
	if (const FArcItemFragment_Tags* Tags = ArcItemsHelper::GetFragment<FArcItemFragment_Tags>(this))
	{
		ItemAggregatedTags.AppendTags(Tags->ItemTags);
		// AssetTags too: recipe matching (FArcRecipeIngredient_Tags::DoesItemSatisfy) reads
		// AssetTags and authored items carry their identity tags there — aggregating both
		// keeps tag queries (CountItemsByTags, bag acceptance, equipment filter) consistent
		// with recipe matching regardless of which container the asset authored.
		ItemAggregatedTags.AppendTags(Tags->AssetTags);
	}
}

void FArcItemData::RefreshItemDefinition()
{
	ItemDefinition = FArcItemDefinitionData::Find(GetItemDefinitionId());
	RebuildScalableFloatFragments();
	ScalableFloatContext.ItemDefinition = ItemDefinition;
	RebuildAggregatedTags();
}

void FArcItemData::RebuildScalableFloatFragments()
{
	ScalableFloatFragments.Reset();

	if (ItemDefinition == nullptr)
	{
		return;
	}

	// Own definition first — it wins per fragment type.
	const TSet<FArcInstancedStruct>& DefScalable = ItemDefinition->GetScalableFloatFragments();
	for (const FArcInstancedStruct& I : DefScalable)
	{
		ScalableFloatFragments.FindOrAdd(I.GetScriptStruct()) = I.GetPtr<FArcScalableFloatItemFragment>();
	}

	// Then attached items fill the remaining types (first socket wins among sockets).
	for (const FArcItemData* SocketItem : ItemsInSockets)
	{
		const FArcItemDefinitionData* SocketDefinition = SocketItem != nullptr ? SocketItem->GetItemDefinition() : nullptr;
		if (SocketDefinition == nullptr)
		{
			continue;
		}
		const TSet<FArcInstancedStruct>& SocketScalable = SocketDefinition->GetScalableFloatFragments();
		for (const FArcInstancedStruct& I : SocketScalable)
		{
			if (ScalableFloatFragments.Contains(I.GetScriptStruct()) == false)
			{
				ScalableFloatFragments.Add(I.GetScriptStruct(), I.GetPtr<FArcScalableFloatItemFragment>());
			}
		}
	}
}

void FArcItemData::OnItemAdded()
{
	ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(this
		, [](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
		{
			InFragment->OnItemAdded(ItemData);
		});
}

void FArcItemData::OnPreRemove()
{
	ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(this
		, [](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
		{
			InFragment->OnPreRemoveItem(ItemData);
		});
}

void FArcItemData::Deinitialize()
{
}

const uint8* FArcItemData::FindFragment(UScriptStruct* InStructType) const
{
	const uint8* Ptr = ItemDefinition != nullptr ? ItemDefinition->GetFragment(InStructType) : nullptr;
	if (Ptr != nullptr)
	{
		return Ptr;
	}

	for (const FArcItemData* SocketItem : GetItemsInSockets())
	{
		Ptr = SocketItem->FindFragment(InStructType);
		if (Ptr)
		{
			return Ptr;
		}
	}
	return nullptr;
}

void FArcItemData::SetItemInstances(const FArcItemInstanceArray& InInstances)
{
	if (&InInstances != &ItemInstances)
	{
		ItemInstances = InInstances;
	}
	InstancedData.Reset();
	for (FInstancedStruct& Inst : ItemInstances.Data)
	{
		if (Inst.IsValid())
		{
			FArcItemInstance* Instance = Inst.GetMutablePtr<FArcItemInstance>();
			if (Instance)
			{
				Instance->OwningItem = this;
				InstancedData.Add(Inst.GetScriptStruct(), FStructView(Inst));
			}
		}
	}
}

const FPrimaryAssetId& FArcItemData::GetItemDefinitionId() const
{
	return Spec.ItemDefinitionId;
}

const FArcItemDefinitionData* FArcItemData::GetItemDefinition() const
{
	return ItemDefinition;
}

const FGameplayTagContainer& FArcItemData::GetItemAggregatedTags() const
{
	return ItemAggregatedTags;
}
