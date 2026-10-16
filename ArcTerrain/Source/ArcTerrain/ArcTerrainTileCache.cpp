// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainTileCache.h"

#if WITH_EDITOR

#include "DerivedDataCache.h"
#include "DerivedDataCacheKey.h"
#include "DerivedDataCacheRecord.h"
#include "DerivedDataRequestOwner.h"
#include "DerivedDataValue.h"

#include <atomic>

namespace ArcTerrainTileCache_9b3c
{
	UE::DerivedData::FCacheKey MakeCacheKey(const FIoHash& Hash)
	{
		UE::DerivedData::FCacheKey CacheKey;
		CacheKey.Bucket = UE::DerivedData::FCacheBucket(TEXT("ArcTerrainCell"));
		CacheKey.Hash = Hash;
		return CacheKey;
	}
}

namespace ArcTerrainTileCache
{
	FSharedBuffer TryGet(const FIoHash& Key, const UE::DerivedData::FValueId& ValueId, const TCHAR* DebugName)
	{
		using namespace UE::DerivedData;

		FCacheGetRequest Request =
			{ UE::FSharedString(DebugName), ArcTerrainTileCache_9b3c::MakeCacheKey(Key), ECachePolicy::Default };
		FRequestOwner RequestOwner(EPriority::Blocking);

		FSharedBuffer Buffer;
		GetCache().Get(MakeArrayView(&Request, 1), RequestOwner,
			[&Buffer, &ValueId](FCacheGetResponse&& Response)
			{
				if (Response.Status == EStatus::Ok)
				{
					Buffer = Response.Record.GetValue(ValueId).GetData().Decompress();
				}
			});
		RequestOwner.Wait();

		return Buffer;
	}

	struct FAsyncGet
	{
		explicit FAsyncGet(UE::DerivedData::EPriority Priority)
			: Owner(Priority)
		{
		}

		UE::DerivedData::FRequestOwner Owner;
		FSharedBuffer Buffer;
		std::atomic<bool> bComplete { false };
	};

	FAsyncGetPtr GetAsync(
		const FIoHash& Key,
		const UE::DerivedData::FValueId& ValueId,
		const TCHAR* Context,
		FArcTerrainAsyncGetCompletion&& Completion)
	{
		using namespace UE::DerivedData;

		TSharedRef<FAsyncGet, ESPMode::ThreadSafe> State =
			MakeShared<FAsyncGet, ESPMode::ThreadSafe>(EPriority::Normal);
		// Without KeepAlive the owner cancels — and JOINS — from its destructor, which for a shared state
		// the callback itself may release would both block the dropper and trip the barrier deadlock check.
		State->Owner.KeepAlive();

		const FCacheGetRequest Request =
			{ UE::FSharedString(Context), ArcTerrainTileCache_9b3c::MakeCacheKey(Key), ECachePolicy::Default };
		GetCache().Get(MakeArrayView(&Request, 1), State->Owner,
			[State, ValueId, Completion = MoveTemp(Completion)](FCacheGetResponse&& Response) mutable
			{
				if (Response.Status == EStatus::Ok)
				{
					State->Buffer = Response.Record.GetValue(ValueId).GetData().Decompress();
				}
				State->bComplete.store(true, std::memory_order_release);
				if (Completion)
				{
					Completion();
				}
			});
		return State;
	}

	bool IsComplete(const FAsyncGetPtr& Request)
	{
		return Request.IsValid() && Request->bComplete.load(std::memory_order_acquire);
	}

	FSharedBuffer TakeBuffer(const FAsyncGetPtr& Request)
	{
		if (!IsComplete(Request))
		{
			return FSharedBuffer();
		}
		return MoveTemp(Request->Buffer);
	}

	void Put(const FIoHash& Key, const UE::DerivedData::FValueId& ValueId, FSharedBuffer&& Blob, const TCHAR* DebugName)
	{
		using namespace UE::DerivedData;

		if (Blob.IsNull())
		{
			return;
		}

		FValue Value = FValue::Compress(Blob);
		Blob.Reset();

		FCacheRecordBuilder RecordBuilder(ArcTerrainTileCache_9b3c::MakeCacheKey(Key));
		RecordBuilder.AddValue(ValueId, Value);

		FRequestOwner RequestOwner(EPriority::Normal);
		const FCachePutRequest PutRequest =
			{ UE::FSharedString(DebugName), RecordBuilder.Build(), ECachePolicy::Default };
		GetCache().Put(MakeArrayView(&PutRequest, 1), RequestOwner);
		// Outlive this scope: the put is fire-and-forget, unlike the blocking get.
		RequestOwner.KeepAlive();
	}

	const UE::DerivedData::FValueId& HeightTexelsValueId()
	{
		static const UE::DerivedData::FValueId ValueId = UE::DerivedData::FValueId::FromName("HeightTexels");
		return ValueId;
	}

	const UE::DerivedData::FValueId& WeightPlanesValueId()
	{
		static const UE::DerivedData::FValueId ValueId = UE::DerivedData::FValueId::FromName("WeightPlanes");
		return ValueId;
	}

	const UE::DerivedData::FValueId& DataMasksValueId()
	{
		static const UE::DerivedData::FValueId ValueId = UE::DerivedData::FValueId::FromName("DataMasks");
		return ValueId;
	}

	const UE::DerivedData::FValueId& RailValueId(EArcTerrainRail Rail)
	{
		switch (Rail)
		{
		case EArcTerrainRail::Weight:
			return WeightPlanesValueId();
		case EArcTerrainRail::Data:
			return DataMasksValueId();
		default:
			return HeightTexelsValueId();
		}
	}
}

#endif // WITH_EDITOR
