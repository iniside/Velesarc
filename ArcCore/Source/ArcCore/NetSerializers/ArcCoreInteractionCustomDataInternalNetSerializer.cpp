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

#include "ArcCoreInteractionCustomDataInternalNetSerializer.h"

#include "Interaction/ArcCoreInteractionCustomData.h"
#include "..\Interaction\ArcInteractionLevelPlacedComponent.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/Serialization/PolymorphicNetSerializerImpl.h"

namespace Arcx::Net
{
	using namespace UE::Net;

	struct FArcCoreInteractionCustomDataInternalForNetSerializer
	{
		static TSharedPtr<FArcCoreInteractionCustomData>& GetItem(FArcCoreInteractionCustomDataInternal& Source)
		{
			return Source.CustomData;
		}
	};

	struct FArcCoreInteractionCustomDataInternalNetSerializer
			: TPolymorphicStructNetSerializerImpl<
				FArcCoreInteractionCustomDataInternal, FArcCoreInteractionCustomData, FArcCoreInteractionCustomDataInternalForNetSerializer::GetItem>
	{
		using InternalNetSerializerType = TPolymorphicStructNetSerializerImpl<
			FArcCoreInteractionCustomDataInternal, FArcCoreInteractionCustomData, FArcCoreInteractionCustomDataInternalForNetSerializer::GetItem>;
		using ConfigType = FArcCoreInteractionCustomDataInternalNetSerializerConfig;

		static const uint32 Version = 0;
		static const ConfigType DefaultConfig;

		static void InitTypeCache();

	private:
		class FNetSerializerRegistryDelegates final : private UE::Net::FNetSerializerRegistryDelegates
		{
		public:
			FNetSerializerRegistryDelegates();
			virtual ~FNetSerializerRegistryDelegates() override;

		private:
			virtual void OnPreFreezeNetSerializerRegistry() override;

			virtual void OnPostFreezeNetSerializerRegistry() override;
		};

		static FArcCoreInteractionCustomDataInternalNetSerializer::FNetSerializerRegistryDelegates NetSerializerRegistryDelegates;
		static bool bIsPostFreezeCalled;
	};

	UE_NET_IMPLEMENT_SERIALIZER(FArcCoreInteractionCustomDataInternalNetSerializer);

	const FArcCoreInteractionCustomDataInternalNetSerializer::ConfigType FArcCoreInteractionCustomDataInternalNetSerializer::DefaultConfig;
	FArcCoreInteractionCustomDataInternalNetSerializer::FNetSerializerRegistryDelegates
	FArcCoreInteractionCustomDataInternalNetSerializer::NetSerializerRegistryDelegates;
	bool FArcCoreInteractionCustomDataInternalNetSerializer::bIsPostFreezeCalled = false;

	void FArcCoreInteractionCustomDataInternalNetSerializer::InitTypeCache()
	{
		// When post freeze is called we expect all custom serializers to have been
		// registered so that the type cache will get the appropriate serializer when
		// creating the ReplicationStateDescriptor.
		if (bIsPostFreezeCalled)
		{
			InternalNetSerializerType::InitTypeCache<FArcCoreInteractionCustomDataInternalNetSerializer>();
		}
	}

	static const FName PropertyNetSerializerRegistry_NAME_ArcCoreInteractionCustomDataInternal("ArcCoreInteractionCustomDataInternal");
	UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcCoreInteractionCustomDataInternal
		, FArcCoreInteractionCustomDataInternalNetSerializer);

	void InitArcCoreInteractionCustomDataInternalNetSerializerTypeCache()
	{
		Arcx::Net::FArcCoreInteractionCustomDataInternalNetSerializer::InitTypeCache();
	}
	FArcCoreInteractionCustomDataInternalNetSerializer::FNetSerializerRegistryDelegates::FNetSerializerRegistryDelegates()
		: UE::Net::FNetSerializerRegistryDelegates(EFlags::ShouldBindLoadedModulesUpdatedDelegate)
	{

	}
	FArcCoreInteractionCustomDataInternalNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
	{
		UE_NET_UNREGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcCoreInteractionCustomDataInternal);
	}

	void FArcCoreInteractionCustomDataInternalNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
	{
		UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcCoreInteractionCustomDataInternal);
	}

	void FArcCoreInteractionCustomDataInternalNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
	{
		bIsPostFreezeCalled = true;
		InitArcCoreInteractionCustomDataInternalNetSerializerTypeCache();
	}
}
