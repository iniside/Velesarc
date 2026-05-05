// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassTestReplicatedArrayWrapperNetSerializer.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Replication/ArcIrisReplicatedArray.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcMassTestReplicatedArrayWrapper, Verbose, All);

namespace ArcMassTestReplication
{
	using namespace UE::Net;
	using FInner = ArcMassReplication::FArcIrisReplicatedArrayNetSerializer;

	static const UE::Net::FNetSerializer& InnerSerializer()
	{
		return UE_NET_GET_SERIALIZER(ArcMassReplication::FArcIrisReplicatedArrayNetSerializer);
	}

	static NetSerializerValuePointer OffsetPtr(NetSerializerValuePointer Base, uint32 Offset)
	{
		return Base + static_cast<NetSerializerValuePointer>(Offset);
	}

	void FArcMassTestReplicatedArrayWrapperNetSerializer::Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args)
	{
		const ConfigType& Cfg = *static_cast<const ConfigType*>(Args.NetSerializerConfig);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Log, TEXT("Quantize: enter Source=0x%llx Target=0x%llx InnerOffset=%u InnerItemDescriptor=%s"),
			(uint64)Args.Source, (uint64)Args.Target, Cfg.InnerMemberOffset,
			Cfg.InnerConfig.ItemDescriptor.IsValid() ? TEXT("valid") : TEXT("null"));
		FNetQuantizeArgs Forwarded = Args;
		Forwarded.NetSerializerConfig = &Cfg.InnerConfig;
		Forwarded.Source = OffsetPtr(Args.Source, Cfg.InnerMemberOffset);
		InnerSerializer().Quantize(Context, Forwarded);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Log, TEXT("Quantize: exit HasError=%d"), Context.HasErrorOrOverflow() ? 1 : 0);
	}

	void FArcMassTestReplicatedArrayWrapperNetSerializer::Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args)
	{
		const ConfigType& Cfg = *static_cast<const ConfigType*>(Args.NetSerializerConfig);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Log, TEXT("Dequantize: enter Source=0x%llx Target=0x%llx InnerOffset=%u"),
			(uint64)Args.Source, (uint64)Args.Target, Cfg.InnerMemberOffset);
		FNetDequantizeArgs Forwarded = Args;
		Forwarded.NetSerializerConfig = &Cfg.InnerConfig;
		Forwarded.Target = OffsetPtr(Args.Target, Cfg.InnerMemberOffset);
		InnerSerializer().Dequantize(Context, Forwarded);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Log, TEXT("Dequantize: exit HasError=%d"), Context.HasErrorOrOverflow() ? 1 : 0);
	}

	void FArcMassTestReplicatedArrayWrapperNetSerializer::Serialize(FNetSerializationContext& Context, const FNetSerializeArgs& Args)
	{
		const ConfigType& Cfg = *static_cast<const ConfigType*>(Args.NetSerializerConfig);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Log, TEXT("Serialize: enter Source=0x%llx"), (uint64)Args.Source);
		FNetSerializeArgs Forwarded = Args;
		Forwarded.NetSerializerConfig = &Cfg.InnerConfig;
		InnerSerializer().Serialize(Context, Forwarded);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Log, TEXT("Serialize: exit HasError=%d"), Context.HasErrorOrOverflow() ? 1 : 0);
	}

	void FArcMassTestReplicatedArrayWrapperNetSerializer::Deserialize(FNetSerializationContext& Context, const FNetDeserializeArgs& Args)
	{
		const ConfigType& Cfg = *static_cast<const ConfigType*>(Args.NetSerializerConfig);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Log, TEXT("Deserialize: enter Target=0x%llx"), (uint64)Args.Target);
		FNetDeserializeArgs Forwarded = Args;
		Forwarded.NetSerializerConfig = &Cfg.InnerConfig;
		InnerSerializer().Deserialize(Context, Forwarded);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Log, TEXT("Deserialize: exit HasError=%d"), Context.HasErrorOrOverflow() ? 1 : 0);
	}

	void FArcMassTestReplicatedArrayWrapperNetSerializer::SerializeDelta(FNetSerializationContext& Context, const FNetSerializeDeltaArgs& Args)
	{
		const ConfigType& Cfg = *static_cast<const ConfigType*>(Args.NetSerializerConfig);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Log, TEXT("SerializeDelta: enter Source=0x%llx Prev=0x%llx"),
			(uint64)Args.Source, (uint64)Args.Prev);
		FNetSerializeDeltaArgs Forwarded = Args;
		Forwarded.NetSerializerConfig = &Cfg.InnerConfig;
		InnerSerializer().SerializeDelta(Context, Forwarded);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Log, TEXT("SerializeDelta: exit HasError=%d"), Context.HasErrorOrOverflow() ? 1 : 0);
	}

	void FArcMassTestReplicatedArrayWrapperNetSerializer::DeserializeDelta(FNetSerializationContext& Context, const FNetDeserializeDeltaArgs& Args)
	{
		const ConfigType& Cfg = *static_cast<const ConfigType*>(Args.NetSerializerConfig);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Log, TEXT("DeserializeDelta: enter Target=0x%llx Prev=0x%llx"),
			(uint64)Args.Target, (uint64)Args.Prev);
		FNetDeserializeDeltaArgs Forwarded = Args;
		Forwarded.NetSerializerConfig = &Cfg.InnerConfig;
		InnerSerializer().DeserializeDelta(Context, Forwarded);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Log, TEXT("DeserializeDelta: exit HasError=%d"), Context.HasErrorOrOverflow() ? 1 : 0);
	}

	bool FArcMassTestReplicatedArrayWrapperNetSerializer::IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args)
	{
		const ConfigType& Cfg = *static_cast<const ConfigType*>(Args.NetSerializerConfig);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Verbose, TEXT("IsEqual: enter bStateIsQuantized=%d"), Args.bStateIsQuantized ? 1 : 0);
		FNetIsEqualArgs Forwarded = Args;
		Forwarded.NetSerializerConfig = &Cfg.InnerConfig;
		if (!Args.bStateIsQuantized)
		{
			Forwarded.Source0 = OffsetPtr(Args.Source0, Cfg.InnerMemberOffset);
			Forwarded.Source1 = OffsetPtr(Args.Source1, Cfg.InnerMemberOffset);
		}
		bool bResult = InnerSerializer().IsEqual(Context, Forwarded);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Verbose, TEXT("IsEqual: exit Result=%d"), bResult ? 1 : 0);
		return bResult;
	}

	bool FArcMassTestReplicatedArrayWrapperNetSerializer::Validate(FNetSerializationContext& Context, const FNetValidateArgs& Args)
	{
		const ConfigType& Cfg = *static_cast<const ConfigType*>(Args.NetSerializerConfig);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Verbose, TEXT("Validate: enter Source=0x%llx"), (uint64)Args.Source);
		FNetValidateArgs Forwarded = Args;
		Forwarded.NetSerializerConfig = &Cfg.InnerConfig;
		Forwarded.Source = OffsetPtr(Args.Source, Cfg.InnerMemberOffset);
		bool bResult = InnerSerializer().Validate(Context, Forwarded);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Verbose, TEXT("Validate: exit Result=%d"), bResult ? 1 : 0);
		return bResult;
	}

	void FArcMassTestReplicatedArrayWrapperNetSerializer::Apply(FNetSerializationContext& Context, const FNetApplyArgs& Args)
	{
		const ConfigType& Cfg = *static_cast<const ConfigType*>(Args.NetSerializerConfig);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Log, TEXT("Apply: enter Source=0x%llx Target=0x%llx InnerOffset=%u"),
			(uint64)Args.Source, (uint64)Args.Target, Cfg.InnerMemberOffset);
		FNetApplyArgs Forwarded = Args;
		Forwarded.NetSerializerConfig = &Cfg.InnerConfig;
		Forwarded.Source = OffsetPtr(Args.Source, Cfg.InnerMemberOffset);
		Forwarded.Target = OffsetPtr(Args.Target, Cfg.InnerMemberOffset);
		InnerSerializer().Apply(Context, Forwarded);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Log, TEXT("Apply: exit"));
	}

	void FArcMassTestReplicatedArrayWrapperNetSerializer::CloneDynamicState(FNetSerializationContext& Context, const FNetCloneDynamicStateArgs& Args)
	{
		const ConfigType& Cfg = *static_cast<const ConfigType*>(Args.NetSerializerConfig);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Verbose, TEXT("CloneDynamicState: enter Source=0x%llx Target=0x%llx"), (uint64)Args.Source, (uint64)Args.Target);
		FNetCloneDynamicStateArgs Forwarded = Args;
		Forwarded.NetSerializerConfig = &Cfg.InnerConfig;
		InnerSerializer().CloneDynamicState(Context, Forwarded);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Verbose, TEXT("CloneDynamicState: exit"));
	}

	void FArcMassTestReplicatedArrayWrapperNetSerializer::FreeDynamicState(FNetSerializationContext& Context, const FNetFreeDynamicStateArgs& Args)
	{
		const ConfigType& Cfg = *static_cast<const ConfigType*>(Args.NetSerializerConfig);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Verbose, TEXT("FreeDynamicState: enter Source=0x%llx"), (uint64)Args.Source);
		FNetFreeDynamicStateArgs Forwarded = Args;
		Forwarded.NetSerializerConfig = &Cfg.InnerConfig;
		InnerSerializer().FreeDynamicState(Context, Forwarded);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Verbose, TEXT("FreeDynamicState: exit"));
	}

	void FArcMassTestReplicatedArrayWrapperNetSerializer::CollectNetReferences(FNetSerializationContext& Context, const FNetCollectReferencesArgs& Args)
	{
		const ConfigType& Cfg = *static_cast<const ConfigType*>(Args.NetSerializerConfig);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Verbose, TEXT("CollectNetReferences: enter Source=0x%llx"), (uint64)Args.Source);
		FNetCollectReferencesArgs Forwarded = Args;
		Forwarded.NetSerializerConfig = &Cfg.InnerConfig;
		InnerSerializer().CollectNetReferences(Context, Forwarded);
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Verbose, TEXT("CollectNetReferences: exit"));
	}

	// --- Property info ---

	FArcMassTestReplicatedArrayWrapperPropertyNetSerializerInfo::FArcMassTestReplicatedArrayWrapperPropertyNetSerializerInfo(
		const FName InStructName, const FNetSerializer& InSerializer)
		: FNamedStructPropertyNetSerializerInfo(InStructName, InSerializer)
	{
	}

	bool FArcMassTestReplicatedArrayWrapperPropertyNetSerializerInfo::IsSupported(const FProperty* Property) const
	{
		const FStructProperty* StructProp = CastField<FStructProperty>(Property);
		if (StructProp == nullptr || StructProp->Struct == nullptr)
		{
			return false;
		}
		return IsSupportedStruct(StructProp->Struct->GetFName());
	}

	FNetSerializerConfig* FArcMassTestReplicatedArrayWrapperPropertyNetSerializerInfo::BuildNetSerializerConfig(
		void* NetSerializerConfigBuffer, const FProperty* Property) const
	{
		FArcMassTestReplicatedArrayWrapperNetSerializerConfig* Config =
			new (NetSerializerConfigBuffer) FArcMassTestReplicatedArrayWrapperNetSerializerConfig();

		const UScriptStruct* WrapperStruct = nullptr;
		if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			WrapperStruct = StructProp->Struct;
		}
		if (WrapperStruct == nullptr)
		{
			WrapperStruct = FArcMassTestReplicatedItemArrayWrapper::StaticStruct();
		}

		const FStructProperty* InnerStructProp = nullptr;
		for (TFieldIterator<FStructProperty> It(WrapperStruct); It; ++It)
		{
			const UScriptStruct* MemberStruct = It->Struct;
			while (MemberStruct != nullptr)
			{
				if (MemberStruct->GetFName() == FName(TEXT("ArcIrisReplicatedArray")))
				{
					InnerStructProp = *It;
					break;
				}
				MemberStruct = Cast<UScriptStruct>(MemberStruct->GetSuperStruct());
			}
			if (InnerStructProp != nullptr)
			{
				break;
			}
		}

		if (InnerStructProp == nullptr)
		{
			UE_LOG(LogArcMassTestReplicatedArrayWrapper, Warning, TEXT("BuildNetSerializerConfig: no ArcIrisReplicatedArray-derived member found in %s"),
				WrapperStruct ? *WrapperStruct->GetName() : TEXT("null"));
			return Config;
		}

		Config->InnerMemberOffset = static_cast<uint32>(InnerStructProp->GetOffset_ForGC());

		const UScriptStruct* InnerStruct = InnerStructProp->Struct;
		for (TFieldIterator<FArrayProperty> It(InnerStruct); It; ++It)
		{
			const FArrayProperty* ArrayProp = *It;
			const FStructProperty* ItemProp = CastField<FStructProperty>(ArrayProp->Inner);
			if (ItemProp == nullptr)
			{
				continue;
			}
			const UScriptStruct* ItemStruct = ItemProp->Struct;
			if (!ItemStruct->IsChildOf(FFastArraySerializerItem::StaticStruct()))
			{
				continue;
			}

			Config->InnerConfig.ItemDescriptor = FReplicationStateDescriptorBuilder::CreateDescriptorForStruct(ItemStruct);
			Config->InnerConfig.ItemsArrayOffset = static_cast<uint32>(ArrayProp->GetOffset_ForGC());
			Config->InnerConfig.ItemStride = static_cast<uint32>(ItemStruct->GetStructureSize());
			Config->InnerConfig.ItemStruct = ItemStruct;

			UE_LOG(LogArcMassTestReplicatedArrayWrapper, Log,
				TEXT("BuildNetSerializerConfig: Wrapper=%s InnerOffset=%u InnerStruct=%s ItemStruct=%s ItemsArrayOffset=%u ItemStride=%u"),
				*WrapperStruct->GetName(),
				Config->InnerMemberOffset,
				*InnerStruct->GetName(),
				*ItemStruct->GetName(),
				Config->InnerConfig.ItemsArrayOffset,
				Config->InnerConfig.ItemStride);
			break;
		}

		return Config;
	}

	// --- Registration ---

	UE_NET_IMPLEMENT_SERIALIZER(FArcMassTestReplicatedArrayWrapperNetSerializer);
	FArcMassTestReplicatedArrayWrapperNetSerializer::ConfigType FArcMassTestReplicatedArrayWrapperNetSerializer::DefaultConfig = FArcMassTestReplicatedArrayWrapperNetSerializer::ConfigType();

	FArcMassTestReplicatedArrayWrapperNetSerializer::FNetSerializerRegistryDelegates FArcMassTestReplicatedArrayWrapperNetSerializer::NetSerializerRegistryDelegates;

	static const FName PropertyNetSerializerRegistry_NAME_ArcMassTestReplicatedItemArrayWrapper(TEXT("ArcMassTestReplicatedItemArrayWrapper"));

	static FArcMassTestReplicatedArrayWrapperPropertyNetSerializerInfo ArcMassTestWrapperSerializerInfo(
		PropertyNetSerializerRegistry_NAME_ArcMassTestReplicatedItemArrayWrapper,
		UE_NET_GET_SERIALIZER(FArcMassTestReplicatedArrayWrapperNetSerializer));

	FArcMassTestReplicatedArrayWrapperNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
	{
		FPropertyNetSerializerInfoRegistry::Unregister(&ArcMassTestWrapperSerializerInfo);
	}

	void FArcMassTestReplicatedArrayWrapperNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
	{
		UE_LOG(LogArcMassTestReplicatedArrayWrapper, Log, TEXT("OnPreFreezeNetSerializerRegistry: registering FArcMassTestReplicatedArrayWrapperNetSerializer"));
		FPropertyNetSerializerInfoRegistry::Register(&ArcMassTestWrapperSerializerInfo);
	}

	void FArcMassTestReplicatedArrayWrapperNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
	{
	}
} // namespace ArcMassTestReplication
