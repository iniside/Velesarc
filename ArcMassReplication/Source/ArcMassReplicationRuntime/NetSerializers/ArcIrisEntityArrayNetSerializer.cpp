// Copyright Lukasz Baran. All Rights Reserved.

#include "NetSerializers/ArcIrisEntityArrayNetSerializer.h"
#include "NetSerializers/ArcMassEntityNetDataStore.h"
#include "Replication/ArcMassEntityReplicationProxy.h"
#include "Subsystem/ArcMassEntityReplicationProxySubsystem.h"
#include "Fragments/ArcMassNetId.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/ReplicationSystem/ReplicationOperations.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamUtil.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/Serialization/NetSerializers.h"
#include "NetSerializers/ArcIrisReplicatedArrayNetSerializer.h"
#include "Containers/ScriptArray.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcIrisEntityArray, Verbose, All);

namespace ArcMassReplication
{
	using namespace UE::Net;

	namespace EntityArrayPrivate
	{
		UArcMassEntityReplicationProxySubsystem* GetSubsystem(FNetSerializationContext& Context)
		{
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("GetSubsystem: enter"));

			FNetTokenStore* TokenStore = Context.GetNetTokenStore();
			if (TokenStore == nullptr)
			{
				UE_LOG(LogArcIrisEntityArray, Warning, TEXT("GetSubsystem: TokenStore is null, returning nullptr"));
				return nullptr;
			}
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("GetSubsystem: TokenStore valid"));

