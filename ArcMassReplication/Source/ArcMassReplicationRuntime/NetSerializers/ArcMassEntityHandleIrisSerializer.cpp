// Copyright Lukasz Baran. All Rights Reserved.

#include "NetSerializers/ArcMassEntityHandleIrisSerializer.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/Serialization/ObjectNetSerializer.h"
#include "Replication/ArcMassEntityVessel.h"
#include "Subsystem/ArcMassEntityReplicationSubsystem.h"

namespace ArcMassReplication
{
	using namespace UE::Net;

	namespace ArcMassEntityHandleNetSerializer_Private
	{
		// Forward to Iris's standard FObjectNetSerializer for everything that touches
		// the wire / object reference cache. The serializer is registered globally so
		// we can resolve it by name.
		const FNetSerializer& GetObjectSerializer()
		{
			return UE_NET_GET_SERIALIZER(FObjectNetSerializer);
		}

		const FNetSerializerConfig* GetObjectSerializerConfig()
		{
			return UE_NET_GET_SERIALIZER_DEFAULT_CONFIG(FObjectNetSerializer);
		}

		// Serializer is invoked from the replication system without direct world context.
		// Mirror the FindGameWorld approach used by the root factory until a proper
		// world-from-context path is available on this engine branch.
		// TODO: Phase 7+ — derive world from FNetSerializationContext / replication system
		// once the engine surfaces it publicly.
		UWorld* FindGameWorld()
		{
			if (GEngine == nullptr)
			{
				return nullptr;
			}
			UWorld* FirstMatch = nullptr;
			for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
			{
				UWorld* World = WorldContext.World();
				if (World == nullptr)
				{
					continue;
				}
				if (WorldContext.WorldType != EWorldType::Game && WorldContext.WorldType != EWorldType::PIE)
				{
					continue;
				}
				if (FirstMatch == nullptr)
				{
					FirstMatch = World;
				}
			}
			return FirstMatch;
		}

		UArcMassEntityReplicationSubsystem* GetSubsystem()
		{
			UWorld* World = FindGameWorld();
			return World != nullptr ? World->GetSubsystem<UArcMassEntityReplicationSubsystem>() : nullptr;
		}
	}

