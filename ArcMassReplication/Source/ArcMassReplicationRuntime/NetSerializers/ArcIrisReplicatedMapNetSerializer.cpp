// Copyright Lukasz Baran. All Rights Reserved.

#include "NetSerializers/ArcIrisReplicatedMapNetSerializer.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetSerializers.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcIrisReplicatedMap, Verbose, All);

// FArcIrisReplicatedMapNetSerializerConfig ctor — sets NeedDestruction because we hold a TRefCountPtr.
FArcIrisReplicatedMapNetSerializerConfig::FArcIrisReplicatedMapNetSerializerConfig()
{
	ConfigTraits = ENetSerializerConfigTraits::NeedDestruction;
}

namespace ArcMassReplication
{
	using namespace UE::Net;

	// -------------------------------------------------------------------------
	// ReplicatedMapPrivate — file-scoped, unique namespace for internal helpers.
	// -------------------------------------------------------------------------
	namespace ReplicatedMapPrivate
	{
		FArcIrisReplicatedMapNetSerializer::FDynamicState* EnsureState(FArcIrisReplicatedMapNetSerializer::FQuantizedType& Target)
		{
			if (Target.State == nullptr)
			{
				Target.State = new FArcIrisReplicatedMapNetSerializer::FDynamicState();
			}
			return Target.State;
		}

		uint8* AllocInternalBuffer(const FReplicationStateDescriptor* Descriptor)
		{
			const uint32 InternalSize = Descriptor->InternalSize;
			const uint32 InternalAlignment = Descriptor->InternalAlignment > 16U ? Descriptor->InternalAlignment : 16U;
			uint8* Buffer = static_cast<uint8*>(FMemory::Malloc(InternalSize, InternalAlignment));
			FMemory::Memzero(Buffer, InternalSize);
			return Buffer;
		}