			FArcMassEntityNetDataStore* DataStore = TokenStore->GetDataStore<FArcMassEntityNetDataStore>();
			if (DataStore == nullptr)
			{
				UE_LOG(LogArcIrisEntityArray, Warning, TEXT("GetSubsystem: DataStore is null, returning nullptr"));
				return nullptr;
			}
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("GetSubsystem: DataStore valid"));

			UArcMassEntityReplicationProxySubsystem* Subsystem = DataStore->GetSubsystem();
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("GetSubsystem: Subsystem=%s"), Subsystem ? TEXT("valid") : TEXT("null"));
			return Subsystem;
		}

		FArcIrisEntityArrayNetSerializer::FDynamicState* EnsureState(FArcIrisEntityArrayNetSerializer::QuantizedType& Target)
		{
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("EnsureState: State=%s"), Target.State ? TEXT("exists") : TEXT("null, allocating"));
			if (Target.State == nullptr)
			{
				Target.State = new FArcIrisEntityArrayNetSerializer::FDynamicState();
				UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("EnsureState: allocated new FDynamicState"));
			}
			return Target.State;
		}
	} // namespace EntityArrayPrivate

	// --- Slot helpers ---

	void FArcIrisEntityArrayNetSerializer::FreeFragmentSlot(
		FNetSerializationContext& Context,
		FQuantizedFragmentSlot& Slot)
	{
		if (!Slot.IsValid())
		{
			return;
		}
		if (EnumHasAnyFlags(Slot.Descriptor->Traits, EReplicationStateTraits::HasDynamicState))
		{
			const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
			FStructNetSerializerConfig StructConfig;
			StructConfig.StateDescriptor = Slot.Descriptor;
			FNetFreeDynamicStateArgs FreeArgs;
			FreeArgs.Version = 0;
			FreeArgs.NetSerializerConfig = &StructConfig;
			FreeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(Slot.Buffer.GetData());
			StructSerializer->FreeDynamicState(Context, FreeArgs);
		}
		Slot.Buffer = FNetSerializerAlignedStorage{};
		Slot.Descriptor = nullptr;
		Slot.FragmentType = nullptr;
	}

	void FArcIrisEntityArrayNetSerializer::CloneFragmentSlot(
		FNetSerializationContext& Context,
		const FQuantizedFragmentSlot& Src,
		FQuantizedFragmentSlot& Dst)
	{
		FreeFragmentSlot(Context, Dst);
		if (!Src.IsValid())
		{
			return;
		}
		Dst.Descriptor = Src.Descriptor;
		Dst.FragmentType = Src.FragmentType;

		const uint32 Alignment = FMath::Max(static_cast<uint32>(Src.Descriptor->InternalAlignment), 16U);
		Dst.Buffer.AdjustSize(Context, Src.Descriptor->InternalSize, Alignment);
		FMemory::Memzero(Dst.Buffer.GetData(), Src.Descriptor->InternalSize);

		if (EnumHasAnyFlags(Src.Descriptor->Traits, EReplicationStateTraits::HasDynamicState))
		{
			const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
			FStructNetSerializerConfig StructConfig;
			StructConfig.StateDescriptor = Src.Descriptor;
			FNetCloneDynamicStateArgs CloneArgs;
			CloneArgs.Version = 0;
			CloneArgs.NetSerializerConfig = &StructConfig;
			CloneArgs.Source = reinterpret_cast<NetSerializerValuePointer>(Src.Buffer.GetData());
			CloneArgs.Target = reinterpret_cast<NetSerializerValuePointer>(Dst.Buffer.GetData());
			StructSerializer->CloneDynamicState(Context, CloneArgs);
		}
		else
		{
			FMemory::Memcpy(Dst.Buffer.GetData(), Src.Buffer.GetData(), Src.Descriptor->InternalSize);
		}
	}

	void FArcIrisEntityArrayNetSerializer::QuantizeFragmentSlot(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		const UScriptStruct* FragType,
		const uint8* SrcMemory,
		FQuantizedFragmentSlot& OutSlot)
	{
		const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
		FStructNetSerializerConfig StructConfig;
		StructConfig.StateDescriptor = Descriptor;

		const uint32 InternalSize = Descriptor->InternalSize;
		const uint32 Alignment = FMath::Max(static_cast<uint32>(Descriptor->InternalAlignment), 16U);

		if (OutSlot.Buffer.Num() == 0)
		{
			OutSlot.Buffer.AdjustSize(Context, InternalSize, Alignment);
			FMemory::Memzero(OutSlot.Buffer.GetData(), InternalSize);
		}
		OutSlot.Descriptor = Descriptor;
		OutSlot.FragmentType = FragType;

		FNetQuantizeArgs QuantizeArgs;
		QuantizeArgs.Version = 0;
		QuantizeArgs.NetSerializerConfig = &StructConfig;
		QuantizeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(SrcMemory);
		QuantizeArgs.Target = reinterpret_cast<NetSerializerValuePointer>(OutSlot.Buffer.GetData());
		StructSerializer->Quantize(Context, QuantizeArgs);
	}

	void FArcIrisEntityArrayNetSerializer::SerializeFragmentFromQuantized(
		FNetSerializationContext& Context,
		const FQuantizedFragmentSlot& Slot)
	{
		if (!Slot.IsValid())
		{
			return;
		}
		const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
		FStructNetSerializerConfig StructConfig;
		StructConfig.StateDescriptor = Slot.Descriptor;
		FNetSerializeArgs SerializeArgs;
		SerializeArgs.Version = 0;
		SerializeArgs.NetSerializerConfig = &StructConfig;
		SerializeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(Slot.Buffer.GetData());
		StructSerializer->Serialize(Context, SerializeArgs);
	}

	void FArcIrisEntityArrayNetSerializer::SerializeFragmentDelta(
		FNetSerializationContext& Context,
		const FQuantizedFragmentSlot& CurrSlot,
		const FQuantizedFragmentSlot& PrevSlot)
	{
		if (!CurrSlot.IsValid())
		{
			return;
		}
		if (!PrevSlot.IsValid())
		{
			SerializeFragmentFromQuantized(Context, CurrSlot);
			return;
		}
		const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
		FStructNetSerializerConfig StructConfig;
		StructConfig.StateDescriptor = CurrSlot.Descriptor;
		FNetSerializeDeltaArgs DeltaArgs;
		DeltaArgs.Version = 0;
		DeltaArgs.NetSerializerConfig = &StructConfig;
		DeltaArgs.Source = reinterpret_cast<NetSerializerValuePointer>(CurrSlot.Buffer.GetData());
		DeltaArgs.Prev = reinterpret_cast<NetSerializerValuePointer>(PrevSlot.Buffer.GetData());
		StructSerializer->SerializeDelta(Context, DeltaArgs);
	}

	void FArcIrisEntityArrayNetSerializer::DeserializeFragmentToQuantized(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		const UScriptStruct* FragType,
		FQuantizedFragmentSlot& Slot)
	{
		if (Descriptor == nullptr)
		{
			return;
		}
		const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
		FStructNetSerializerConfig StructConfig;
		StructConfig.StateDescriptor = Descriptor;

		const uint32 InternalSize = Descriptor->InternalSize;
		const uint32 Alignment = FMath::Max(static_cast<uint32>(Descriptor->InternalAlignment), 16U);

		if (Slot.Buffer.Num() == 0)
		{
			Slot.Buffer.AdjustSize(Context, InternalSize, Alignment);
			FMemory::Memzero(Slot.Buffer.GetData(), InternalSize);
		}
		Slot.Descriptor = Descriptor;
		Slot.FragmentType = FragType;

		FNetDeserializeArgs DeserializeArgs;
		DeserializeArgs.Version = 0;
		DeserializeArgs.NetSerializerConfig = &StructConfig;
		DeserializeArgs.Target = reinterpret_cast<NetSerializerValuePointer>(Slot.Buffer.GetData());
		StructSerializer->Deserialize(Context, DeserializeArgs);
	}

	void FArcIrisEntityArrayNetSerializer::DequantizeFragmentSlot(
		FNetSerializationContext& Context,
		const FQuantizedFragmentSlot& Slot,
		FInstancedStruct& OutDst)
	{
		if (!Slot.IsValid())
		{
			return;
		}
		if (!OutDst.IsValid() || OutDst.GetScriptStruct() != Slot.FragmentType)
		{
			OutDst.InitializeAs(Slot.FragmentType);
		}
		const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
		FStructNetSerializerConfig StructConfig;
		StructConfig.StateDescriptor = Slot.Descriptor;
		FNetDequantizeArgs DequantizeArgs;
		DequantizeArgs.Version = 0;
		DequantizeArgs.NetSerializerConfig = &StructConfig;
		DequantizeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(Slot.Buffer.GetData());
		DequantizeArgs.Target = reinterpret_cast<NetSerializerValuePointer>(OutDst.GetMutableMemory());
		StructSerializer->Dequantize(Context, DequantizeArgs);
	}

	void FArcIrisEntityArrayNetSerializer::CollectFragmentReferences(
		FNetSerializationContext& Context,
		const FQuantizedFragmentSlot& Slot,
		const FNetCollectReferencesArgs& Args)
	{
		if (!Slot.IsValid() || !EnumHasAnyFlags(Slot.Descriptor->Traits, EReplicationStateTraits::HasObjectReference))
		{
			return;
		}
		const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
		FStructNetSerializerConfig StructConfig;
		StructConfig.StateDescriptor = Slot.Descriptor;
		FNetCollectReferencesArgs CollectArgs = Args;
		CollectArgs.Version = 0;
		CollectArgs.NetSerializerConfig = &StructConfig;
		CollectArgs.Source = reinterpret_cast<NetSerializerValuePointer>(Slot.Buffer.GetData());
		StructSerializer->CollectNetReferences(Context, CollectArgs);
	}

	// --- Helpers ---

	const FArcMassReplicationDescriptorSet* FArcIrisEntityArrayNetSerializer::GetDescriptorSetFromContext(FNetSerializationContext& Context, uint32 Hash)
	{
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("GetDescriptorSetFromContext: Hash=%u"), Hash);

		if (Hash == 0)
		{
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("GetDescriptorSetFromContext: Hash==0, returning nullptr"));
			return nullptr;
		}

		UArcMassEntityReplicationProxySubsystem* Subsystem = EntityArrayPrivate::GetSubsystem(Context);
		if (Subsystem == nullptr)
		{
			UE_LOG(LogArcIrisEntityArray, Warning, TEXT("GetDescriptorSetFromContext: Subsystem null for Hash=%u"), Hash);
			return nullptr;
		}

		const FArcMassReplicationDescriptorSet* DescSet = Subsystem->FindDescriptorSetByHash(Hash);
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("GetDescriptorSetFromContext: Hash=%u -> DescSet=%s"), Hash, DescSet ? TEXT("found") : TEXT("not found"));
		return DescSet;
	}

	bool FArcIrisEntityArrayNetSerializer::ShouldFilterEntityForConnection(FNetSerializationContext& Context, uint32 NetIdValue)
	{
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("ShouldFilterEntityForConnection: NetIdValue=%u"), NetIdValue);

		if (NetIdValue == 0)
		{
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("ShouldFilterEntityForConnection: NetIdValue==0, not filtered"));
			return false;
		}

		uint32 ConnectionId = Context.GetLocalConnectionId();
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("ShouldFilterEntityForConnection: NetIdValue=%u ConnectionId=%u"), NetIdValue, ConnectionId);
		if (ConnectionId == 0)
		{
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("ShouldFilterEntityForConnection: ConnectionId==0 (server), not filtered"));
			return false;
		}

		UArcMassEntityReplicationProxySubsystem* Subsystem = EntityArrayPrivate::GetSubsystem(Context);
		if (Subsystem == nullptr)
		{
			UE_LOG(LogArcIrisEntityArray, Warning, TEXT("ShouldFilterEntityForConnection: Subsystem null, not filtered"));
			return false;
		}

		FArcMassNetId NetId(NetIdValue);
		bool bFiltered = !Subsystem->IsEntityRelevantToConnection(NetId, ConnectionId);
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("ShouldFilterEntityForConnection: NetIdValue=%u ConnectionId=%u -> bFiltered=%d"), NetIdValue, ConnectionId, (int32)bFiltered);
		return bFiltered;
	}

	uint32 FArcIrisEntityArrayNetSerializer::FilterFragmentMaskForConnection(
		FNetSerializationContext& Context, uint32 NetIdValue, uint32 FragmentMask,
		const FArcMassReplicationDescriptorSet* DescSet)
	{
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("FilterFragmentMaskForConnection: NetIdValue=%u FragmentMask=0x%X DescSet=%s"), NetIdValue, FragmentMask, DescSet ? TEXT("valid") : TEXT("null"));

		if (DescSet == nullptr || FragmentMask == 0)
		{
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("FilterFragmentMaskForConnection: early return, DescSet null or mask 0, returning mask=0x%X"), FragmentMask);
			return FragmentMask;
		}

		uint32 ConnectionId = Context.GetLocalConnectionId();
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("FilterFragmentMaskForConnection: ConnectionId=%u"), ConnectionId);
		if (ConnectionId == 0)
		{
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("FilterFragmentMaskForConnection: ConnectionId==0 (server), no filter"));
			return FragmentMask;
		}

		UArcMassEntityReplicationProxySubsystem* Subsystem = EntityArrayPrivate::GetSubsystem(Context);
		if (Subsystem == nullptr)
		{
			UE_LOG(LogArcIrisEntityArray, Warning, TEXT("FilterFragmentMaskForConnection: Subsystem null, returning unfiltered mask"));
			return FragmentMask;
		}

		FArcMassNetId NetId(NetIdValue);
		bool bIsOwner = Subsystem->IsConnectionOwnerOf(NetId, ConnectionId);
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("FilterFragmentMaskForConnection: NetIdValue=%u bIsOwner=%d"), NetIdValue, (int32)bIsOwner);
		if (bIsOwner)
		{
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("FilterFragmentMaskForConnection: owner, returning full mask=0x%X"), FragmentMask);
			return FragmentMask;
		}

		uint32 FilteredMask = FragmentMask;
		for (int32 SlotIdx = 0; SlotIdx < DescSet->FragmentFilters.Num(); ++SlotIdx)
		{
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("FilterFragmentMaskForConnection: SlotIdx=%d Filter=%d"), SlotIdx, (int32)DescSet->FragmentFilters[SlotIdx]);
			if (DescSet->FragmentFilters[SlotIdx] == EArcMassReplicationFilter::OwnerOnly)
			{
				FilteredMask &= ~(1U << SlotIdx);
				UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("FilterFragmentMaskForConnection: removed slot %d (OwnerOnly), FilteredMask=0x%X"), SlotIdx, FilteredMask);
			}
		}

		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("FilterFragmentMaskForConnection: result FilteredMask=0x%X"), FilteredMask);
		return FilteredMask;
	}

	// --- Quantize: update baseline with dirty entities ---

	void FArcIrisEntityArrayNetSerializer::Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args)
	{
		const SourceType& Source = *reinterpret_cast<const SourceType*>(Args.Source);
		QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
		FDynamicState* State = EntityArrayPrivate::EnsureState(Target);

		UE_LOG(LogArcIrisEntityArray, Log, TEXT("Quantize: enter Entities=%d DirtyEntities=%d/%d PendingRemovals=%d HasDirty=%d"),
			Source.Entities.Num(), Source.DirtyEntities.CountSetBits(), Source.DirtyEntities.Num(),
			Source.PendingRemovals.Num(), Source.HasDirtyEntities() ? 1 : 0);
		for (int32 EI = 0; EI < Source.Entities.Num(); ++EI)
		{
			UE_LOG(LogArcIrisEntityArray, Log, TEXT("Quantize: enter Entity[%d] NetId=%u DirtyMask=0x%X RepKey=%u DirtyBit=%d"),
				EI, Source.Entities[EI].NetId.GetValue(),
				Source.Entities[EI].FragmentDirtyMask, Source.Entities[EI].ReplicationKey,
				Source.DirtyEntities.IsValidIndex(EI) ? (Source.DirtyEntities[EI] ? 1 : 0) : -1);
		}

		const FArcMassReplicationDescriptorSet* DescSet = nullptr;
		if (Source.OwnerProxy)
		{
			DescSet = &Source.OwnerProxy->GetDescriptorSet();
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Quantize: OwnerProxy valid, DescSet Hash=%u"), DescSet ? DescSet->Hash : 0u);
		}
		else
		{
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Quantize: OwnerProxy is null"));
		}

		State->DescriptorSetHash = DescSet ? DescSet->Hash : 0;
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Quantize: DescriptorSetHash=%u"), State->DescriptorSetHash);

		bool bHasChanges = false;

		State->RemovedNetIds.Reset();
		for (const FArcMassNetId& RemovedId : Source.PendingRemovals)
		{
			uint32 NetIdValue = RemovedId.GetValue();
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Quantize: processing removal NetIdValue=%u"), NetIdValue);
			State->Baseline.Remove(NetIdValue);
			State->RemovedNetIds.Add(NetIdValue);
			bHasChanges = true;
		}
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Quantize: removals processed, RemovedNetIds=%d"), State->RemovedNetIds.Num());

		for (int32 EntityIdx = 0; EntityIdx < Source.Entities.Num(); ++EntityIdx)
		{
			if (!Source.DirtyEntities.IsValidIndex(EntityIdx) || !Source.DirtyEntities[EntityIdx])
			{
				UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Quantize: EntityIdx=%d not dirty, skipping"), EntityIdx);
				continue;
			}

			const FArcIrisReplicatedEntity& SrcEntity = Source.Entities[EntityIdx];
			uint32 NetIdValue = SrcEntity.NetId.GetValue();
			UE_LOG(LogArcIrisEntityArray, Log, TEXT("Quantize: processing dirty entity EntityIdx=%d NetIdValue=%u FragmentSlots=%d FragmentDirtyMask=0x%X"),
				EntityIdx, NetIdValue, SrcEntity.FragmentSlots.Num(), SrcEntity.FragmentDirtyMask);

			FQuantizedEntity& QEntity = State->Baseline.FindOrAdd(NetIdValue);
			QEntity.NetIdValue = NetIdValue;
			QEntity.ReplicationKey = SrcEntity.ReplicationKey;
			QEntity.FragmentCount = static_cast<uint8>(SrcEntity.FragmentSlots.Num());
			QEntity.FragmentSlots.SetNum(QEntity.FragmentCount);
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Quantize: QEntity NetIdValue=%u ReplicationKey=%u FragmentCount=%d"),
				QEntity.NetIdValue, QEntity.ReplicationKey, (int32)QEntity.FragmentCount);

			for (int32 SlotIdx = 0; SlotIdx < SrcEntity.FragmentSlots.Num(); ++SlotIdx)
			{
				if ((SrcEntity.FragmentDirtyMask & (1U << SlotIdx)) == 0)
				{
					UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Quantize: SlotIdx=%d not in dirty mask, skipping"), SlotIdx);
					continue;
				}

				const FInstancedStruct& SrcSlot = SrcEntity.FragmentSlots[SlotIdx];
				if (!SrcSlot.IsValid())
				{
					UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Quantize: SlotIdx=%d SrcSlot invalid, skipping"), SlotIdx);
					continue;
				}

				const UScriptStruct* FragType = SrcSlot.GetScriptStruct();
				UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Quantize: SlotIdx=%d FragType=%s"), SlotIdx, FragType ? *FragType->GetName() : TEXT("null"));
				const TRefCountPtr<const FReplicationStateDescriptor>& SlotDesc = DescSet->Descriptors[SlotIdx];
				if (!SlotDesc.IsValid())
				{
					continue;
				}
				QuantizeFragmentSlot(Context, SlotDesc.GetReference(), FragType, SrcSlot.GetMemory(), QEntity.FragmentSlots[SlotIdx]);
				UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Quantize: SlotIdx=%d QuantizeFragmentSlot done"), SlotIdx);

				{
					const FNetSerializer& ArcArraySerializer = UE_NET_GET_SERIALIZER(FArcIrisReplicatedArrayNetSerializer);
					for (uint32 MemberIdx = 0; MemberIdx < SlotDesc->MemberCount; ++MemberIdx)
					{
						if (SlotDesc->MemberSerializerDescriptors[MemberIdx].Serializer != &ArcArraySerializer)
						{
							continue;
						}
						const uint32 ExternalOffset = SlotDesc->MemberDescriptors[MemberIdx].ExternalMemberOffset;
						const FArcIrisReplicatedArrayNetSerializerConfig* ArrConfig =
							static_cast<const FArcIrisReplicatedArrayNetSerializerConfig*>(
								SlotDesc->MemberSerializerDescriptors[MemberIdx].SerializerConfig);
						FArcIrisReplicatedArray* LiveArray = reinterpret_cast<FArcIrisReplicatedArray*>(
							const_cast<uint8*>(SrcSlot.GetMemory()) + ExternalOffset);
						const FScriptArray& RawItems = *reinterpret_cast<const FScriptArray*>(
							reinterpret_cast<const uint8*>(LiveArray) + ArrConfig->ItemsArrayOffset);
						UE_LOG(LogArcIrisEntityArray, Log,
							TEXT("Quantize.ClearDirtyState: SlotIdx=%d MemberIdx=%u LiveArray=%p AddedIDs=%d ChangedIDs=%d PendingRemovals=%d (before clear)"),
							SlotIdx, MemberIdx, LiveArray,
							LiveArray->ReceivedAddedIDs.Num(),
							LiveArray->ReceivedChangedIDs.Num(),
							LiveArray->PendingRemovals.Num());
						LiveArray->ClearDirtyState();
						UE_LOG(LogArcIrisEntityArray, Log,
							TEXT("Quantize.ClearDirtyState: after clear AddedIDs=%d ChangedIDs=%d PendingRemovals=%d"),
							LiveArray->ReceivedAddedIDs.Num(),
							LiveArray->ReceivedChangedIDs.Num(),
							LiveArray->PendingRemovals.Num());
					}
				}
			}

			bHasChanges = true;
		}

		if (bHasChanges)
		{
			++State->ChangeVersion;
			UE_LOG(LogArcIrisEntityArray, Log, TEXT("Quantize: bHasChanges=true, ChangeVersion=%llu BaselineSize=%d"), State->ChangeVersion, State->Baseline.Num());
		}
		else
		{
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Quantize: no changes, ChangeVersion=%llu"), State->ChangeVersion);
		}

		const_cast<SourceType&>(Source).ClearDirtyState();
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Quantize: exit"));
	}

	// --- IsEqual ---

	bool FArcIrisEntityArrayNetSerializer::IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args)
	{
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("IsEqual: enter bStateIsQuantized=%d"), Args.bStateIsQuantized ? 1 : 0);

		if (!Args.bStateIsQuantized)
		{
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("IsEqual: not quantized, returning false"));
			return false;
		}

		const QuantizedType& A = *reinterpret_cast<const QuantizedType*>(Args.Source0);
		const QuantizedType& B = *reinterpret_cast<const QuantizedType*>(Args.Source1);

		uint64 VersionA = A.State ? A.State->ChangeVersion : 0;
		uint64 VersionB = B.State ? B.State->ChangeVersion : 0;
		bool bEqual = (VersionA == VersionB);
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("IsEqual: VersionA=%llu VersionB=%llu -> bEqual=%d"), VersionA, VersionB, bEqual ? 1 : 0);
		return bEqual;
	}

	// --- Dequantize: convert received delta into source representation for Apply ---

	void FArcIrisEntityArrayNetSerializer::Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args)
	{
		const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		SourceType& Target = *reinterpret_cast<SourceType*>(Args.Target);

		UE_LOG(LogArcIrisEntityArray, Log, TEXT("Dequantize: enter Source.State=%s"), Source.State ? TEXT("valid") : TEXT("null"));

		if (Source.State == nullptr)
		{
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Dequantize: State null, early return"));
			return;
		}

		const FDynamicState& State = *Source.State;
		UE_LOG(LogArcIrisEntityArray, Log, TEXT("Dequantize: ReceivedRemovedNetIds=%d ReceivedDirtyNetIds=%d BaselineSize=%d"),
			State.ReceivedRemovedNetIds.Num(), State.ReceivedDirtyNetIds.Num(), State.Baseline.Num());

		Target.PendingRemovals.Reset();
		for (uint32 RemovedNetIdValue : State.ReceivedRemovedNetIds)
		{
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Dequantize: adding PendingRemoval NetIdValue=%u"), RemovedNetIdValue);
			Target.PendingRemovals.Add(FArcMassNetId(RemovedNetIdValue));
		}

		Target.Entities.Reset();
		for (uint32 DirtyNetId : State.ReceivedDirtyNetIds)
		{
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Dequantize: processing dirty NetId=%u"), DirtyNetId);
			const FQuantizedEntity* QEntity = State.Baseline.Find(DirtyNetId);
			if (QEntity == nullptr)
			{
				UE_LOG(LogArcIrisEntityArray, Warning, TEXT("Dequantize: DirtyNetId=%u not in Baseline, skipping"), DirtyNetId);
				continue;
			}

			FArcIrisReplicatedEntity& NewEntity = Target.Entities.AddDefaulted_GetRef();
			NewEntity.NetId = FArcMassNetId(QEntity->NetIdValue);
			NewEntity.ReplicationKey = QEntity->ReplicationKey;
			NewEntity.FragmentDirtyMask = (1U << QEntity->FragmentCount) - 1U;
			NewEntity.FragmentSlots.SetNum(QEntity->FragmentSlots.Num());
			for (int32 SlotIdx = 0; SlotIdx < QEntity->FragmentSlots.Num(); ++SlotIdx)
			{
				DequantizeFragmentSlot(Context, QEntity->FragmentSlots[SlotIdx], NewEntity.FragmentSlots[SlotIdx]);
			}
			UE_LOG(LogArcIrisEntityArray, Log, TEXT("Dequantize: added entity NetIdValue=%u ReplicationKey=%u FragmentCount=%d DirtyMask=0x%X FragmentSlots=%d"),
				QEntity->NetIdValue, QEntity->ReplicationKey, (int32)QEntity->FragmentCount, NewEntity.FragmentDirtyMask, NewEntity.FragmentSlots.Num());
		}

		Target.DirtyEntities.Init(true, Target.Entities.Num());
		UE_LOG(LogArcIrisEntityArray, Log, TEXT("Dequantize: exit Entities=%d PendingRemovals=%d"), Target.Entities.Num(), Target.PendingRemovals.Num());
	}

	// --- Serialize: full state (all baseline entities) ---

	void FArcIrisEntityArrayNetSerializer::Serialize(FNetSerializationContext& Context, const FNetSerializeArgs& Args)
	{
		const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

		UE_LOG(LogArcIrisEntityArray, Log, TEXT("Serialize: enter Source.State=%s"), Source.State ? TEXT("valid") : TEXT("null"));

		if (Source.State == nullptr)
		{
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Serialize: State null, writing zeros (Hash=0, RemovedCount=0, EntityCount=0)"));
			Writer->WriteBits(0U, 32U);
			Writer->WriteBits(0U, 8U);
			Writer->WriteBits(0U, 16U);
			return;
		}

		const FDynamicState& State = *Source.State;
		const FArcMassReplicationDescriptorSet* DescSet = GetDescriptorSetFromContext(Context, State.DescriptorSetHash);

		UE_LOG(LogArcIrisEntityArray, Log, TEXT("Serialize: DescriptorSetHash=%u DescSet=%s BaselineSize=%d"),
			State.DescriptorSetHash, DescSet ? TEXT("valid") : TEXT("null"), State.Baseline.Num());

		Writer->WriteBits(State.DescriptorSetHash, 32U);
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Serialize: wrote DescriptorSetHash=%u"), State.DescriptorSetHash);

		Writer->WriteBits(0U, 8U);
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Serialize: wrote RemovedCount=0 (full serialize, no removals)"));

		TArray<const FQuantizedEntity*> PassingEntities;
		for (const TPair<uint32, FQuantizedEntity>& Pair : State.Baseline)
		{
			bool bFiltered = ShouldFilterEntityForConnection(Context, Pair.Key);
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Serialize: baseline entity NetIdValue=%u filtered=%d"), Pair.Key, bFiltered ? 1 : 0);
			if (!bFiltered)
			{
				PassingEntities.Add(&Pair.Value);
			}
		}

		uint16 EntityCount = static_cast<uint16>(PassingEntities.Num());
		UE_LOG(LogArcIrisEntityArray, Log, TEXT("Serialize: writing EntityCount=%d (filtered from %d)"), (int32)EntityCount, State.Baseline.Num());
		Writer->WriteBits(static_cast<uint32>(EntityCount), 16U);

		for (const FQuantizedEntity* QEntity : PassingEntities)
		{
			uint32 FilteredMask = FilterFragmentMaskForConnection(Context, QEntity->NetIdValue,
				(1U << QEntity->FragmentCount) - 1U, DescSet);

			UE_LOG(LogArcIrisEntityArray, Log, TEXT("Serialize: entity NetIdValue=%u ReplicationKey=%u FragmentCount=%d FilteredMask=0x%X"),
				QEntity->NetIdValue, QEntity->ReplicationKey, (int32)QEntity->FragmentCount, FilteredMask);

			Writer->WriteBits(QEntity->NetIdValue, 32U);
			Writer->WriteBits(QEntity->ReplicationKey, 32U);
			Writer->WriteBits(FilteredMask, 32U);

			for (int32 SlotIdx = 0; SlotIdx < QEntity->FragmentCount; ++SlotIdx)
			{
				if ((FilteredMask & (1U << SlotIdx)) == 0)
				{
					UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Serialize: SlotIdx=%d not in FilteredMask, skipping"), SlotIdx);
					continue;
				}

				bool bDescValid = DescSet && DescSet->Descriptors.IsValidIndex(SlotIdx) && DescSet->Descriptors[SlotIdx].IsValid();
				bool bSlotValid = QEntity->FragmentSlots.IsValidIndex(SlotIdx) && QEntity->FragmentSlots[SlotIdx].IsValid();
				UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Serialize: SlotIdx=%d bDescValid=%d bSlotValid=%d"), SlotIdx, bDescValid ? 1 : 0, bSlotValid ? 1 : 0);

				if (bDescValid && bSlotValid)
				{
					UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Serialize: SlotIdx=%d calling SerializeFragmentFromQuantized"), SlotIdx);
					SerializeFragmentFromQuantized(Context, QEntity->FragmentSlots[SlotIdx]);
					UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Serialize: SlotIdx=%d SerializeFragmentFromQuantized done HasError=%d"), SlotIdx, Context.HasErrorOrOverflow() ? 1 : 0);
				}
			}
		}

		UE_LOG(LogArcIrisEntityArray, Log, TEXT("Serialize: exit"));
	}

	// --- Deserialize (full state) ---

	void FArcIrisEntityArrayNetSerializer::Deserialize(FNetSerializationContext& Context, const FNetDeserializeArgs& Args)
	{
		QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
		FDynamicState* State = EntityArrayPrivate::EnsureState(Target);
		FNetBitStreamReader* Reader = Context.GetBitStreamReader();

		UE_LOG(LogArcIrisEntityArray, Log, TEXT("Deserialize: enter"));

		State->DescriptorSetHash = Reader->ReadBits(32U);
		UE_LOG(LogArcIrisEntityArray, Log, TEXT("Deserialize: read DescriptorSetHash=%u"), State->DescriptorSetHash);

		State->Baseline.Reset();
		State->ReceivedDirtyNetIds.Reset();
		State->ReceivedRemovedNetIds.Reset();

		const FArcMassReplicationDescriptorSet* DescSet = GetDescriptorSetFromContext(Context, State->DescriptorSetHash);
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Deserialize: DescSet=%s"), DescSet ? TEXT("valid") : TEXT("null"));

		uint32 RemovedCount = Reader->ReadBits(8U);
		UE_LOG(LogArcIrisEntityArray, Log, TEXT("Deserialize: RemovedCount=%u"), RemovedCount);
		for (uint32 Idx = 0; Idx < RemovedCount; ++Idx)
		{
			uint32 RemovedNetId = Reader->ReadBits(32U);
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Deserialize: removed NetId=%u"), RemovedNetId);
			State->ReceivedRemovedNetIds.Add(RemovedNetId);
		}

		uint32 EntityCount = Reader->ReadBits(16U);
		UE_LOG(LogArcIrisEntityArray, Log, TEXT("Deserialize: EntityCount=%u"), EntityCount);
		for (uint32 EntityIdx = 0; EntityIdx < EntityCount; ++EntityIdx)
		{
			uint32 NetIdValue = Reader->ReadBits(32U);
			uint32 RepKey = Reader->ReadBits(32U);
			uint32 FragmentMask = Reader->ReadBits(32U);
			UE_LOG(LogArcIrisEntityArray, Log, TEXT("Deserialize: entity[%u] NetIdValue=%u RepKey=%u FragmentMask=0x%X"),
				EntityIdx, NetIdValue, RepKey, FragmentMask);

			FQuantizedEntity& QEntity = State->Baseline.Add(NetIdValue);
			QEntity.NetIdValue = NetIdValue;
			QEntity.ReplicationKey = RepKey;

			int32 MaxSlot = 0;
			uint32 Mask = FragmentMask;
			while (Mask != 0)
			{
				int32 Bit = FMath::CountTrailingZeros(Mask);
				MaxSlot = FMath::Max(MaxSlot, Bit + 1);
				Mask &= ~(1U << Bit);
			}
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Deserialize: entity[%u] MaxSlot=%d"), EntityIdx, MaxSlot);

			QEntity.FragmentCount = static_cast<uint8>(MaxSlot);
			QEntity.FragmentSlots.SetNum(MaxSlot);

			for (int32 SlotIdx = 0; SlotIdx < MaxSlot; ++SlotIdx)
			{
				if ((FragmentMask & (1U << SlotIdx)) == 0)
				{
					UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Deserialize: entity[%u] SlotIdx=%d not in mask, skipping"), EntityIdx, SlotIdx);
					continue;
				}

				bool bDescValid = DescSet && DescSet->Descriptors.IsValidIndex(SlotIdx) && DescSet->Descriptors[SlotIdx].IsValid();
				bool bTypeValid = DescSet && DescSet->FragmentTypes.IsValidIndex(SlotIdx);
				UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Deserialize: entity[%u] SlotIdx=%d bDescValid=%d bTypeValid=%d FragType=%s"),
					EntityIdx, SlotIdx, bDescValid ? 1 : 0, bTypeValid ? 1 : 0,
					(bTypeValid && DescSet->FragmentTypes[SlotIdx]) ? *DescSet->FragmentTypes[SlotIdx]->GetName() : TEXT("null"));

				if (bDescValid && bTypeValid)
				{
					DeserializeFragmentToQuantized(
						Context,
						DescSet->Descriptors[SlotIdx].GetReference(),
						DescSet->FragmentTypes[SlotIdx],
						QEntity.FragmentSlots[SlotIdx]);
					UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Deserialize: entity[%u] SlotIdx=%d DeserializeFragmentToQuantized done HasError=%d"),
						EntityIdx, SlotIdx, Context.HasErrorOrOverflow() ? 1 : 0);

					if (Context.HasErrorOrOverflow())
					{
						UE_LOG(LogArcIrisEntityArray, Warning, TEXT("Deserialize: error/overflow at entity[%u] SlotIdx=%d, aborting"), EntityIdx, SlotIdx);
						return;
					}
				}
				else
				{
					UE_LOG(LogArcIrisEntityArray, Warning, TEXT("Deserialize: entity[%u] SlotIdx=%d missing descriptor or type, skipping fragment read"), EntityIdx, SlotIdx);
				}
			}

			State->ReceivedDirtyNetIds.Add(NetIdValue);
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Deserialize: entity[%u] added to ReceivedDirtyNetIds"), EntityIdx);
		}

		++State->ChangeVersion;
		UE_LOG(LogArcIrisEntityArray, Log, TEXT("Deserialize: exit ChangeVersion=%llu BaselineSize=%d ReceivedDirtyNetIds=%d"),
			State->ChangeVersion, State->Baseline.Num(), State->ReceivedDirtyNetIds.Num());
	}

	// --- SerializeDelta: diff current vs prev baseline ---

	void FArcIrisEntityArrayNetSerializer::SerializeDelta(FNetSerializationContext& Context, const FNetSerializeDeltaArgs& Args)
	{
		const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		const QuantizedType& Prev = *reinterpret_cast<const QuantizedType*>(Args.Prev);
		FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

		const FDynamicState* CurrentState = Source.State;
		const FDynamicState* PrevState = Prev.State;

		UE_LOG(LogArcIrisEntityArray, Log, TEXT("SerializeDelta: enter CurrentState=%s PrevState=%s"),
			CurrentState ? TEXT("valid") : TEXT("null"),
			PrevState ? TEXT("valid") : TEXT("null"));

		uint32 DescHash = CurrentState ? CurrentState->DescriptorSetHash : 0;
		const FArcMassReplicationDescriptorSet* DescSet = GetDescriptorSetFromContext(Context, DescHash);
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("SerializeDelta: DescHash=%u DescSet=%s"), DescHash, DescSet ? TEXT("valid") : TEXT("null"));

		Writer->WriteBits(DescHash, 32U);
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("SerializeDelta: wrote DescHash=%u"), DescHash);

		uint8 RemovedCount = CurrentState ? static_cast<uint8>(FMath::Min(CurrentState->RemovedNetIds.Num(), 255)) : 0;
		Writer->WriteBits(static_cast<uint32>(RemovedCount), 8U);
		UE_LOG(LogArcIrisEntityArray, Log, TEXT("SerializeDelta: RemovedCount=%d"), (int32)RemovedCount);
		if (CurrentState)
		{
			for (int32 Idx = 0; Idx < RemovedCount; ++Idx)
			{
				UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("SerializeDelta: writing removed NetId=%u"), CurrentState->RemovedNetIds[Idx]);
				Writer->WriteBits(CurrentState->RemovedNetIds[Idx], 32U);
			}
		}

		TArray<const FQuantizedEntity*> DirtyEntities;
		if (CurrentState)
		{
			for (const TPair<uint32, FQuantizedEntity>& CurPair : CurrentState->Baseline)
			{
				if (ShouldFilterEntityForConnection(Context, CurPair.Key))
				{
					UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("SerializeDelta: entity NetIdValue=%u filtered for connection, skipping"), CurPair.Key);
					continue;
				}

				bool bIsDirty = false;
				if (!PrevState || !PrevState->Baseline.Contains(CurPair.Key))
				{
					UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("SerializeDelta: entity NetIdValue=%u is new (not in prev baseline), dirty=true"), CurPair.Key);
					bIsDirty = true;
				}
				else
				{
					const FQuantizedEntity& PrevEntity = PrevState->Baseline.FindChecked(CurPair.Key);
					bIsDirty = (PrevEntity.ReplicationKey != CurPair.Value.ReplicationKey);
					UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("SerializeDelta: entity NetIdValue=%u PrevRepKey=%u CurRepKey=%u dirty=%d"),
						CurPair.Key, PrevEntity.ReplicationKey, CurPair.Value.ReplicationKey, bIsDirty ? 1 : 0);
				}

				if (bIsDirty)
				{
					DirtyEntities.Add(&CurPair.Value);
				}
			}
		}

		uint16 DirtyCount = static_cast<uint16>(DirtyEntities.Num());
		UE_LOG(LogArcIrisEntityArray, Log, TEXT("SerializeDelta: DirtyCount=%d"), (int32)DirtyCount);
		Writer->WriteBits(static_cast<uint32>(DirtyCount), 16U);

		for (const FQuantizedEntity* QEntity : DirtyEntities)
		{
			uint32 FilteredMask = FilterFragmentMaskForConnection(Context, QEntity->NetIdValue,
				(1U << QEntity->FragmentCount) - 1U, DescSet);

			UE_LOG(LogArcIrisEntityArray, Log, TEXT("SerializeDelta: dirty entity NetIdValue=%u ReplicationKey=%u FragmentCount=%d FilteredMask=0x%X"),
				QEntity->NetIdValue, QEntity->ReplicationKey, (int32)QEntity->FragmentCount, FilteredMask);

			Writer->WriteBits(QEntity->NetIdValue, 32U);
			Writer->WriteBits(QEntity->ReplicationKey, 32U);
			Writer->WriteBits(FilteredMask, 32U);

			const FQuantizedEntity* PrevQEntity = (PrevState && PrevState->Baseline.Contains(QEntity->NetIdValue))
				? PrevState->Baseline.Find(QEntity->NetIdValue)
				: nullptr;

			for (int32 SlotIdx = 0; SlotIdx < QEntity->FragmentCount; ++SlotIdx)
			{
				if ((FilteredMask & (1U << SlotIdx)) == 0)
				{
					UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("SerializeDelta: SlotIdx=%d not in FilteredMask, skipping"), SlotIdx);
					continue;
				}

				bool bDescValid = DescSet && DescSet->Descriptors.IsValidIndex(SlotIdx) && DescSet->Descriptors[SlotIdx].IsValid();
				bool bSlotValid = QEntity->FragmentSlots.IsValidIndex(SlotIdx) && QEntity->FragmentSlots[SlotIdx].IsValid();
				UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("SerializeDelta: SlotIdx=%d bDescValid=%d bSlotValid=%d"), SlotIdx, bDescValid ? 1 : 0, bSlotValid ? 1 : 0);

				if (bDescValid && bSlotValid)
				{
					const FQuantizedFragmentSlot& CurrSlot = QEntity->FragmentSlots[SlotIdx];
					const bool bPrevSlotValid = PrevQEntity
						&& PrevQEntity->FragmentSlots.IsValidIndex(SlotIdx)
						&& PrevQEntity->FragmentSlots[SlotIdx].IsValid();
					if (bPrevSlotValid)
					{
						UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("SerializeDelta: SlotIdx=%d calling SerializeFragmentDelta"), SlotIdx);
						SerializeFragmentDelta(Context, CurrSlot, PrevQEntity->FragmentSlots[SlotIdx]);
					}
					else
					{
						UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("SerializeDelta: SlotIdx=%d calling SerializeFragmentFromQuantized (no prev)"), SlotIdx);
						SerializeFragmentFromQuantized(Context, CurrSlot);
					}
					UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("SerializeDelta: SlotIdx=%d done HasError=%d"), SlotIdx, Context.HasErrorOrOverflow() ? 1 : 0);
				}
			}
		}

		UE_LOG(LogArcIrisEntityArray, Log, TEXT("SerializeDelta: exit"));
	}

	// --- DeserializeDelta: merge incoming delta into baseline ---

	void FArcIrisEntityArrayNetSerializer::DeserializeDelta(FNetSerializationContext& Context, const FNetDeserializeDeltaArgs& Args)
	{
		QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
		const QuantizedType& Prev = *reinterpret_cast<const QuantizedType*>(Args.Prev);

		FDynamicState* State = EntityArrayPrivate::EnsureState(Target);
		FNetBitStreamReader* Reader = Context.GetBitStreamReader();

		UE_LOG(LogArcIrisEntityArray, Log, TEXT("DeserializeDelta: enter Prev.State=%s"), Prev.State ? TEXT("valid") : TEXT("null"));

		if (Prev.State)
		{
			State->Baseline = Prev.State->Baseline;
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("DeserializeDelta: copied prev baseline, BaselineSize=%d"), State->Baseline.Num());

			for (TPair<uint32, FQuantizedEntity>& Pair : State->Baseline)
			{
				for (FQuantizedFragmentSlot& Slot : Pair.Value.FragmentSlots)
				{
					if (Slot.IsValid() && EnumHasAnyFlags(Slot.Descriptor->Traits, EReplicationStateTraits::HasDynamicState))
					{
						FQuantizedFragmentSlot SrcSlot = Slot;
						FreeFragmentSlot(Context, Slot);
						CloneFragmentSlot(Context, SrcSlot, Slot);
					}
				}
			}
		}

		State->DescriptorSetHash = Reader->ReadBits(32U);
		UE_LOG(LogArcIrisEntityArray, Log, TEXT("DeserializeDelta: DescriptorSetHash=%u"), State->DescriptorSetHash);

		State->ReceivedDirtyNetIds.Reset();
		State->ReceivedRemovedNetIds.Reset();

		const FArcMassReplicationDescriptorSet* DescSet = GetDescriptorSetFromContext(Context, State->DescriptorSetHash);
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("DeserializeDelta: DescSet=%s"), DescSet ? TEXT("valid") : TEXT("null"));

		uint32 RemovedCount = Reader->ReadBits(8U);
		UE_LOG(LogArcIrisEntityArray, Log, TEXT("DeserializeDelta: RemovedCount=%u"), RemovedCount);
		for (uint32 Idx = 0; Idx < RemovedCount; ++Idx)
		{
			uint32 RemovedNetId = Reader->ReadBits(32U);
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("DeserializeDelta: removed NetId=%u"), RemovedNetId);
			State->Baseline.Remove(RemovedNetId);
			State->ReceivedRemovedNetIds.Add(RemovedNetId);
		}

		uint32 DirtyCount = Reader->ReadBits(16U);
		UE_LOG(LogArcIrisEntityArray, Log, TEXT("DeserializeDelta: DirtyCount=%u"), DirtyCount);
		for (uint32 EntityIdx = 0; EntityIdx < DirtyCount; ++EntityIdx)
		{
			uint32 NetIdValue = Reader->ReadBits(32U);
			uint32 RepKey = Reader->ReadBits(32U);
			uint32 FragmentMask = Reader->ReadBits(32U);
			UE_LOG(LogArcIrisEntityArray, Log, TEXT("DeserializeDelta: entity[%u] NetIdValue=%u RepKey=%u FragmentMask=0x%X"),
				EntityIdx, NetIdValue, RepKey, FragmentMask);

			FQuantizedEntity& QEntity = State->Baseline.FindOrAdd(NetIdValue);
			QEntity.NetIdValue = NetIdValue;
			QEntity.ReplicationKey = RepKey;

			int32 MaxSlot = 0;
			uint32 Mask = FragmentMask;
			while (Mask != 0)
			{
				int32 Bit = FMath::CountTrailingZeros(Mask);
				MaxSlot = FMath::Max(MaxSlot, Bit + 1);
				Mask &= ~(1U << Bit);
			}
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("DeserializeDelta: entity[%u] MaxSlot=%d"), EntityIdx, MaxSlot);

			if (QEntity.FragmentCount < static_cast<uint8>(MaxSlot))
			{
				QEntity.FragmentCount = static_cast<uint8>(MaxSlot);
			}
			QEntity.FragmentSlots.SetNum(FMath::Max(QEntity.FragmentSlots.Num(), MaxSlot));

			for (int32 SlotIdx = 0; SlotIdx < MaxSlot; ++SlotIdx)
			{
				if ((FragmentMask & (1U << SlotIdx)) == 0)
				{
					UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("DeserializeDelta: entity[%u] SlotIdx=%d not in mask, skipping"), EntityIdx, SlotIdx);
					continue;
				}

				bool bDescValid = DescSet && DescSet->Descriptors.IsValidIndex(SlotIdx) && DescSet->Descriptors[SlotIdx].IsValid();
				bool bTypeValid = DescSet && DescSet->FragmentTypes.IsValidIndex(SlotIdx);
				UE_LOG(LogArcIrisEntityArray, Log, TEXT("DeserializeDelta: entity[%u] SlotIdx=%d DescSet=%s bDescValid=%d bTypeValid=%d FragType=%s MemberCount=%d"),
					EntityIdx, SlotIdx,
					DescSet ? TEXT("valid") : TEXT("null"),
					bDescValid ? 1 : 0, bTypeValid ? 1 : 0,
					(bTypeValid && DescSet->FragmentTypes[SlotIdx]) ? *DescSet->FragmentTypes[SlotIdx]->GetName() : TEXT("null"),
					bDescValid ? (int32)DescSet->Descriptors[SlotIdx]->MemberCount : -1);

				if (bDescValid && bTypeValid)
				{
					DeserializeFragmentToQuantized(
						Context,
						DescSet->Descriptors[SlotIdx].GetReference(),
						DescSet->FragmentTypes[SlotIdx],
						QEntity.FragmentSlots[SlotIdx]);
					UE_LOG(LogArcIrisEntityArray, Log, TEXT("DeserializeDelta: entity[%u] SlotIdx=%d DeserializeFragmentToQuantized done HasError=%d"),
						EntityIdx, SlotIdx, Context.HasErrorOrOverflow() ? 1 : 0);

					if (Context.HasErrorOrOverflow())
					{
						UE_LOG(LogArcIrisEntityArray, Warning, TEXT("DeserializeDelta: error/overflow at entity[%u] SlotIdx=%d, aborting"), EntityIdx, SlotIdx);
						return;
					}
				}
				else
				{
					UE_LOG(LogArcIrisEntityArray, Warning, TEXT("DeserializeDelta: entity[%u] SlotIdx=%d missing descriptor or type, skipping fragment read"), EntityIdx, SlotIdx);
				}
			}

			State->ReceivedDirtyNetIds.Add(NetIdValue);
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("DeserializeDelta: entity[%u] added to ReceivedDirtyNetIds"), EntityIdx);
		}

		++State->ChangeVersion;
		UE_LOG(LogArcIrisEntityArray, Log, TEXT("DeserializeDelta: exit ChangeVersion=%llu BaselineSize=%d ReceivedDirtyNetIds=%d"),
			State->ChangeVersion, State->Baseline.Num(), State->ReceivedDirtyNetIds.Num());
	}

	// --- Validate ---

	bool FArcIrisEntityArrayNetSerializer::Validate(FNetSerializationContext& Context, const FNetValidateArgs& Args)
	{
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Validate: enter, returning true"));
		return true;
	}

	// --- Apply: fires callbacks on the actual actor property ---

	void FArcIrisEntityArrayNetSerializer::Apply(FNetSerializationContext& Context, const FNetApplyArgs& Args)
	{
		const SourceType& Source = *reinterpret_cast<const SourceType*>(Args.Source);
		SourceType& Target = *reinterpret_cast<SourceType*>(Args.Target);

		UE_LOG(LogArcIrisEntityArray, Log, TEXT("Apply: enter Source.Entities=%d Source.PendingRemovals=%d Target.Entities=%d Target.OwnerProxy=%s"),
			Source.Entities.Num(), Source.PendingRemovals.Num(), Target.Entities.Num(),
			Target.OwnerProxy ? TEXT("valid") : TEXT("null"));

		const FArcMassReplicationDescriptorSet* DescSet = nullptr;
		if (Target.OwnerProxy)
		{
			DescSet = &Target.OwnerProxy->GetDescriptorSet();
			UE_LOG(LogArcIrisEntityArray, Log, TEXT("Apply: DescSet Hash=%u FragmentTypes=%d"), DescSet ? DescSet->Hash : 0u, DescSet ? DescSet->FragmentTypes.Num() : 0);
			if (DescSet)
			{
				for (int32 i = 0; i < DescSet->FragmentTypes.Num(); ++i)
				{
					UE_LOG(LogArcIrisEntityArray, Log, TEXT("Apply: DescSet FragmentTypes[%d]=%s DescriptorValid=%d MemberCount=%d"),
						i,
						DescSet->FragmentTypes[i] ? *DescSet->FragmentTypes[i]->GetName() : TEXT("null"),
						(DescSet->Descriptors.IsValidIndex(i) && DescSet->Descriptors[i].IsValid()) ? 1 : 0,
						(DescSet->Descriptors.IsValidIndex(i) && DescSet->Descriptors[i].IsValid()) ? (int32)DescSet->Descriptors[i]->MemberCount : -1);
				}
			}
		}
		else
		{
			UE_LOG(LogArcIrisEntityArray, Warning, TEXT("Apply: OwnerProxy null, DescSet will be null"));
		}

		for (const FArcMassNetId& RemovedNetId : Source.PendingRemovals)
		{
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Apply: PreReplicatedRemove for NetId=%u"), RemovedNetId.GetValue());
			Target.PreReplicatedRemove(RemovedNetId);
			int32 Idx = Target.FindIndexByNetId(RemovedNetId);
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Apply: FindIndexByNetId=%d for NetId=%u"), Idx, RemovedNetId.GetValue());
			if (Idx != INDEX_NONE)
			{
				Target.Entities.RemoveAtSwap(Idx);
				UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Apply: removed entity at Idx=%d, Target.Entities now=%d"), Idx, Target.Entities.Num());
			}
		}

		for (int32 TargetIdx = Target.Entities.Num() - 1; TargetIdx >= 0; --TargetIdx)
		{
			FArcMassNetId NetId = Target.Entities[TargetIdx].NetId;
			bool bNotInSource = Source.FindIndexByNetId(NetId) == INDEX_NONE;
			bool bNotInRemovals = Source.PendingRemovals.FindByPredicate([NetId](const FArcMassNetId& R) { return R == NetId; }) == nullptr;
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Apply: stale check TargetIdx=%d NetId=%u NotInSource=%d NotInRemovals=%d"),
				TargetIdx, NetId.GetValue(), bNotInSource ? 1 : 0, bNotInRemovals ? 1 : 0);
			if (bNotInSource && bNotInRemovals)
			{
				UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Apply: stale entity at TargetIdx=%d NetId=%u, PreReplicatedRemove+RemoveAtSwap"), TargetIdx, NetId.GetValue());
				Target.PreReplicatedRemove(NetId);
				Target.Entities.RemoveAtSwap(TargetIdx);
			}
		}

		for (const FArcIrisReplicatedEntity& SrcEntity : Source.Entities)
		{
			int32 TargetIdx = Target.FindIndexByNetId(SrcEntity.NetId);
			bool bIsNew = (TargetIdx == INDEX_NONE);

			UE_LOG(LogArcIrisEntityArray, Log, TEXT("Apply: SrcEntity NetId=%u bIsNew=%d FragmentSlots=%d FragmentDirtyMask=0x%X"),
				SrcEntity.NetId.GetValue(), bIsNew ? 1 : 0, SrcEntity.FragmentSlots.Num(), SrcEntity.FragmentDirtyMask);

			if (bIsNew && DescSet)
			{
				UE_LOG(LogArcIrisEntityArray, Log, TEXT("Apply: calling AddEntity NetId=%u with %d FragmentTypes"), SrcEntity.NetId.GetValue(), DescSet->FragmentTypes.Num());
				TargetIdx = Target.AddEntity(SrcEntity.NetId, DescSet->FragmentTypes);
				UE_LOG(LogArcIrisEntityArray, Log, TEXT("Apply: AddEntity done NetId=%u TargetIdx=%d"), SrcEntity.NetId.GetValue(), TargetIdx);
				if (TargetIdx != INDEX_NONE)
				{
					const FArcIrisReplicatedEntity& AddedEntity = Target.Entities[TargetIdx];
					UE_LOG(LogArcIrisEntityArray, Log, TEXT("Apply: post-AddEntity TargetEntity.FragmentSlots=%d"), AddedEntity.FragmentSlots.Num());
					for (int32 i = 0; i < AddedEntity.FragmentSlots.Num(); ++i)
					{
						UE_LOG(LogArcIrisEntityArray, Log, TEXT("Apply: post-AddEntity slot[%d] Valid=%d Type=%s"),
							i,
							AddedEntity.FragmentSlots[i].IsValid() ? 1 : 0,
							(AddedEntity.FragmentSlots[i].IsValid() && AddedEntity.FragmentSlots[i].GetScriptStruct()) ? *AddedEntity.FragmentSlots[i].GetScriptStruct()->GetName() : TEXT("none"));
					}
				}
			}
			else if (bIsNew)
			{
				UE_LOG(LogArcIrisEntityArray, Warning, TEXT("Apply: bIsNew=true but DescSet=null for NetId=%u, cannot AddEntity"), SrcEntity.NetId.GetValue());
			}

			if (TargetIdx == INDEX_NONE)
			{
				UE_LOG(LogArcIrisEntityArray, Warning, TEXT("Apply: TargetIdx==INDEX_NONE for NetId=%u (no DescSet?), skipping"), SrcEntity.NetId.GetValue());
				continue;
			}

			FArcIrisReplicatedEntity& TargetEntity = Target.Entities[TargetIdx];

			if (!bIsNew && TargetEntity.ReplicationKey == SrcEntity.ReplicationKey)
			{
				UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Apply: NetId=%u ReplicationKey unchanged (%u), skipping update"), SrcEntity.NetId.GetValue(), SrcEntity.ReplicationKey);
				continue;
			}

			TargetEntity.ReplicationKey = SrcEntity.ReplicationKey;
			uint32 AppliedMask = 0;

			for (int32 SlotIdx = 0; SlotIdx < SrcEntity.FragmentSlots.Num(); ++SlotIdx)
			{
				if ((SrcEntity.FragmentDirtyMask & (1U << SlotIdx)) == 0)
				{
					UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Apply: NetId=%u SlotIdx=%d not in dirty mask, skipping"), SrcEntity.NetId.GetValue(), SlotIdx);
					continue;
				}

				const FInstancedStruct& SrcSlot = SrcEntity.FragmentSlots[SlotIdx];
				if (!SrcSlot.IsValid())
				{
					UE_LOG(LogArcIrisEntityArray, Warning, TEXT("Apply: NetId=%u SlotIdx=%d SrcSlot INVALID, skipping"), SrcEntity.NetId.GetValue(), SlotIdx);
					continue;
				}

				UE_LOG(LogArcIrisEntityArray, Log, TEXT("Apply: NetId=%u SlotIdx=%d SrcSlot type=%s"),
					SrcEntity.NetId.GetValue(), SlotIdx, SrcSlot.GetScriptStruct() ? *SrcSlot.GetScriptStruct()->GetName() : TEXT("null"));

				UE_LOG(LogArcIrisEntityArray, Log, TEXT("Apply: NetId=%u TargetEntity.FragmentSlots=%d IsValidIndex=%d"),
					SrcEntity.NetId.GetValue(), TargetEntity.FragmentSlots.Num(), TargetEntity.FragmentSlots.IsValidIndex(SlotIdx) ? 1 : 0);

				if (TargetEntity.FragmentSlots.IsValidIndex(SlotIdx))
				{
					FInstancedStruct& DstSlot = TargetEntity.FragmentSlots[SlotIdx];
					bool bDstValid = DstSlot.IsValid();
					bool bTypesMatch = bDstValid && DstSlot.GetScriptStruct() == SrcSlot.GetScriptStruct();
					UE_LOG(LogArcIrisEntityArray, Log, TEXT("Apply: NetId=%u SlotIdx=%d DstValid=%d DstType=%s SrcType=%s TypesMatch=%d"),
						SrcEntity.NetId.GetValue(), SlotIdx,
						bDstValid ? 1 : 0,
						(bDstValid && DstSlot.GetScriptStruct()) ? *DstSlot.GetScriptStruct()->GetName() : TEXT("none"),
						SrcSlot.GetScriptStruct() ? *SrcSlot.GetScriptStruct()->GetName() : TEXT("null"),
						bTypesMatch ? 1 : 0);
					if (bDstValid && bTypesMatch)
					{
						// Proxy is pure transport: bulk-copy the slot. Per-member callback
						// dispatch for replicated-array fields happens later, inside
						// UArcMassEntityReplicationProxySubsystem::OnClientEntityAdded/Changed
						// via ApplyReplicatedFragmentToTarget, against the Mass entity's
						// fragment instance — that's where consumers read state.
						SrcSlot.GetScriptStruct()->CopyScriptStruct(DstSlot.GetMutableMemory(), SrcSlot.GetMemory());
						AppliedMask |= (1U << SlotIdx);
						UE_LOG(LogArcIrisEntityArray, Log, TEXT("Apply: NetId=%u SlotIdx=%d CopyScriptStruct done, AppliedMask=0x%X"), SrcEntity.NetId.GetValue(), SlotIdx, AppliedMask);
					}
				}
				else
				{
					UE_LOG(LogArcIrisEntityArray, Warning, TEXT("Apply: NetId=%u SlotIdx=%d NOT valid index in TargetEntity.FragmentSlots (size=%d)"),
						SrcEntity.NetId.GetValue(), SlotIdx, TargetEntity.FragmentSlots.Num());
				}
			}

			if (bIsNew)
			{
				UE_LOG(LogArcIrisEntityArray, Log, TEXT("Apply: PostReplicatedAdd NetId=%u"), SrcEntity.NetId.GetValue());
				Target.PostReplicatedAdd(SrcEntity.NetId);
			}
			else if (AppliedMask != 0)
			{
				UE_LOG(LogArcIrisEntityArray, Log, TEXT("Apply: PostReplicatedChange NetId=%u AppliedMask=0x%X"), SrcEntity.NetId.GetValue(), AppliedMask);
				Target.PostReplicatedChange(SrcEntity.NetId, AppliedMask);
			}
			else
			{
				UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("Apply: NetId=%u no fragments applied (AppliedMask=0)"), SrcEntity.NetId.GetValue());
			}
		}

		UE_LOG(LogArcIrisEntityArray, Log, TEXT("Apply: exit Target.Entities=%d"), Target.Entities.Num());
	}

	// --- Dynamic state ---

	void FArcIrisEntityArrayNetSerializer::CloneDynamicState(FNetSerializationContext& Context, const FNetCloneDynamicStateArgs& Args)
	{
		const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);

		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("CloneDynamicState: Source.State=%s"), Source.State ? TEXT("valid") : TEXT("null"));

		if (Source.State == nullptr)
		{
			Target.State = nullptr;
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("CloneDynamicState: Source null, Target.State=null"));
			return;
		}

		FDynamicState* NewState = new FDynamicState();
		NewState->DescriptorSetHash = Source.State->DescriptorSetHash;
		NewState->Baseline = Source.State->Baseline;
		NewState->RemovedNetIds = Source.State->RemovedNetIds;
		NewState->ChangeVersion = Source.State->ChangeVersion;
		NewState->ReceivedDirtyNetIds = Source.State->ReceivedDirtyNetIds;
		NewState->ReceivedRemovedNetIds = Source.State->ReceivedRemovedNetIds;
		Target.State = NewState;

		for (TPair<uint32, FQuantizedEntity>& DstPair : NewState->Baseline)
		{
			const FQuantizedEntity* SrcEntity = Source.State->Baseline.Find(DstPair.Key);
			if (!SrcEntity)
			{
				continue;
			}
			for (int32 SlotIdx = 0; SlotIdx < DstPair.Value.FragmentSlots.Num(); ++SlotIdx)
			{
				if (!SrcEntity->FragmentSlots.IsValidIndex(SlotIdx))
				{
					continue;
				}
				CloneFragmentSlot(Context, SrcEntity->FragmentSlots[SlotIdx], DstPair.Value.FragmentSlots[SlotIdx]);
			}
		}

		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("CloneDynamicState: cloned DescHash=%u BaselineSize=%d ChangeVersion=%llu"),
			NewState->DescriptorSetHash, NewState->Baseline.Num(), NewState->ChangeVersion);
	}

	void FArcIrisEntityArrayNetSerializer::FreeDynamicState(FNetSerializationContext& Context, const FNetFreeDynamicStateArgs& Args)
	{
		QuantizedType& Value = *reinterpret_cast<QuantizedType*>(Args.Source);
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("FreeDynamicState: State=%s"), Value.State ? TEXT("valid") : TEXT("null"));
		if (Value.State)
		{
			for (TPair<uint32, FQuantizedEntity>& Pair : Value.State->Baseline)
			{
				for (FQuantizedFragmentSlot& Slot : Pair.Value.FragmentSlots)
				{
					FreeFragmentSlot(Context, Slot);
				}
			}
		}
		delete Value.State;
		Value.State = nullptr;
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("FreeDynamicState: freed"));
	}

	void FArcIrisEntityArrayNetSerializer::CollectNetReferences(FNetSerializationContext& Context, const FNetCollectReferencesArgs& Args)
	{
		const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("CollectNetReferences: enter Source.State=%s"), Source.State ? TEXT("valid") : TEXT("null"));

		if (Source.State == nullptr)
		{
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("CollectNetReferences: State null, early return"));
			return;
		}

		const FArcMassReplicationDescriptorSet* DescSet = GetDescriptorSetFromContext(Context, Source.State->DescriptorSetHash);
		if (DescSet == nullptr)
		{
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("CollectNetReferences: DescSet null for Hash=%u, early return"), Source.State->DescriptorSetHash);
			return;
		}

		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("CollectNetReferences: BaselineSize=%d"), Source.State->Baseline.Num());

		for (const TPair<uint32, FQuantizedEntity>& Pair : Source.State->Baseline)
		{
			const FQuantizedEntity& Entity = Pair.Value;
			const uint32 AllFragmentsMask = Entity.FragmentCount >= 32 ? MAX_uint32 : ((1U << Entity.FragmentCount) - 1U);
			const uint32 FilteredMask = FilterFragmentMaskForConnection(Context, Entity.NetIdValue, AllFragmentsMask, DescSet);
			UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("CollectNetReferences: entity NetIdValue=%u FragmentCount=%d AllMask=0x%X FilteredMask=0x%X"),
				Entity.NetIdValue, (int32)Entity.FragmentCount, AllFragmentsMask, FilteredMask);

			for (int32 SlotIdx = 0; SlotIdx < Entity.FragmentCount; ++SlotIdx)
			{
				if ((FilteredMask & (1U << SlotIdx)) == 0)
				{
					UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("CollectNetReferences: SlotIdx=%d filtered, skipping"), SlotIdx);
					continue;
				}

				bool bDescValid = DescSet->Descriptors.IsValidIndex(SlotIdx) && DescSet->Descriptors[SlotIdx].IsValid();
				bool bSlotValid = Entity.FragmentSlots.IsValidIndex(SlotIdx) && Entity.FragmentSlots[SlotIdx].IsValid();
				UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("CollectNetReferences: SlotIdx=%d bDescValid=%d bSlotValid=%d"), SlotIdx, bDescValid ? 1 : 0, bSlotValid ? 1 : 0);

				if (!bDescValid || !bSlotValid)
				{
					UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("CollectNetReferences: SlotIdx=%d invalid, skipping"), SlotIdx);
					continue;
				}

				UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("CollectNetReferences: SlotIdx=%d calling CollectFragmentReferences"), SlotIdx);
				CollectFragmentReferences(Context, Entity.FragmentSlots[SlotIdx], Args);
			}
		}

		UE_LOG(LogArcIrisEntityArray, Verbose, TEXT("CollectNetReferences: exit"));
	}

	// --- Registration ---

	UE_NET_IMPLEMENT_SERIALIZER(FArcIrisEntityArrayNetSerializer);
	FArcIrisEntityArrayNetSerializer::ConfigType FArcIrisEntityArrayNetSerializer::DefaultConfig = FArcIrisEntityArrayNetSerializer::ConfigType();

	FArcIrisEntityArrayNetSerializer::FNetSerializerRegistryDelegates FArcIrisEntityArrayNetSerializer::NetSerializerRegistryDelegates;
	static const FName PropertyNetSerializerRegistry_NAME_ArcIrisEntityArray(TEXT("ArcIrisEntityArray"));
	UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcIrisEntityArray, FArcIrisEntityArrayNetSerializer);

	FArcIrisEntityArrayNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
	{
		UE_NET_UNREGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcIrisEntityArray);
	}

	void FArcIrisEntityArrayNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
	{
		UE_LOG(LogArcIrisEntityArray, Log, TEXT("OnPreFreezeNetSerializerRegistry: registering FArcIrisEntityArrayNetSerializer"));
		UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcIrisEntityArray);
	}

	void FArcIrisEntityArrayNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
	{
		UE_LOG(LogArcIrisEntityArray, Log, TEXT("OnPostFreezeNetSerializerRegistry: called"));
	}
} // namespace ArcMassReplication
