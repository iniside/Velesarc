// Copyright Lukasz Baran. All Rights Reserved.

#include "NetSerializers/ArcMassEntityHandleIrisSerializer.h"
#include "NetSerializers/ArcMassEntityNetDataStore.h"
#include "Fragments/ArcMassNetId.h"
#include "Subsystem/ArcMassEntityReplicationSubsystem.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/Serialization/NetSerializerDelegates.h"

namespace ArcMassReplication
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	namespace EntityHandleNetSerializerPrivate
	{
		UArcMassEntityReplicationSubsystem* GetReplicationSubsystem(FNetSerializationContext& Context)
		{
			FNetTokenStore* TokenStore = Context.GetNetTokenStore();
			if (TokenStore == nullptr)
			{
				return nullptr;
			}

			FArcMassEntityNetDataStore* DataStore = TokenStore->GetDataStore<FArcMassEntityNetDataStore>();
			if (DataStore == nullptr)
			{
				return nullptr;
			}

			return DataStore->GetSubsystem();
		}
	}

	void FArcMassEntityHandleNetSerializer::Serialize(FNetSerializationContext& Context, const FNetSerializeArgs& Args)
	{
		const QuantizedType* Source = reinterpret_cast<const QuantizedType*>(Args.Source);
		FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();
		Writer->WriteBits(Source->NetId, 32U);
	}

	void FArcMassEntityHandleNetSerializer::Deserialize(FNetSerializationContext& Context, const FNetDeserializeArgs& Args)
	{
		QuantizedType* Target = reinterpret_cast<QuantizedType*>(Args.Target);
		FNetBitStreamReader* Reader = Context.GetBitStreamReader();
		Target->NetId = Reader->ReadBits(32U);
	}

	void FArcMassEntityHandleNetSerializer::Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args)
	{
		QuantizedType* Target = reinterpret_cast<QuantizedType*>(Args.Target);
		const SourceType* Source = reinterpret_cast<const SourceType*>(Args.Source);

		Target->NetId = 0;

		if (Source->IsSet())
		{
			UArcMassEntityReplicationSubsystem* Subsystem = EntityHandleNetSerializerPrivate::GetReplicationSubsystem(Context);
			if (Subsystem != nullptr)
			{
				FArcMassNetId NetId = Subsystem->FindNetId(*Source);
				Target->NetId = NetId.GetValue();
			}
		}
	}

	void FArcMassEntityHandleNetSerializer::Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args)
	{
		const QuantizedType* Source = reinterpret_cast<const QuantizedType*>(Args.Source);
		SourceType* Target = reinterpret_cast<SourceType*>(Args.Target);

		Target->Reset();

		if (Source->NetId != 0)
		{
			UArcMassEntityReplicationSubsystem* Subsystem = EntityHandleNetSerializerPrivate::GetReplicationSubsystem(Context);
			if (Subsystem != nullptr)
			{
				*Target = Subsystem->FindEntityByNetId(FArcMassNetId(Source->NetId));
			}
		}
	}

	void FArcMassEntityHandleNetSerializer::SerializeDelta(FNetSerializationContext& Context, const FNetSerializeDeltaArgs& Args)
	{
		NetSerializeDeltaDefault<Serialize>(Context, Args);
	}

	void FArcMassEntityHandleNetSerializer::DeserializeDelta(FNetSerializationContext& Context, const FNetDeserializeDeltaArgs& Args)
	{
		NetDeserializeDeltaDefault<Deserialize>(Context, Args);
	}

	void FArcMassEntityHandleNetSerializer::CloneStructMember(FNetSerializationContext& Context, const FNetCloneDynamicStateArgs& CloneArgs)
	{
	}

	bool FArcMassEntityHandleNetSerializer::IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args)
	{
		const QuantizedType& Value0 = *reinterpret_cast<const QuantizedType*>(Args.Source0);
		const QuantizedType& Value1 = *reinterpret_cast<const QuantizedType*>(Args.Source1);
		return Value0.NetId == Value1.NetId;
	}

	bool FArcMassEntityHandleNetSerializer::Validate(FNetSerializationContext& Context, const FNetValidateArgs& Args)
	{
		return true;
	}

	UE_NET_IMPLEMENT_SERIALIZER(FArcMassEntityHandleNetSerializer);
	FArcMassEntityHandleNetSerializer::ConfigType FArcMassEntityHandleNetSerializer::DefaultConfig = FArcMassEntityHandleNetSerializer::ConfigType();

	FArcMassEntityHandleNetSerializer::FNetSerializerRegistryDelegates FArcMassEntityHandleNetSerializer::NetSerializerRegistryDelegates;
	static const FName PropertyNetSerializerRegistry_NAME_MassEntityHandle("MassEntityHandle");
	UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_MassEntityHandle, FArcMassEntityHandleNetSerializer);

	FArcMassEntityHandleNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
	{
		UE_NET_UNREGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_MassEntityHandle);
	}

	void FArcMassEntityHandleNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
	{
		UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_MassEntityHandle);
	}

	void FArcMassEntityHandleNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
	{
	}
} // namespace ArcMassReplication