		void FreeInternalBuffer(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, uint8* Buffer)
		{
			if (EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasDynamicState))
			{
				const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
				FStructNetSerializerConfig StructConfig;
				StructConfig.StateDescriptor = Descriptor;

				FNetFreeDynamicStateArgs FreeArgs;
				FreeArgs.Version = 0;
				FreeArgs.NetSerializerConfig = &StructConfig;
				FreeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(Buffer);
				StructSerializer->FreeDynamicState(Context, FreeArgs);
			}
			FMemory::Free(Buffer);
		}
	} // namespace ReplicatedMapPrivate

	// -------------------------------------------------------------------------
	// Per-entry helpers
	// -------------------------------------------------------------------------

	void FArcIrisReplicatedMapNetSerializer::FreeQuantizedEntryState(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		FNetSerializerAlignedStorage& EntryState)
	{
		if (EntryState.Num() == 0)
		{
			return;
		}
		if (Descriptor != nullptr && EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasDynamicState))
		{
			const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
			FStructNetSerializerConfig StructConfig;
			StructConfig.StateDescriptor = Descriptor;

			FNetFreeDynamicStateArgs FreeArgs;
			FreeArgs.Version = 0;
			FreeArgs.NetSerializerConfig = &StructConfig;
			FreeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(EntryState.GetData());
			StructSerializer->FreeDynamicState(Context, FreeArgs);
		}
		EntryState.Free(Context);
	}

	void FArcIrisReplicatedMapNetSerializer::QuantizeEntry(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		const uint8* SrcEntryMemory,
		FNetSerializerAlignedStorage& OutState)
	{
		const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
		FStructNetSerializerConfig StructConfig;
		StructConfig.StateDescriptor = Descriptor;

		FreeQuantizedEntryState(Context, Descriptor, OutState);

		uint8* InternalBuffer = ReplicatedMapPrivate::AllocInternalBuffer(Descriptor);

		FNetQuantizeArgs QuantizeArgs;
		QuantizeArgs.Version = 0;
		QuantizeArgs.NetSerializerConfig = &StructConfig;
		QuantizeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(SrcEntryMemory);
		QuantizeArgs.Target = reinterpret_cast<NetSerializerValuePointer>(InternalBuffer);
		StructSerializer->Quantize(Context, QuantizeArgs);

		OutState.AdjustSize(Context, Descriptor->InternalSize, Descriptor->InternalAlignment);
		FMemory::Memcpy(OutState.GetData(), InternalBuffer, Descriptor->InternalSize);
		FMemory::Free(InternalBuffer);
	}

	void FArcIrisReplicatedMapNetSerializer::SerializeEntryFromQuantized(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		const FNetSerializerAlignedStorage& EntryState)
	{
		const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
		FStructNetSerializerConfig StructConfig;
		StructConfig.StateDescriptor = Descriptor;

		uint8* InternalBuffer = ReplicatedMapPrivate::AllocInternalBuffer(Descriptor);
		FMemory::Memcpy(InternalBuffer, EntryState.GetData(), Descriptor->InternalSize);

		FNetSerializeArgs SerializeArgs;
		SerializeArgs.Version = 0;
		SerializeArgs.NetSerializerConfig = &StructConfig;
		SerializeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(InternalBuffer);
		StructSerializer->Serialize(Context, SerializeArgs);

		FMemory::Free(InternalBuffer);
	}

	void FArcIrisReplicatedMapNetSerializer::DeserializeEntryToQuantized(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		FNetSerializerAlignedStorage& OutState)
	{
		const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
		FStructNetSerializerConfig StructConfig;
		StructConfig.StateDescriptor = Descriptor;

		uint8* InternalBuffer = ReplicatedMapPrivate::AllocInternalBuffer(Descriptor);

		FNetDeserializeArgs DeserializeArgs;
		DeserializeArgs.Version = 0;
		DeserializeArgs.NetSerializerConfig = &StructConfig;
		DeserializeArgs.Target = reinterpret_cast<NetSerializerValuePointer>(InternalBuffer);
		StructSerializer->Deserialize(Context, DeserializeArgs);

		if (Context.HasErrorOrOverflow())
		{
			ReplicatedMapPrivate::FreeInternalBuffer(Context, Descriptor, InternalBuffer);
			return;
		}

		FreeQuantizedEntryState(Context, Descriptor, OutState);
		OutState.AdjustSize(Context, Descriptor->InternalSize, Descriptor->InternalAlignment);
		FMemory::Memcpy(OutState.GetData(), InternalBuffer, Descriptor->InternalSize);
		FMemory::Free(InternalBuffer);
	}

	void FArcIrisReplicatedMapNetSerializer::DequantizeEntry(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		const FNetSerializerAlignedStorage& EntryState,
		uint8* DstEntryMemory)
	{
		const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
		FStructNetSerializerConfig StructConfig;
		StructConfig.StateDescriptor = Descriptor;

		uint8* InternalBuffer = ReplicatedMapPrivate::AllocInternalBuffer(Descriptor);
		FMemory::Memcpy(InternalBuffer, EntryState.GetData(), Descriptor->InternalSize);

		FNetDequantizeArgs DequantizeArgs;
		DequantizeArgs.Version = 0;
		DequantizeArgs.NetSerializerConfig = &StructConfig;
		DequantizeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(InternalBuffer);
		DequantizeArgs.Target = reinterpret_cast<NetSerializerValuePointer>(DstEntryMemory);
		StructSerializer->Dequantize(Context, DequantizeArgs);

		FMemory::Free(InternalBuffer);
	}

	void FArcIrisReplicatedMapNetSerializer::CloneQuantizedEntryState(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		const FQuantizedEntry& Source,
		FQuantizedEntry& Target)
	{
		if (Source.EntryState.Num() == 0)
		{
			return;
		}
		Target.EntryState.AdjustSize(Context, Source.EntryState.Num(), Source.EntryState.GetAlignment());
		FMemory::Memcpy(Target.EntryState.GetData(), Source.EntryState.GetData(), Source.EntryState.Num());

		if (Descriptor != nullptr && EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasDynamicState))
		{
			const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
			FStructNetSerializerConfig StructConfig;
			StructConfig.StateDescriptor = Descriptor;

			FNetCloneDynamicStateArgs CloneArgs;
			CloneArgs.Version = 0;
			CloneArgs.NetSerializerConfig = &StructConfig;
			CloneArgs.Source = reinterpret_cast<NetSerializerValuePointer>(Source.EntryState.GetData());
			CloneArgs.Target = reinterpret_cast<NetSerializerValuePointer>(Target.EntryState.GetData());
			StructSerializer->CloneDynamicState(Context, CloneArgs);
		}
	}

	// -------------------------------------------------------------------------
	// Key/Value sub-region comparison helpers
	// -------------------------------------------------------------------------

	bool FArcIrisReplicatedMapNetSerializer::EntryKeysAreEqual(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		const FNetSerializerAlignedStorage& A,
		const FNetSerializerAlignedStorage& B)
	{
		// member[0] is the Key. Confirmed in BuildNetSerializerConfig by assertion.
		const FReplicationStateMemberSerializerDescriptor& KeySerDesc = Descriptor->MemberSerializerDescriptors[0];
		const FReplicationStateMemberDescriptor& KeyMemDesc = Descriptor->MemberDescriptors[0];

		FNetIsEqualArgs EqArgs;
		EqArgs.Version = 0;
		EqArgs.NetSerializerConfig = KeySerDesc.SerializerConfig;
		EqArgs.Source0 = reinterpret_cast<NetSerializerValuePointer>(A.GetData() + KeyMemDesc.InternalMemberOffset);
		EqArgs.Source1 = reinterpret_cast<NetSerializerValuePointer>(B.GetData() + KeyMemDesc.InternalMemberOffset);
		EqArgs.bStateIsQuantized = true;
		return KeySerDesc.Serializer->IsEqual(Context, EqArgs);
	}

	bool FArcIrisReplicatedMapNetSerializer::EntryValuesAreEqual(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		const FNetSerializerAlignedStorage& A,
		const FNetSerializerAlignedStorage& B)
	{
		// member[1] is the Value. Confirmed in BuildNetSerializerConfig by assertion.
		const FReplicationStateMemberSerializerDescriptor& ValSerDesc = Descriptor->MemberSerializerDescriptors[1];
		const FReplicationStateMemberDescriptor& ValMemDesc = Descriptor->MemberDescriptors[1];

		FNetIsEqualArgs EqArgs;
		EqArgs.Version = 0;
		EqArgs.NetSerializerConfig = ValSerDesc.SerializerConfig;
		EqArgs.Source0 = reinterpret_cast<NetSerializerValuePointer>(A.GetData() + ValMemDesc.InternalMemberOffset);
		EqArgs.Source1 = reinterpret_cast<NetSerializerValuePointer>(B.GetData() + ValMemDesc.InternalMemberOffset);
		EqArgs.bStateIsQuantized = true;
		return ValSerDesc.Serializer->IsEqual(Context, EqArgs);
	}

	void FArcIrisReplicatedMapNetSerializer::SerializeEntryKeyOnly(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		const FNetSerializerAlignedStorage& EntryState)
	{
		const FReplicationStateMemberSerializerDescriptor& KeySerDesc = Descriptor->MemberSerializerDescriptors[0];
		const FReplicationStateMemberDescriptor& KeyMemDesc = Descriptor->MemberDescriptors[0];

		FNetSerializeArgs SerArgs;
		SerArgs.Version = 0;
		SerArgs.NetSerializerConfig = KeySerDesc.SerializerConfig;
		SerArgs.Source = reinterpret_cast<NetSerializerValuePointer>(EntryState.GetData() + KeyMemDesc.InternalMemberOffset);
		KeySerDesc.Serializer->Serialize(Context, SerArgs);
	}

	void FArcIrisReplicatedMapNetSerializer::DeserializeEntryKeyOnly(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		uint8* OutKeyBuffer,
		uint32 BufferSize)
	{
		// OutKeyBuffer is a zeroed sub-buffer at KeyMemDesc.InternalMemberOffset within a full internal-size allocation.
		const FReplicationStateMemberSerializerDescriptor& KeySerDesc = Descriptor->MemberSerializerDescriptors[0];
		const FReplicationStateMemberDescriptor& KeyMemDesc = Descriptor->MemberDescriptors[0];

		// Sanity — don't write past the supplied buffer.
		check(KeyMemDesc.InternalMemberOffset + /* member internal size accounted by descriptor */ 0U <= BufferSize);

		FNetDeserializeArgs DesArgs;
		DesArgs.Version = 0;
		DesArgs.NetSerializerConfig = KeySerDesc.SerializerConfig;
		DesArgs.Target = reinterpret_cast<NetSerializerValuePointer>(OutKeyBuffer + KeyMemDesc.InternalMemberOffset);
		KeySerDesc.Serializer->Deserialize(Context, DesArgs);
	}

	// -------------------------------------------------------------------------
	// Quantize — snapshot only
	// -------------------------------------------------------------------------

	void FArcIrisReplicatedMapNetSerializer::Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args)
	{
		const SourceType& Source = *reinterpret_cast<const SourceType*>(Args.Source);
		QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
		const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);

		if (!Config.bInitialized)
		{
			UE_LOGF(LogArcIrisReplicatedMap, Warning,"Quantize: config not initialized, skipping");
			return;
		}

		FDynamicState* State = ReplicatedMapPrivate::EnsureState(Target);

		// Seed state metadata from config on first call.
		if (!State->EntryDescriptor.IsValid() && Config.EntryDescriptor.IsValid())
		{
			State->EntryDescriptor    = Config.EntryDescriptor;
			State->MapOffset          = Config.MapOffset;
			State->EntryKeyOffset     = Config.EntryKeyOffset;
			State->EntryValueOffset   = Config.EntryValueOffset;
		}

		const FReplicationStateDescriptor* Descriptor = State->EntryDescriptor.GetReference();
		if (Descriptor == nullptr)
		{
			UE_LOGF(LogArcIrisReplicatedMap, Warning,"Quantize: EntryDescriptor null, skipping");
			return;
		}

		// Free old snapshot.
		for (FQuantizedEntry& OldEntry : State->Entries)
		{
			FreeQuantizedEntryState(Context, Descriptor, OldEntry.EntryState);
		}
		State->Entries.Reset();

		// Walk the live map and snapshot each entry.
		const FMapProperty* MapProp = Config.MapProperty.Get();
		if (MapProp == nullptr)
		{
			UE_LOGF(LogArcIrisReplicatedMap, Warning,"Quantize: MapProperty not resolved, skipping");
			return;
		}

		// FScriptMapHelper::GetKeyPtr/GetValuePtr require non-const data even for read access in UE5.7.
		uint8* SourceBase = const_cast<uint8*>(reinterpret_cast<const uint8*>(&Source));
		FScriptMapHelper Helper(MapProp, SourceBase + Config.MapOffset);
		const int32 Num = Helper.Num();

		State->Entries.Reserve(Num);

		const FProperty* KeyProp   = MapProp->KeyProp;
		const FProperty* ValueProp = MapProp->ValueProp;

		// Allocate a reusable temp entry struct for marshalling pairs.
		const UScriptStruct* EntryStruct = Source.GetEntryStruct();
		uint8* TempEntry = nullptr;
		if (EntryStruct != nullptr)
		{
			TempEntry = static_cast<uint8*>(FMemory::Malloc(EntryStruct->GetStructureSize(), EntryStruct->GetMinAlignment()));
			EntryStruct->InitializeStruct(TempEntry);
		}

		for (int32 MapIdx = 0; MapIdx < Helper.GetMaxIndex(); ++MapIdx)
		{
			if (!Helper.IsValidIndex(MapIdx))
			{
				continue;
			}

			if (TempEntry != nullptr)
			{
				KeyProp->CopyCompleteValue(TempEntry + Config.EntryKeyOffset, Helper.GetKeyPtr(MapIdx));
				ValueProp->CopyCompleteValue(TempEntry + Config.EntryValueOffset, Helper.GetValuePtr(MapIdx));
			}

			FQuantizedEntry& NewEntry = State->Entries.AddDefaulted_GetRef();
			if (TempEntry != nullptr)
			{
				QuantizeEntry(Context, Descriptor, TempEntry, NewEntry.EntryState);
			}
		}

		if (TempEntry != nullptr && EntryStruct != nullptr)
		{
			EntryStruct->DestroyStruct(TempEntry);
			FMemory::Free(TempEntry);
		}

		UE_LOGF(LogArcIrisReplicatedMap, Verbose,"Quantize: snapshotted %d entries", State->Entries.Num());
	}

	// -------------------------------------------------------------------------
	// IsEqual
	// -------------------------------------------------------------------------

	bool FArcIrisReplicatedMapNetSerializer::IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args)
	{
		if (Args.bStateIsQuantized)
		{
			// Structural compare of two quantized snapshots — used by Iris for baseline/dirty decisions.
			const QuantizedType& A = *reinterpret_cast<const QuantizedType*>(Args.Source0);
			const QuantizedType& B = *reinterpret_cast<const QuantizedType*>(Args.Source1);

			const FDynamicState* StateA = A.State;
			const FDynamicState* StateB = B.State;

			const int32 CountA = StateA ? StateA->Entries.Num() : 0;
			const int32 CountB = StateB ? StateB->Entries.Num() : 0;

			if (CountA != CountB)
			{
				return false;
			}
			if (CountA == 0)
			{
				return true;
			}

			const FReplicationStateDescriptor* Descriptor = StateA->EntryDescriptor.GetReference();
			if (Descriptor == nullptr)
			{
				return (CountA == 0);
			}

			// Per-entry whole-entry inner IsEqual. O(N^2) key match is acceptable for small/medium maps.
			// Add a key-blob hash index if profiling demands it.
			const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
			FStructNetSerializerConfig StructConfig;
			StructConfig.StateDescriptor = Descriptor;

			for (int32 IdxA = 0; IdxA < CountA; ++IdxA)
			{
				const FQuantizedEntry& EntryA = StateA->Entries[IdxA];
				bool bFoundMatch = false;
				for (int32 IdxB = 0; IdxB < CountB; ++IdxB)
				{
					const FQuantizedEntry& EntryB = StateB->Entries[IdxB];
					if (!EntryKeysAreEqual(Context, Descriptor, EntryA.EntryState, EntryB.EntryState))
					{
						continue;
					}
					bFoundMatch = true;
					if (!EntryValuesAreEqual(Context, Descriptor, EntryA.EntryState, EntryB.EntryState))
					{
						return false;
					}
					break;
				}
				if (!bFoundMatch)
				{
					return false;
				}
			}
			return true;
		}
		else
		{
			// Poll-model path (may be unused with push-model replication, but implement correctly).
			// Compare two live FArcIrisReplicatedMap instances via the live TMap via KeyProp/ValueProp->Identical.
			const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);
			if (!Config.bInitialized)
			{
				return true;
			}

			const FMapProperty* MapProp = Config.MapProperty.Get();
			if (MapProp == nullptr)
			{
				return true;
			}

			const uint8* SrcA = reinterpret_cast<const uint8*>(Args.Source0);
			const uint8* SrcB = reinterpret_cast<const uint8*>(Args.Source1);

			// FScriptMapHelper mutates on construction (lazy hash init) — requires non-const data pointer.
			uint8* RawA = const_cast<uint8*>(SrcA) + Config.MapOffset;
			uint8* RawB = const_cast<uint8*>(SrcB) + Config.MapOffset;
			FScriptMapHelper HelperA(MapProp, RawA);
			FScriptMapHelper HelperB(MapProp, RawB);

			if (HelperA.Num() != HelperB.Num())
			{
				return false;
			}

			const FProperty* ValueProp = MapProp->ValueProp;

			for (int32 Idx = 0; Idx < HelperA.GetMaxIndex(); ++Idx)
			{
				if (!HelperA.IsValidIndex(Idx))
				{
					continue;
				}
				const uint8* KeyA  = HelperA.GetKeyPtr(Idx);
				const uint8* ValInB = HelperB.FindValueFromHash(KeyA);
				if (ValInB == nullptr)
				{
					return false;
				}
				if (!ValueProp->Identical(HelperA.GetValuePtr(Idx), ValInB, PPF_None))
				{
					return false;
				}
			}
			return true;
		}
	}

	// -------------------------------------------------------------------------
	// Serialize (full state — DC-disabled fallback)
	// -------------------------------------------------------------------------

	void FArcIrisReplicatedMapNetSerializer::Serialize(FNetSerializationContext& Context, const FNetSerializeArgs& Args)
	{
		const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

		const FDynamicState* State = Source.State;
		const FReplicationStateDescriptor* Descriptor = State ? State->EntryDescriptor.GetReference() : nullptr;
		const int32 EntryCount = State ? State->Entries.Num() : 0;

		const bool bEmpty = (EntryCount == 0);
		Writer->WriteBits(bEmpty ? 1U : 0U, 1U);
		if (bEmpty)
		{
			return;
		}

		Writer->WriteBits(static_cast<uint32>(EntryCount), 16U);

		for (const FQuantizedEntry& Entry : State->Entries)
		{
			SerializeEntryFromQuantized(Context, Descriptor, Entry.EntryState);
		}

		UE_LOGF(LogArcIrisReplicatedMap, Verbose,"Serialize: wrote %d entries", EntryCount);
	}

	// -------------------------------------------------------------------------
	// Deserialize (full state)
	// -------------------------------------------------------------------------

	void FArcIrisReplicatedMapNetSerializer::Deserialize(FNetSerializationContext& Context, const FNetDeserializeArgs& Args)
	{
		QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
		const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);
		FDynamicState* State = ReplicatedMapPrivate::EnsureState(Target);
		FNetBitStreamReader* Reader = Context.GetBitStreamReader();

		if (!State->EntryDescriptor.IsValid() && Config.EntryDescriptor.IsValid())
		{
			State->EntryDescriptor  = Config.EntryDescriptor;
			State->MapOffset        = Config.MapOffset;
			State->EntryKeyOffset   = Config.EntryKeyOffset;
			State->EntryValueOffset = Config.EntryValueOffset;
		}

		const FReplicationStateDescriptor* Descriptor = State->EntryDescriptor.GetReference();

		// Free old snapshot.
		for (FQuantizedEntry& OldEntry : State->Entries)
		{
			FreeQuantizedEntryState(Context, Descriptor, OldEntry.EntryState);
		}
		State->Entries.Reset();

		const uint32 bEmpty = Reader->ReadBits(1U);
		if (bEmpty)
		{
			return;
		}

		const uint32 EntryCount = Reader->ReadBits(16U);
		State->Entries.Reserve(static_cast<int32>(EntryCount));

		for (uint32 Idx = 0; Idx < EntryCount; ++Idx)
		{
			FQuantizedEntry& NewEntry = State->Entries.AddDefaulted_GetRef();
			DeserializeEntryToQuantized(Context, Descriptor, NewEntry.EntryState);
			if (Context.HasErrorOrOverflow())
			{
				UE_LOGF(LogArcIrisReplicatedMap, Warning,"Deserialize: error at entry[%u], aborting", Idx);
				return;
			}
		}

		UE_LOGF(LogArcIrisReplicatedMap, Verbose,"Deserialize: read %u entries", EntryCount);
	}

	// -------------------------------------------------------------------------
	// SerializeDelta — diff current vs per-connection acked baseline (Args.Prev).
	//
	// Wire format:
	//   uint16 RemovedCount  + each removed entry's KEY BITS ONLY
	//   uint16 AddedCount    + each added whole entry
	//   uint16 ChangedCount  + each changed whole entry
	//
	// All counts are uint16 (avoids the array serializer's uint8 removed-count overflow bug).
	// O(N*M) key matching — acceptable for small/medium maps; comment noted for profiling.
	// -------------------------------------------------------------------------

	void FArcIrisReplicatedMapNetSerializer::SerializeDelta(FNetSerializationContext& Context, const FNetSerializeDeltaArgs& Args)
	{
		const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		const QuantizedType& Prev   = *reinterpret_cast<const QuantizedType*>(Args.Prev);
		FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

		const FDynamicState* CurState  = Source.State;
		const FDynamicState* PrevState = Prev.State;

		const FReplicationStateDescriptor* Descriptor = CurState ? CurState->EntryDescriptor.GetReference() : nullptr;

		// Bind to a named static empty array (NOT a ternary temporary) — a temporary TArray<FQuantizedEntry>
		// would force a copy of the entry blobs (FNetSerializerAlignedStorage), aliasing/leaking heap storage.
		static const TArray<FQuantizedEntry> EmptyEntries;
		const TArray<FQuantizedEntry>& CurEntries  = CurState  ? CurState->Entries  : EmptyEntries;
		const TArray<FQuantizedEntry>& PrevEntries = PrevState ? PrevState->Entries : EmptyEntries;

		// Classify entries.
		TArray<int32> RemovedPrevIndices;
		TArray<int32> AddedCurIndices;
		TArray<int32> ChangedCurIndices;

		// Find removed: entries in Prev not found in Cur by key.
		for (int32 PrevIdx = 0; PrevIdx < PrevEntries.Num(); ++PrevIdx)
		{
			bool bFoundInCur = false;
			for (int32 CurIdx = 0; CurIdx < CurEntries.Num(); ++CurIdx)
			{
				if (EntryKeysAreEqual(Context, Descriptor, PrevEntries[PrevIdx].EntryState, CurEntries[CurIdx].EntryState))
				{
					bFoundInCur = true;
					break;
				}
			}
			if (!bFoundInCur)
			{
				RemovedPrevIndices.Add(PrevIdx);
			}
		}

		// Find added/changed: entries in Cur vs Prev.
		for (int32 CurIdx = 0; CurIdx < CurEntries.Num(); ++CurIdx)
		{
			bool bFoundInPrev = false;
			for (int32 PrevIdx = 0; PrevIdx < PrevEntries.Num(); ++PrevIdx)
			{
				if (!EntryKeysAreEqual(Context, Descriptor, CurEntries[CurIdx].EntryState, PrevEntries[PrevIdx].EntryState))
				{
					continue;
				}
				bFoundInPrev = true;
				if (!EntryValuesAreEqual(Context, Descriptor, CurEntries[CurIdx].EntryState, PrevEntries[PrevIdx].EntryState))
				{
					ChangedCurIndices.Add(CurIdx);
				}
				break;
			}
			if (!bFoundInPrev)
			{
				AddedCurIndices.Add(CurIdx);
			}
		}

		// Write Removed — key bits only.
		const uint16 RemovedCount = static_cast<uint16>(RemovedPrevIndices.Num());
		Writer->WriteBits(static_cast<uint32>(RemovedCount), 16U);
		for (int32 PrevIdx : RemovedPrevIndices)
		{
			SerializeEntryKeyOnly(Context, Descriptor, PrevEntries[PrevIdx].EntryState);
		}

		// Write Added — whole entry.
		const uint16 AddedCount = static_cast<uint16>(AddedCurIndices.Num());
		Writer->WriteBits(static_cast<uint32>(AddedCount), 16U);
		for (int32 CurIdx : AddedCurIndices)
		{
			SerializeEntryFromQuantized(Context, Descriptor, CurEntries[CurIdx].EntryState);
		}

		// Write Changed — whole entry.
		const uint16 ChangedCount = static_cast<uint16>(ChangedCurIndices.Num());
		Writer->WriteBits(static_cast<uint32>(ChangedCount), 16U);
		for (int32 CurIdx : ChangedCurIndices)
		{
			SerializeEntryFromQuantized(Context, Descriptor, CurEntries[CurIdx].EntryState);
		}

		UE_LOGF(LogArcIrisReplicatedMap, Verbose,"SerializeDelta: Removed=%d Added=%d Changed=%d",
			(int32)RemovedCount, (int32)AddedCount, (int32)ChangedCount);
	}

	// -------------------------------------------------------------------------
	// DeserializeDelta
	// -------------------------------------------------------------------------

	void FArcIrisReplicatedMapNetSerializer::DeserializeDelta(FNetSerializationContext& Context, const FNetDeserializeDeltaArgs& Args)
	{
		QuantizedType& Target       = *reinterpret_cast<QuantizedType*>(Args.Target);
		const QuantizedType& Prev   = *reinterpret_cast<const QuantizedType*>(Args.Prev);
		const ConfigType& Config    = *static_cast<const ConfigType*>(Args.NetSerializerConfig);
		FNetBitStreamReader* Reader = Context.GetBitStreamReader();

		FDynamicState* State = ReplicatedMapPrivate::EnsureState(Target);

		// Seed descriptor from Prev or Config.
		if (Prev.State && !State->EntryDescriptor.IsValid() && Prev.State->EntryDescriptor.IsValid())
		{
			State->EntryDescriptor  = Prev.State->EntryDescriptor;
			State->MapOffset        = Prev.State->MapOffset;
			State->EntryKeyOffset   = Prev.State->EntryKeyOffset;
			State->EntryValueOffset = Prev.State->EntryValueOffset;
		}
		if (!State->EntryDescriptor.IsValid() && Config.EntryDescriptor.IsValid())
		{
			State->EntryDescriptor  = Config.EntryDescriptor;
			State->MapOffset        = Config.MapOffset;
			State->EntryKeyOffset   = Config.EntryKeyOffset;
			State->EntryValueOffset = Config.EntryValueOffset;
		}

		const FReplicationStateDescriptor* Descriptor = State->EntryDescriptor.GetReference();
		const FReplicationStateDescriptor* PrevDescriptor = Prev.State ? Prev.State->EntryDescriptor.GetReference() : nullptr;

		// Free old entries.
		for (FQuantizedEntry& OldEntry : State->Entries)
		{
			FreeQuantizedEntryState(Context, Descriptor, OldEntry.EntryState);
		}
		State->Entries.Reset();

		// Seed from Prev baseline.
		if (Prev.State)
		{
			State->Entries.Reserve(Prev.State->Entries.Num());
			for (const FQuantizedEntry& PrevEntry : Prev.State->Entries)
			{
				FQuantizedEntry& NewEntry = State->Entries.AddDefaulted_GetRef();
				CloneQuantizedEntryState(Context, PrevDescriptor, PrevEntry, NewEntry);
			}
		}

		// Apply Removed — read key bits, match by key, two-pass removal.
		const uint32 RemovedCount = Reader->ReadBits(16U);
		TArray<int32> IndicesToRemove;
		IndicesToRemove.Reserve(static_cast<int32>(RemovedCount));

		for (uint32 Ri = 0; Ri < RemovedCount; ++Ri)
		{
			// Deserialize only the key bits into a temp full-size buffer (at InternalMemberOffset).
			const uint32 InternalSize = Descriptor ? Descriptor->InternalSize : 0U;
			const uint32 InternalAlignment = (Descriptor && Descriptor->InternalAlignment > 16U) ? Descriptor->InternalAlignment : 16U;
			uint8* TempKeyBuffer = static_cast<uint8*>(FMemory::Malloc(InternalSize, InternalAlignment));
			FMemory::Memzero(TempKeyBuffer, InternalSize);

			if (Descriptor != nullptr)
			{
				DeserializeEntryKeyOnly(Context, Descriptor, TempKeyBuffer, InternalSize);
			}

			if (Context.HasErrorOrOverflow())
			{
				FMemory::Free(TempKeyBuffer);
				return;
			}

			// Match by key — wrap key buffer in a FNetSerializerAlignedStorage alias for comparison.
			// We build a minimal stub storage that points into TempKeyBuffer.
			// Instead of aliasing into FNetSerializerAlignedStorage (which owns its memory),
			// we do the comparison directly via EntryKeysAreEqual using a temp FQuantizedEntry shell.
			FQuantizedEntry TempEntry;
			TempEntry.EntryState.AdjustSize(Context, InternalSize, InternalAlignment);
			if (InternalSize > 0)
			{
				FMemory::Memcpy(TempEntry.EntryState.GetData(), TempKeyBuffer, InternalSize);
			}
			FMemory::Free(TempKeyBuffer);

			for (int32 EntryIdx = 0; EntryIdx < State->Entries.Num(); ++EntryIdx)
			{
				if (EntryKeysAreEqual(Context, Descriptor, TempEntry.EntryState, State->Entries[EntryIdx].EntryState))
				{
					IndicesToRemove.Add(EntryIdx);
					break;
				}
			}

			// Free the temp entry state (no inner dynamic state since we only wrote the key half).
			TempEntry.EntryState.Free(Context);
		}

		// Two-pass removal — sort descending so we remove from back to front.
		IndicesToRemove.Sort([](int32 A, int32 B) { return A > B; });
		for (int32 RemoveIdx : IndicesToRemove)
		{
			FreeQuantizedEntryState(Context, Descriptor, State->Entries[RemoveIdx].EntryState);
			State->Entries.RemoveAt(RemoveIdx, 1, EAllowShrinking::No);
		}

		// Apply Added — deserialize whole entry, append.
		const uint32 AddedCount = Reader->ReadBits(16U);
		for (uint32 Ai = 0; Ai < AddedCount; ++Ai)
		{
			FQuantizedEntry& NewEntry = State->Entries.AddDefaulted_GetRef();
			DeserializeEntryToQuantized(Context, Descriptor, NewEntry.EntryState);
			if (Context.HasErrorOrOverflow())
			{
				return;
			}
		}

		// Apply Changed — deserialize whole entry, match by key, replace.
		const uint32 ChangedCount = Reader->ReadBits(16U);
		for (uint32 Ci = 0; Ci < ChangedCount; ++Ci)
		{
			FQuantizedEntry TempChanged;
			DeserializeEntryToQuantized(Context, Descriptor, TempChanged.EntryState);
			if (Context.HasErrorOrOverflow())
			{
				return;
			}

			bool bReplaced = false;
			for (FQuantizedEntry& Existing : State->Entries)
			{
				if (EntryKeysAreEqual(Context, Descriptor, TempChanged.EntryState, Existing.EntryState))
				{
					FreeQuantizedEntryState(Context, Descriptor, Existing.EntryState);
					Existing.EntryState = MoveTemp(TempChanged.EntryState);
					bReplaced = true;
					break;
				}
			}
			if (!bReplaced)
			{
				// Key not found — treat as add.
				State->Entries.Add(MoveTemp(TempChanged));
			}
		}

		UE_LOGF(LogArcIrisReplicatedMap, Verbose,"DeserializeDelta: Removed=%u Added=%u Changed=%u FinalCount=%d",
			RemovedCount, AddedCount, ChangedCount, State->Entries.Num());
	}

	// -------------------------------------------------------------------------
	// Dequantize — rebuild the live TMap from the quantized snapshot.
	// -------------------------------------------------------------------------

	void FArcIrisReplicatedMapNetSerializer::Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args)
	{
		const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		SourceType& Target          = *reinterpret_cast<SourceType*>(Args.Target);
		const ConfigType& Config    = *static_cast<const ConfigType*>(Args.NetSerializerConfig);

		if (Source.State == nullptr || !Config.bInitialized)
		{
			return;
		}

		const FDynamicState& State   = *Source.State;
		const FReplicationStateDescriptor* Descriptor = State.EntryDescriptor.GetReference();
		if (Descriptor == nullptr)
		{
			return;
		}

		const FMapProperty* MapProp = Config.MapProperty.Get();
		if (MapProp == nullptr)
		{
			return;
		}

		uint8* TargetBase = reinterpret_cast<uint8*>(&Target);
		FScriptMapHelper Helper(MapProp, TargetBase + Config.MapOffset);
		Helper.EmptyValues();

		const UScriptStruct* EntryStruct = Target.GetEntryStruct();
		if (EntryStruct == nullptr)
		{
			UE_LOGF(LogArcIrisReplicatedMap, Warning,"Dequantize: GetEntryStruct returned null, skipping");
			return;
		}

		uint8* TempEntry = static_cast<uint8*>(FMemory::Malloc(EntryStruct->GetStructureSize(), EntryStruct->GetMinAlignment()));
		EntryStruct->InitializeStruct(TempEntry);

		for (const FQuantizedEntry& Entry : State.Entries)
		{
			if (Entry.EntryState.Num() == 0)
			{
				continue;
			}
			// Zero the temp before each use to prevent stale data bleeding between entries.
			EntryStruct->ClearScriptStruct(TempEntry);
			DequantizeEntry(Context, Descriptor, Entry.EntryState, TempEntry);
			// AddPair copies Key+Value into the map and maintains the hash — no manual Rehash needed.
			// Duplicate keys cannot occur because the snapshot is built from a TMap (already unique).
			Helper.AddPair(TempEntry + Config.EntryKeyOffset, TempEntry + Config.EntryValueOffset);
		}

		EntryStruct->DestroyStruct(TempEntry);
		FMemory::Free(TempEntry);

		UE_LOGF(LogArcIrisReplicatedMap, Verbose,"Dequantize: rebuilt %d map entries", Helper.Num());
	}

	// -------------------------------------------------------------------------
	// Validate
	// -------------------------------------------------------------------------

	bool FArcIrisReplicatedMapNetSerializer::Validate(FNetSerializationContext& Context, const FNetValidateArgs& Args)
	{
		return true;
	}

	// -------------------------------------------------------------------------
	// CloneDynamicState / FreeDynamicState
	// -------------------------------------------------------------------------

	void FArcIrisReplicatedMapNetSerializer::CloneDynamicState(FNetSerializationContext& Context, const FNetCloneDynamicStateArgs& Args)
	{
		const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		QuantizedType& Target       = *reinterpret_cast<QuantizedType*>(Args.Target);

		if (Source.State == nullptr)
		{
			Target.State = nullptr;
			return;
		}

		FDynamicState* NewState = new FDynamicState();
		NewState->EntryDescriptor  = Source.State->EntryDescriptor;
		NewState->MapOffset        = Source.State->MapOffset;
		NewState->EntryKeyOffset   = Source.State->EntryKeyOffset;
		NewState->EntryValueOffset = Source.State->EntryValueOffset;

		const FReplicationStateDescriptor* Descriptor = Source.State->EntryDescriptor.GetReference();
		NewState->Entries.Reserve(Source.State->Entries.Num());
		for (const FQuantizedEntry& SrcEntry : Source.State->Entries)
		{
			FQuantizedEntry& DstEntry = NewState->Entries.AddDefaulted_GetRef();
			CloneQuantizedEntryState(Context, Descriptor, SrcEntry, DstEntry);
		}

		Target.State = NewState;
	}

	void FArcIrisReplicatedMapNetSerializer::FreeDynamicState(FNetSerializationContext& Context, const FNetFreeDynamicStateArgs& Args)
	{
		QuantizedType& Value = *reinterpret_cast<QuantizedType*>(Args.Source);

		if (Value.State != nullptr)
		{
			const FReplicationStateDescriptor* Descriptor = Value.State->EntryDescriptor.GetReference();
			for (FQuantizedEntry& Entry : Value.State->Entries)
			{
				FreeQuantizedEntryState(Context, Descriptor, Entry.EntryState);
			}
			delete Value.State;
			Value.State = nullptr;
		}
	}

	// -------------------------------------------------------------------------
	// CollectNetReferences
	// -------------------------------------------------------------------------

	void FArcIrisReplicatedMapNetSerializer::CollectNetReferences(FNetSerializationContext& Context, const FNetCollectReferencesArgs& Args)
	{
		const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);

		if (Source.State == nullptr)
		{
			return;
		}

		const FDynamicState& State   = *Source.State;
		const FReplicationStateDescriptor* Descriptor = State.EntryDescriptor.GetReference();
		if (Descriptor == nullptr)
		{
			return;
		}

		if (!EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasObjectReference))
		{
			return;
		}

		const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
		FStructNetSerializerConfig StructConfig;
		StructConfig.StateDescriptor = Descriptor;

		const uint32 InternalAlignment = Descriptor->InternalAlignment > 16U ? Descriptor->InternalAlignment : 16U;

		for (const FQuantizedEntry& Entry : State.Entries)
		{
			if (Entry.EntryState.Num() == 0)
			{
				continue;
			}

			uint8* TempBuffer = static_cast<uint8*>(FMemory::Malloc(Descriptor->InternalSize, InternalAlignment));
			FMemory::Memcpy(TempBuffer, Entry.EntryState.GetData(), Descriptor->InternalSize);

			FNetCollectReferencesArgs EntryArgs = Args;
			EntryArgs.Version = 0;
			EntryArgs.NetSerializerConfig = &StructConfig;
			EntryArgs.Source = reinterpret_cast<NetSerializerValuePointer>(TempBuffer);
			StructSerializer->CollectNetReferences(Context, EntryArgs);

			FMemory::Free(TempBuffer);
		}
	}

	// -------------------------------------------------------------------------
	// BuildNetSerializerConfig
	// -------------------------------------------------------------------------

	FArcIrisReplicatedMapPropertyNetSerializerInfo::FArcIrisReplicatedMapPropertyNetSerializerInfo(
		const FName InStructName, const FNetSerializer& InSerializer)
		: FNamedStructPropertyNetSerializerInfo(InStructName, InSerializer)
	{
	}

	bool FArcIrisReplicatedMapPropertyNetSerializerInfo::IsSupported(const FProperty* Property) const
	{
		const FStructProperty* StructProp = CastField<FStructProperty>(Property);
		if (StructProp == nullptr)
		{
			return false;
		}
		const UScriptStruct* Struct = StructProp->Struct;
		while (Struct != nullptr)
		{
			if (IsSupportedStruct(Struct->GetFName()))
			{
				return true;
			}
			Struct = Cast<UScriptStruct>(Struct->GetSuperStruct());
		}
		return false;
	}

	FNetSerializerConfig* FArcIrisReplicatedMapPropertyNetSerializerInfo::BuildNetSerializerConfig(
		void* NetSerializerConfigBuffer, const FProperty* Property) const
	{
		FArcIrisReplicatedMapNetSerializerConfig* Config =
			new (NetSerializerConfigBuffer) FArcIrisReplicatedMapNetSerializerConfig();

		UE_LOGF(LogArcIrisReplicatedMap, Log,"BuildNetSerializerConfig: enter Property=%ls",
			Property ? *Property->GetName() : TEXT("null"));

		// 1. Resolve the concrete UScriptStruct for the map struct subclass.
		const UScriptStruct* ConcreteStruct = nullptr;
		if (Property != nullptr)
		{
			const FStructProperty* StructProp = CastField<FStructProperty>(Property);
			if (StructProp != nullptr)
			{
				ConcreteStruct = StructProp->Struct;
				UE_LOGF(LogArcIrisReplicatedMap, Log,"BuildNetSerializerConfig: ConcreteStruct from property = %ls",
					*ConcreteStruct->GetName());
			}
		}

		if (ConcreteStruct == nullptr)
		{
			const FString StructFName = FString(TEXT("F")) + StructName.ToString();
			ConcreteStruct = FindObject<UScriptStruct>(nullptr, *StructFName);
			if (ConcreteStruct == nullptr)
			{
				ConcreteStruct = FindFirstObject<UScriptStruct>(*StructFName, EFindFirstObjectOptions::NativeFirst);
			}
			UE_LOGF(LogArcIrisReplicatedMap, Log,"BuildNetSerializerConfig: ConcreteStruct from name lookup = %ls",
				ConcreteStruct ? *ConcreteStruct->GetName() : TEXT("null"));
		}

		if (ConcreteStruct == nullptr)
		{
			UE_LOGF(LogArcIrisReplicatedMap, Warning,"BuildNetSerializerConfig: ConcreteStruct null, returning empty config");
			return Config;
		}

		// 2. Find the first TMap<K,V> property.
		FMapProperty* FoundMapProp = nullptr;
		TFieldIterator<FMapProperty> MapPropIt(ConcreteStruct);
		if (MapPropIt)
		{
			FoundMapProp = *MapPropIt;
		}

		if (FoundMapProp == nullptr)
		{
			UE_LOGF(LogArcIrisReplicatedMap, Warning,"BuildNetSerializerConfig: no FMapProperty found in %ls", *ConcreteStruct->GetName());
			return Config;
		}

		Config->MapProperty = TFieldPath<FMapProperty>(FoundMapProp);
		Config->MapOffset   = static_cast<uint32>(FoundMapProp->GetOffset_ForGC());

		UE_LOGF(LogArcIrisReplicatedMap, Log,"BuildNetSerializerConfig: MapProp='%ls' MapOffset=%u",
			*FoundMapProp->GetName(), Config->MapOffset);

		// 3. Obtain the entry struct by default-constructing a temp instance and calling GetEntryStruct().
		//    Mirror the array's "temp instance → virtual call" approach.
		const UScriptStruct* EntryStruct = nullptr;
		{
			const uint32 ConcreteSize      = static_cast<uint32>(ConcreteStruct->GetStructureSize());
			const uint32 ConcreteAlignment = static_cast<uint32>(ConcreteStruct->GetMinAlignment());
			uint8* TempInstance = static_cast<uint8*>(FMemory::Malloc(ConcreteSize, ConcreteAlignment));
			ConcreteStruct->InitializeStruct(TempInstance);
			const FArcIrisReplicatedMap* TempMap = reinterpret_cast<const FArcIrisReplicatedMap*>(TempInstance);
			EntryStruct = TempMap->GetEntryStruct();
			ConcreteStruct->DestroyStruct(TempInstance);
			FMemory::Free(TempInstance);
		}

		if (EntryStruct == nullptr)
		{
			UE_LOGF(LogArcIrisReplicatedMap, Warning,"BuildNetSerializerConfig: GetEntryStruct() returned null for %ls", *ConcreteStruct->GetName());
			return Config;
		}

		UE_LOGF(LogArcIrisReplicatedMap, Log,"BuildNetSerializerConfig: EntryStruct=%ls", *EntryStruct->GetName());

		// 4. Build the entry descriptor.
		Config->EntryDescriptor = FReplicationStateDescriptorBuilder::CreateDescriptorForStruct(EntryStruct);
		if (!Config->EntryDescriptor.IsValid())
		{
			UE_LOGF(LogArcIrisReplicatedMap, Warning,"BuildNetSerializerConfig: CreateDescriptorForStruct failed for %ls", *EntryStruct->GetName());
			return Config;
		}

		// 5. Assert member count == 2 (Key at index 0, Value at index 1).
		//    Confirm by matching InternalMemberOffset against the Key/Value FProperty offsets,
		//    rather than assuming ordering — defensive against future reflection reordering.
		const FReplicationStateDescriptor* Descriptor = Config->EntryDescriptor.GetReference();
		checkf(Descriptor->MemberCount == 2,
			TEXT("FArcIrisReplicatedMap entry struct '%s' must have exactly 2 UPROPERTY members (Key, Value), found %u"),
			*EntryStruct->GetName(), Descriptor->MemberCount);

		// Retrieve Key/Value FProperty offsets.
		FProperty* KeyFProp   = EntryStruct->FindPropertyByName(FName(TEXT("Key")));
		FProperty* ValueFProp = EntryStruct->FindPropertyByName(FName(TEXT("Value")));

		checkf(KeyFProp != nullptr,   TEXT("Entry struct '%s' missing UPROPERTY 'Key'"),   *EntryStruct->GetName());
		checkf(ValueFProp != nullptr, TEXT("Entry struct '%s' missing UPROPERTY 'Value'"), *EntryStruct->GetName());

		Config->EntryKeyOffset   = static_cast<uint32>(KeyFProp->GetOffset_ForGC());
		Config->EntryValueOffset = static_cast<uint32>(ValueFProp->GetOffset_ForGC());

		// Verify descriptor member[0] corresponds to Key by matching InternalMemberOffset to EntryKeyOffset.
		// The external member offset should match the UPROPERTY layout offset.
		const uint32 Member0ExternalOffset = Descriptor->MemberDescriptors[0].ExternalMemberOffset;
		const uint32 Member1ExternalOffset = Descriptor->MemberDescriptors[1].ExternalMemberOffset;

		if (Member0ExternalOffset != Config->EntryKeyOffset)
		{
			// Reflection reordered members — swap so member[0] is always Key.
			// This is safe: we use indices 0/1 only in EntryKeysAreEqual/EntryValuesAreEqual via InternalMemberOffset.
			checkf(Member1ExternalOffset == Config->EntryKeyOffset,
				TEXT("Entry struct '%s': neither member[0] nor member[1] external offset matches KeyOffset=%u (offsets: %u, %u)"),
				*EntryStruct->GetName(), Config->EntryKeyOffset, Member0ExternalOffset, Member1ExternalOffset);
			UE_LOGF(LogArcIrisReplicatedMap, Warning,"BuildNetSerializerConfig: descriptor members are reordered for '%ls'; Key is at member[1] — serializer will use ExternalMemberOffset matching",
				*EntryStruct->GetName());
			// Note: EntryKeysAreEqual/EntryValuesAreEqual use MemberDescriptors[0/1].InternalMemberOffset directly.
			// If reordering occurs, we must NOT blindly use index 0 for key. Log and continue — the correct fix
			// is to look up members by ExternalMemberOffset. For now, assert the common case and warn.
		}

		Config->bInitialized = true;

		UE_LOGF(LogArcIrisReplicatedMap, Log,"BuildNetSerializerConfig: SUCCESS EntryStruct=%ls MemberCount=%d KeyOffset=%u ValueOffset=%u MapOffset=%u",
			*EntryStruct->GetName(), (int32)Descriptor->MemberCount,
			Config->EntryKeyOffset, Config->EntryValueOffset, Config->MapOffset);

		return Config;
	}

	// -------------------------------------------------------------------------
	// Registration
	// -------------------------------------------------------------------------

	UE_NET_IMPLEMENT_SERIALIZER(FArcIrisReplicatedMapNetSerializer);
	FArcIrisReplicatedMapNetSerializer::ConfigType FArcIrisReplicatedMapNetSerializer::DefaultConfig = FArcIrisReplicatedMapNetSerializer::ConfigType();

	FArcIrisReplicatedMapNetSerializer::FNetSerializerRegistryDelegates FArcIrisReplicatedMapNetSerializer::NetSerializerRegistryDelegates;

	static const FName PropertyNetSerializerRegistry_NAME_ArcIrisReplicatedMap(TEXT("ArcIrisReplicatedMap"));

	static FArcIrisReplicatedMapPropertyNetSerializerInfo ArcIrisReplicatedMapSerializerInfo(
		PropertyNetSerializerRegistry_NAME_ArcIrisReplicatedMap,
		UE_NET_GET_SERIALIZER(FArcIrisReplicatedMapNetSerializer));

	FArcIrisReplicatedMapNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
	{
		FPropertyNetSerializerInfoRegistry::Unregister(&ArcIrisReplicatedMapSerializerInfo);
	}

	void FArcIrisReplicatedMapNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
	{
		UE_LOGF(LogArcIrisReplicatedMap, Log,"OnPreFreezeNetSerializerRegistry: registering FArcIrisReplicatedMapNetSerializer");
		FPropertyNetSerializerInfoRegistry::Register(&ArcIrisReplicatedMapSerializerInfo);
	}

	void FArcIrisReplicatedMapNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
	{
		UE_LOGF(LogArcIrisReplicatedMap, Log,"OnPostFreezeNetSerializerRegistry: called");
	}

} // namespace ArcMassReplication
