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

#include "Items/ArcItemSpec.h"

#include "ArcItemBundleNames.h"
#include "Items/ArcItemDefinition.h"
#include "UObject/SoftObjectPath.h"

#include "Core/ArcCoreAssetManager.h"

#include "Iris/Serialization/PolymorphicNetSerializerImpl.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/Serialization/NetReferenceCollector.h"
#include "Net/UnrealNetwork.h"
#include "Iris/ReplicationSystem/ReplicationFragmentUtil.h"

DEFINE_LOG_CATEGORY(LogArcItemSpec);

const UArcItemDefinition* FArcItemSpec::GetItemDefinition() const
{
	const UArcItemDefinition* Item = UArcCoreAssetManager::Get().GetAssetWithBundles<UArcItemDefinition>(ItemDefinitionId, false);
	return Item;
}

FArcItemSpec FArcItemSpec::NewItem(const UArcItemDefinition* NewItem
								   , uint8 Level
								   , int32 Amount)
{
	FArcItemSpec NewEntry;
	NewEntry.ItemId = FArcItemId::Generate();
	NewEntry.Level = Level;
	NewEntry.Amount = Amount;
	return NewEntry;
}

FArcItemSpec FArcItemSpec::NewItem(const FPrimaryAssetId& NewItem, uint8 Level, int32 Amount)
{
	FArcItemSpec NewEntry;
	
	NewEntry.ItemId = FArcItemId::Generate();
	NewEntry.Level = Level;
	NewEntry.Amount = Amount;
	NewEntry.SetItemDefinition(NewItem);

	return NewEntry;
}

namespace UE::Net
{
	struct FArcItemSpecFragmentInstancesNetSerializer : public TPolymorphicArrayStructNetSerializerImpl<FArcItemSpecFragmentInstances, FArcItemFragment_ItemInstanceBase, FArcItemSpecFragmentInstances::GetArray, FArcItemSpecFragmentInstances::SetArrayNum>
	{
		using InternalNetSerializerType = TPolymorphicArrayStructNetSerializerImpl<FArcItemSpecFragmentInstances, FArcItemFragment_ItemInstanceBase, FArcItemSpecFragmentInstances::GetArray, FArcItemSpecFragmentInstances::SetArrayNum>;

		static const uint32 Version = 0;
		static const ConfigType DefaultConfig;

	private:
		class FNetSerializerRegistryDelegates final : private UE::Net::FNetSerializerRegistryDelegates
		{
		public:
			FNetSerializerRegistryDelegates();
			virtual ~FNetSerializerRegistryDelegates();

		private:
			virtual void OnPreFreezeNetSerializerRegistry() override;
			virtual void OnPostFreezeNetSerializerRegistry() override;
			virtual void OnLoadedModulesUpdated() override;
		};

		static FArcItemSpecFragmentInstancesNetSerializer::FNetSerializerRegistryDelegates NetSerializerRegistryDelegates;
		static bool bIsPostFreezeCalled;

	public:
		static void InitTypeCache();
	};
	bool FArcItemSpecFragmentInstancesNetSerializer::bIsPostFreezeCalled = false;
	FArcItemSpecFragmentInstancesNetSerializer::FNetSerializerRegistryDelegates FArcItemSpecFragmentInstancesNetSerializer::NetSerializerRegistryDelegates;
	static const FName PropertyNetSerializerRegistry_NAME_ArcItemSpecFragmentInstances(TEXT("ArcItemSpecFragmentInstances"));
	UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemSpecFragmentInstances, FArcItemSpecFragmentInstancesNetSerializer);

	const FArcItemSpecFragmentInstancesNetSerializer::ConfigType FArcItemSpecFragmentInstancesNetSerializer::DefaultConfig;

	UE_NET_IMPLEMENT_SERIALIZER(FArcItemSpecFragmentInstancesNetSerializer);
	
	void InitArcItemSpecFragmentInstancesNetSerializerNetSerializerTypeCache()
	{
		UE::Net::FArcItemSpecFragmentInstancesNetSerializer::InitTypeCache();
	}

	void FArcItemSpecFragmentInstancesNetSerializer::InitTypeCache()
	{
		// When post freeze is called we expect all custom serializers to have been registered
		// so that the type cache will get the appropriate serializer when creating the ReplicationStateDescriptor.
		if (bIsPostFreezeCalled)
		{
			InternalNetSerializerType::InitTypeCache<FArcItemSpecFragmentInstancesNetSerializer>();
		}
	}

	FArcItemSpecFragmentInstancesNetSerializer::FNetSerializerRegistryDelegates::FNetSerializerRegistryDelegates()
		: UE::Net::FNetSerializerRegistryDelegates(EFlags::ShouldBindLoadedModulesUpdatedDelegate)
	{
	}

	FArcItemSpecFragmentInstancesNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
	{
		UE_NET_UNREGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemSpecFragmentInstances);
	}

	void FArcItemSpecFragmentInstancesNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
	{
		UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemSpecFragmentInstances);
	}

	void FArcItemSpecFragmentInstancesNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
	{
		bIsPostFreezeCalled = true;
	}

	void FArcItemSpecFragmentInstancesNetSerializer::FNetSerializerRegistryDelegates::OnLoadedModulesUpdated()
	{
		InitArcItemSpecFragmentInstancesNetSerializerNetSerializerTypeCache();
	}
}
