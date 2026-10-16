// Copyright Lukasz Baran. All Rights Reserved.

#include "Foliage/ArcEcotopeFoliageFieldCache.h"

#if WITH_EDITOR

#include "DerivedDataCache.h"
#include "DerivedDataCacheKey.h"
#include "DerivedDataCacheRecord.h"
#include "DerivedDataRequestOwner.h"
#include "DerivedDataValue.h"

namespace ArcEcotopeFoliageFieldCache_4e17
{
	UE::DerivedData::FCacheKey MakeCacheKey(const FIoHash& Hash)
	{
		UE::DerivedData::FCacheKey CacheKey;
		CacheKey.Bucket = UE::DerivedData::FCacheBucket(TEXT("ArcEcotopeFoliageField"));
		CacheKey.Hash = Hash;
		return CacheKey;
	}

	const UE::DerivedData::FValueId& TexelsValueId()
	{
		static const UE::DerivedData::FValueId ValueId = UE::DerivedData::FValueId::FromName("FieldTexels");
		return ValueId;
	}
}

namespace ArcEcotopeFoliageFieldCache
{
	FSharedBuffer TryGet(const FIoHash& Key, const TCHAR* DebugName)
	{
		using namespace UE::DerivedData;

		const FCacheGetRequest Request =
			{ UE::FSharedString(DebugName), ArcEcotopeFoliageFieldCache_4e17::MakeCacheKey(Key), ECachePolicy::Default };
		FRequestOwner RequestOwner(EPriority::Blocking);

		FSharedBuffer Buffer;
		GetCache().Get(MakeArrayView(&Request, 1), RequestOwner,
			[&Buffer](FCacheGetResponse&& Response)
			{
				if (Response.Status == EStatus::Ok)
				{
					Buffer = Response.Record
						.GetValue(ArcEcotopeFoliageFieldCache_4e17::TexelsValueId()).GetData().Decompress();
				}
			});
		RequestOwner.Wait();

		return Buffer;
	}

	void Put(const FIoHash& Key, FSharedBuffer&& Blob, const TCHAR* DebugName)
	{
		using namespace UE::DerivedData;

		if (Blob.IsNull())
		{
			return;
		}

		FValue Value = FValue::Compress(Blob);
		Blob.Reset();

		FCacheRecordBuilder RecordBuilder(ArcEcotopeFoliageFieldCache_4e17::MakeCacheKey(Key));
		RecordBuilder.AddValue(ArcEcotopeFoliageFieldCache_4e17::TexelsValueId(), Value);

		FRequestOwner RequestOwner(EPriority::Normal);
		const FCachePutRequest PutRequest =
			{ UE::FSharedString(DebugName), RecordBuilder.Build(), ECachePolicy::Default };
		GetCache().Put(MakeArrayView(&PutRequest, 1), RequestOwner);
		// Outlive this scope: the put is fire-and-forget, unlike the blocking get.
		RequestOwner.KeepAlive();
	}
}

#endif // WITH_EDITOR
