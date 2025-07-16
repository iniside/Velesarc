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

#pragma once
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Iris/ReplicationState/Private/IrisFastArraySerializerInternal.h"
#include "Iris/Serialization/NetSerializer.h"
#include "Iris/Serialization/NetSerializerConfig.h"
#include "Iris/Serialization/NetSerializers.h"
// #include "Iris/Serialization/PolymorphicNetSerializer.h"
// #include "Iris/Serialization/PolymorphicNetSerializerImpl.h"
#include "ArcPolymorphicStructNetSerializer.h"
#include "Iris/Serialization/NetSerializerDelegates.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsArray.h"

#include "ArcItemDataNetSerializer.generated.h"

USTRUCT()
struct FArcItemDataPtrSerializerConfig : public FArcPolymorphicStructNetSerializerConfig
{
	GENERATED_BODY()
};

USTRUCT()
struct FArcItemDataSerializerConfig : public FStructNetSerializerConfig
{
	GENERATED_BODY()
};

template <>
struct TStructOpsTypeTraits<FArcItemDataSerializerConfig>
		: public TStructOpsTypeTraitsBase2<FArcItemDataSerializerConfig>
{
	enum
	{
		WithCopy = false
	};
};

namespace Arcx::Net
{
	struct FItemEntryPtrAccessorForNetSerializer
	{
	public:
		static TSharedPtr<FArcItemData>& GetItem(FArcItemDataInternal& Source)
		{
			return Source.GetItem();
		}
	};

	struct FArcItemDataPtrNetSerializer
			: public UE::Net::TPolymorphicStructNetSerializerImpl<
				FArcItemDataInternal, FArcItemData, FItemEntryPtrAccessorForNetSerializer::GetItem>
	{
	public:
		using InternalNetSerializerType = UE::Net::TPolymorphicStructNetSerializerImpl<
			FArcItemDataInternal, FArcItemData, FItemEntryPtrAccessorForNetSerializer::GetItem>;

		// Version
		static const uint32 Version = 0;
		static constexpr bool bUseDefaultDelta = true;

		//static void InitTypeCache();

		using ConfigType = InternalNetSerializerType::ConfigType;

		static const ConfigType DefaultConfig;

		class FNetSerializerRegistryDelegates final : private UE::Net::FNetSerializerRegistryDelegates
		{
		public:
			virtual ~FNetSerializerRegistryDelegates() override;

		private:
			virtual void OnPreFreezeNetSerializerRegistry() override;

			virtual void OnPostFreezeNetSerializerRegistry() override;
		};

		static bool bIsPostFreezeCalled;
		static FArcItemDataPtrNetSerializer::FNetSerializerRegistryDelegates NetSerializerRegistryDelegates;
	};

	UE_NET_DECLARE_SERIALIZER(FArcItemDataPtrNetSerializer
		, ARCCORE_API);

	//UE_NET_DECLARE_SERIALIZER(FArcItemDataNetSerializer
	//	, ARCCORE_API);
}
