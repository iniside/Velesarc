/**
 * This file is part of ArcX.
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

#include "ArcItemSpecNetSerializer.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
// #include "Iris/Serialization/PolymorphicNetSerializerImpl.h"
#include "Iris/Serialization/InternalNetSerializers.h"

#include "Iris/ReplicationSystem/ReplicationOperationsInternal.h"

//namespace Arcx::Net
//{
//	// using namespace UE::Net;
//
//	UE_NET_IMPLEMENT_SERIALIZER(FArcItemSpecPtrNetSerializer);
//
//	void FArcItemSpecPtrNetSerializer::InitTypeCache()
//	{
//		// When post freeze is called we expect all custom serializers to have been
//		// registered so that the type cache will get the appropriate serializer when
//		// creating the ReplicationStateDescriptor.
//		if (bIsPostFreezeCalled)
//		{
//			InternalNetSerializerType::InitTypeCache<FArcItemSpecPtrNetSerializer>();
//		}
//	}
//
//	const FArcItemSpecPtrNetSerializer::ConfigType FArcItemSpecPtrNetSerializer::DefaultConfig =
//			FArcItemSpecPtrNetSerializer::ConfigType();
//
//	FArcItemSpecPtrNetSerializer::FNetSerializerRegistryDelegates FArcItemSpecPtrNetSerializer::NetSerializerRegistryDelegates;
//	bool FArcItemSpecPtrNetSerializer::bIsPostFreezeCalled = false;
//
//	//static const FName PropertyNetSerializerRegistry_NAME_ArcItemSpec("ArcItemSpec");
//	// UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemEntryInternal,
//	//FArcItemDataInternalNetSerializer);
//
//	void InitArcItemSpecPtrNetSerializerTypeCache()
//	{
//		Arcx::Net::FArcItemSpecPtrNetSerializer::bIsPostFreezeCalled = true;
//		Arcx::Net::FArcItemSpecPtrNetSerializer::InitTypeCache();
//	}
//
//	FArcItemSpecPtrNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
//	{
//	}
//
//	void FArcItemSpecPtrNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
//	{
//	}
//
//	void FArcItemSpecPtrNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
//	{
//		InitArcItemSpecPtrNetSerializerTypeCache();
//	}
//}