	void FArcMassEntityHandleNetSerializer::Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args)
	{
		const FMassEntityHandle& SourceHandle = *reinterpret_cast<const FMassEntityHandle*>(Args.Source);

		// Translate FMassEntityHandle -> UArcMassEntityVessel*. Forward to FObjectNetSerializer
		// which produces a FNetObjectReference into the quantized buffer. If no vessel exists
		// (e.g. handle is invalid or entity isn't replicated), we still forward with nullptr
		// so the buffer gets a well-defined invalid-reference quantization.
		UObject* VesselObject = nullptr;
		if (SourceHandle.IsSet())
		{
			if (UArcMassEntityReplicationSubsystem* Subsystem = ArcMassEntityHandleNetSerializer_Private::GetSubsystem())
			{
				VesselObject = Subsystem->FindVesselForEntity(SourceHandle);
			}
		}

		FNetQuantizeArgs ForwardArgs = Args;
		ForwardArgs.NetSerializerConfig = ArcMassEntityHandleNetSerializer_Private::GetObjectSerializerConfig();
		ForwardArgs.Source = NetSerializerValuePointer(&VesselObject);
		ArcMassEntityHandleNetSerializer_Private::GetObjectSerializer().Quantize(Context, ForwardArgs);
	}

	void FArcMassEntityHandleNetSerializer::Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args)
	{
		FMassEntityHandle& TargetHandle = *reinterpret_cast<FMassEntityHandle*>(Args.Target);
		TargetHandle = FMassEntityHandle();

		// Forward to FObjectNetSerializer to resolve the FNetObjectReference -> UObject*.
		// If unresolved, ResolvedObject will be null; Iris re-tries because we declared
		// bHasCustomNetReference and CollectNetReferences registers the reference.
		UObject* ResolvedObject = nullptr;
		FNetDequantizeArgs ForwardArgs = Args;
		ForwardArgs.NetSerializerConfig = ArcMassEntityHandleNetSerializer_Private::GetObjectSerializerConfig();
		ForwardArgs.Target = NetSerializerValuePointer(&ResolvedObject);
		ArcMassEntityHandleNetSerializer_Private::GetObjectSerializer().Dequantize(Context, ForwardArgs);

		if (UArcMassEntityVessel* Vessel = Cast<UArcMassEntityVessel>(ResolvedObject))
		{
			TargetHandle = Vessel->EntityHandle;
		}
	}

	void FArcMassEntityHandleNetSerializer::Serialize(FNetSerializationContext& Context, const FNetSerializeArgs& Args)
	{
		FNetSerializeArgs ForwardArgs = Args;
		ForwardArgs.NetSerializerConfig = ArcMassEntityHandleNetSerializer_Private::GetObjectSerializerConfig();
		ArcMassEntityHandleNetSerializer_Private::GetObjectSerializer().Serialize(Context, ForwardArgs);
	}

	void FArcMassEntityHandleNetSerializer::Deserialize(FNetSerializationContext& Context, const FNetDeserializeArgs& Args)
	{
		FNetDeserializeArgs ForwardArgs = Args;
		ForwardArgs.NetSerializerConfig = ArcMassEntityHandleNetSerializer_Private::GetObjectSerializerConfig();
		ArcMassEntityHandleNetSerializer_Private::GetObjectSerializer().Deserialize(Context, ForwardArgs);
	}

	// Forwarding serializers are required by NetSerializerBuilder to implement
	// SerializeDelta / DeserializeDelta even when bUseDefaultDelta = true. We
	// forward to FObjectNetSerializer's Delta entry points (which Iris itself
	// synthesizes from its Serialize + IsEqual via NetSerializeDeltaDefault).
	void FArcMassEntityHandleNetSerializer::SerializeDelta(FNetSerializationContext& Context, const FNetSerializeDeltaArgs& Args)
	{
		FNetSerializeDeltaArgs ForwardArgs = Args;
		ForwardArgs.NetSerializerConfig = ArcMassEntityHandleNetSerializer_Private::GetObjectSerializerConfig();
		ArcMassEntityHandleNetSerializer_Private::GetObjectSerializer().SerializeDelta(Context, ForwardArgs);
	}

	void FArcMassEntityHandleNetSerializer::DeserializeDelta(FNetSerializationContext& Context, const FNetDeserializeDeltaArgs& Args)
	{
		FNetDeserializeDeltaArgs ForwardArgs = Args;
		ForwardArgs.NetSerializerConfig = ArcMassEntityHandleNetSerializer_Private::GetObjectSerializerConfig();
		ArcMassEntityHandleNetSerializer_Private::GetObjectSerializer().DeserializeDelta(Context, ForwardArgs);
	}

	bool FArcMassEntityHandleNetSerializer::IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args)
	{
		if (Args.bStateIsQuantized)
		{
			FNetIsEqualArgs ForwardArgs = Args;
			ForwardArgs.NetSerializerConfig = ArcMassEntityHandleNetSerializer_Private::GetObjectSerializerConfig();
			return ArcMassEntityHandleNetSerializer_Private::GetObjectSerializer().IsEqual(Context, ForwardArgs);
		}
		const FMassEntityHandle& A = *reinterpret_cast<const FMassEntityHandle*>(Args.Source0);
		const FMassEntityHandle& B = *reinterpret_cast<const FMassEntityHandle*>(Args.Source1);
		return A == B;
	}

	bool FArcMassEntityHandleNetSerializer::Validate(FNetSerializationContext& Context, const FNetValidateArgs& Args)
	{
		// Source-side validation only — the FMassEntityHandle itself is always
		// structurally valid (POD); the vessel lookup happens during Quantize.
		return true;
	}

	void FArcMassEntityHandleNetSerializer::CloneDynamicState(FNetSerializationContext& Context, const FNetCloneDynamicStateArgs& Args)
	{
		FNetCloneDynamicStateArgs ForwardArgs = Args;
		ForwardArgs.NetSerializerConfig = ArcMassEntityHandleNetSerializer_Private::GetObjectSerializerConfig();
		ArcMassEntityHandleNetSerializer_Private::GetObjectSerializer().CloneDynamicState(Context, ForwardArgs);
	}

	void FArcMassEntityHandleNetSerializer::FreeDynamicState(FNetSerializationContext& Context, const FNetFreeDynamicStateArgs& Args)
	{
		FNetFreeDynamicStateArgs ForwardArgs = Args;
		ForwardArgs.NetSerializerConfig = ArcMassEntityHandleNetSerializer_Private::GetObjectSerializerConfig();
		ArcMassEntityHandleNetSerializer_Private::GetObjectSerializer().FreeDynamicState(Context, ForwardArgs);
	}

	void FArcMassEntityHandleNetSerializer::CollectNetReferences(FNetSerializationContext& Context, const FNetCollectReferencesArgs& Args)
	{
		FNetCollectReferencesArgs ForwardArgs = Args;
		ForwardArgs.NetSerializerConfig = ArcMassEntityHandleNetSerializer_Private::GetObjectSerializerConfig();
		ArcMassEntityHandleNetSerializer_Private::GetObjectSerializer().CollectNetReferences(Context, ForwardArgs);
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
