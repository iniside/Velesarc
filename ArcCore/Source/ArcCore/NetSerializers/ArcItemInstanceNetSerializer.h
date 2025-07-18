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
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Iris/ReplicationState/Private/IrisFastArraySerializerInternal.h"
#include "Iris/Serialization/NetSerializer.h"
#include "Iris/Serialization/NetSerializerConfig.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/Serialization/NetSerializers.h"
// #include "Iris/Serialization/PolymorphicNetSerializer.h"
// #include "Iris/Serialization/PolymorphicNetSerializerImpl.h"
// #include "ArcPolymorphicStructNetSerializer.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemInstance.h"
#include "Items/ArcItemsArray.h"
#include "ArcItemInstanceNetSerializer.generated.h"

//USTRUCT()
//struct FArcItemInstancePtrSerializerConfig : public FArcPolymorphicStructNetSerializerConfig
//{
//	GENERATED_BODY()
//};

USTRUCT()
struct FArcItemInstanceSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()
};

namespace Arcx::Net
{
	struct FArcItemInstanceAccessorForNetSerializer
	{
	public:
		static TSharedPtr<FArcItemInstance>& GetItem(FArcItemInstanceInternal& Source)
		{
			return Source.Data;
		}
	};
	
	struct FArcItemInstancePtrNetSerializer
			: UE::Net::TPolymorphicStructNetSerializerImpl<
				FArcItemInstanceInternal, FArcItemInstance, FArcItemInstanceAccessorForNetSerializer::GetItem>
	{
	public:
		using InternalItemInstanceSerializer = UE::Net::TPolymorphicStructNetSerializerImpl<
			FArcItemInstanceInternal, FArcItemInstance, FArcItemInstanceAccessorForNetSerializer::GetItem>;
	
		// Version
		static const uint32 Version = 0;
		static constexpr bool bUseDefaultDelta = true;
	
		static void InitTypeCache();
	
		// Traits
		using ConfigType = InternalItemInstanceSerializer::ConfigType;// FArcItemInstancePtrSerializerConfig;
	
		static const ConfigType DefaultConfig;
	
		class FNetSerializerRegistryDelegates final : private UE::Net::FNetSerializerRegistryDelegates
		{
		public:
			FNetSerializerRegistryDelegates();
			
			virtual ~FNetSerializerRegistryDelegates() override;
	
		private:
			virtual void OnPreFreezeNetSerializerRegistry() override;
	
			virtual void OnPostFreezeNetSerializerRegistry() override;
			
			virtual void OnLoadedModulesUpdated() override;
		};
	
		static bool bIsPostFreezeCalled;
		static FArcItemInstancePtrNetSerializer::FNetSerializerRegistryDelegates NetSerializerRegistryDelegates;
	};
	
	UE_NET_DECLARE_SERIALIZER(FArcItemInstancePtrNetSerializer, ARCCORE_API);
}
