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

#include "ArcItemInstanceNetSerializer.h"

#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"

namespace Arcx::Net
{
	UE_NET_IMPLEMENT_SERIALIZER(FArcItemInstancePtrNetSerializer);
	
	void FArcItemInstancePtrNetSerializer::InitTypeCache()
	{
		// When post freeze is called we expect all custom serializers to have been
		// registered so that the type cache will get the appropriate serializer when
		// creating the ReplicationStateDescriptor.
		if (bIsPostFreezeCalled)
		{
			InternalItemInstanceSerializer::InitTypeCache<FArcItemInstancePtrNetSerializer>();
		}
	}
	
	const FArcItemInstancePtrNetSerializer::ConfigType FArcItemInstancePtrNetSerializer::DefaultConfig =
			FArcItemInstancePtrNetSerializer::ConfigType();
	
	FArcItemInstancePtrNetSerializer::FNetSerializerRegistryDelegates FArcItemInstancePtrNetSerializer::NetSerializerRegistryDelegates;
	bool FArcItemInstancePtrNetSerializer::bIsPostFreezeCalled = false;

	static const FName PropertyNetSerializerRegistry_NAME_ArcItemInstanceInternal("ArcItemInstanceInternal");
	UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemInstanceInternal
		, FArcItemInstancePtrNetSerializer);
	
	void InitItemInstanceNetSerializerTypeCache()
	{
		Arcx::Net::FArcItemInstancePtrNetSerializer::bIsPostFreezeCalled = true;
		Arcx::Net::FArcItemInstancePtrNetSerializer::InitTypeCache();
	}
	FArcItemInstancePtrNetSerializer::FNetSerializerRegistryDelegates::FNetSerializerRegistryDelegates()
	: UE::Net::FNetSerializerRegistryDelegates(EFlags::ShouldBindLoadedModulesUpdatedDelegate)
	{
	}
	
	FArcItemInstancePtrNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
	{
		UE_NET_UNREGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemInstanceInternal);
	}
	
	void FArcItemInstancePtrNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
	{
		UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemInstanceInternal);
	}

	void FArcItemInstancePtrNetSerializer::FNetSerializerRegistryDelegates::OnLoadedModulesUpdated()
	{
		InitItemInstanceNetSerializerTypeCache();
	}
	
	void FArcItemInstancePtrNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
	{
		Arcx::Net::FArcItemInstancePtrNetSerializer::bIsPostFreezeCalled = true;
	}
}
