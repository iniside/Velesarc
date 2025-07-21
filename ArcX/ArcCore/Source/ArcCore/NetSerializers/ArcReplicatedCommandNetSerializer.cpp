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

#include "ArcReplicatedCommandNetSerializer.h"

#include "GameFeaturesSubsystem.h"
#include "Commands/ArcReplicatedCommand.h"
#include "Engine/Engine.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/Serialization/PolymorphicNetSerializerImpl.h"

namespace Arcx::Net
{
	FSimpleDelegate Delegates::OnGameFeaturePluginLoaded = FSimpleDelegate();
	using namespace UE::Net;

	struct FGameplayEffectContextHandleAccessorForNetSerializer
	{
		static TSharedPtr<FArcReplicatedCommand>& GetItem(FArcReplicatedCommandHandle& Source)
		{
			return Source.GetData();
		}
	};

	struct FArcReplicatedCommandNetSerializer
			: UE::Net::TPolymorphicStructNetSerializerImpl<
				FArcReplicatedCommandHandle, FArcReplicatedCommand,
				FGameplayEffectContextHandleAccessorForNetSerializer::GetItem>
	{
		using InternalNetSerializerType = UE::Net::TPolymorphicStructNetSerializerImpl<
			FArcReplicatedCommandHandle, FArcReplicatedCommand,
			FGameplayEffectContextHandleAccessorForNetSerializer::GetItem>;
		using ConfigType = FArcReplicatedCommandNetSerializerConfig;

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
			
			virtual void OnLoadedModulesUpdated() override;
		};

		static FArcReplicatedCommandNetSerializer::FNetSerializerRegistryDelegates NetSerializerRegistryDelegates;
		static bool bIsPostFreezeCalled;
	};

	UE_NET_IMPLEMENT_SERIALIZER(FArcReplicatedCommandNetSerializer);

	const FArcReplicatedCommandNetSerializer::ConfigType FArcReplicatedCommandNetSerializer::DefaultConfig;
	FArcReplicatedCommandNetSerializer::FNetSerializerRegistryDelegates
	FArcReplicatedCommandNetSerializer::NetSerializerRegistryDelegates;
	bool FArcReplicatedCommandNetSerializer::bIsPostFreezeCalled = false;

	void FArcReplicatedCommandNetSerializer::InitTypeCache()
	{
		if (GEngine)
		{
			UGameFeaturesSubsystem* GFS = GEngine->GetEngineSubsystem<UGameFeaturesSubsystem>();
			
		}
		// When post freeze is called we expect all custom serializers to have been
		// registered so that the type cache will get the appropriate serializer when
		// creating the ReplicationStateDescriptor.
		if (bIsPostFreezeCalled)
		{
			InternalNetSerializerType::InitTypeCache<FArcReplicatedCommandNetSerializer>();
		}
	}

	void InitArcReplicatedCommandNetNetSerializerTypeCache()
	{
		FArcReplicatedCommandNetSerializer::InitTypeCache();
		Delegates::OnGameFeaturePluginLoaded.BindStatic(&FArcReplicatedCommandNetSerializer::InitTypeCache);
	}

	static const FName PropertyNetSerializerRegistry_NAME_ArcReplicatedCommandHandle("ArcReplicatedCommandHandle");
	UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcReplicatedCommandHandle
		, FArcReplicatedCommandNetSerializer);

	FArcReplicatedCommandNetSerializer::FNetSerializerRegistryDelegates::FNetSerializerRegistryDelegates()
	: UE::Net::FNetSerializerRegistryDelegates(EFlags::ShouldBindLoadedModulesUpdatedDelegate)
	{
	}
	
	FArcReplicatedCommandNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
	{
		UE_NET_UNREGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcReplicatedCommandHandle);
	}

	void FArcReplicatedCommandNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
	{
		UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcReplicatedCommandHandle);
	}

	void FArcReplicatedCommandNetSerializer::FNetSerializerRegistryDelegates::OnLoadedModulesUpdated()
	{
		InitArcReplicatedCommandNetNetSerializerTypeCache();
	}
	
	void FArcReplicatedCommandNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
	{
		bIsPostFreezeCalled = true;
	}
}
