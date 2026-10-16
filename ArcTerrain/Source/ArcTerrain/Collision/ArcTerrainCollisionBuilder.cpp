// Copyright Lukasz Baran. All Rights Reserved.

#include "Collision/ArcTerrainCollisionBuilder.h"
#include "Collision/ArcTerrainHeightfieldRef.h"
#include "ArcTerrainChannelSet.h"
#include "Stamps/ArcTerrainStampComponent.h"

#if WITH_EDITOR
#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"
#include "Hash/Blake3.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Chaos/ChaosArchive.h"
#include <atomic>
#endif

namespace ArcTerrainCollisionBuilderPrivate
{
	constexpr int32 GCollisionBlobVersion = 1;

#if WITH_EDITOR
	void HashString(FBlake3& Hash, const FString& String)
	{
		Hash.Update(*String, String.Len() * sizeof(TCHAR));
	}

	template <typename T>
	void HashPod(FBlake3& Hash, const T& Value)
	{
		Hash.Update(&Value, sizeof(T));
	}

	// Hash one raster source's identity + decode fields (shared by the height source and carve
	// layers). ContentId is the content identity (FTextureSource::GetId() / baked-field GUID) — the
	// raster bytes never need hashing.
	void HashSourceFields(FBlake3& Hash, EArcStampSourceKind Kind, const FGuid& ContentId,
		FIntPoint SourceDims,
		EArcTerrainSourceChannel SourceChannel, float EncodingScale, float ZeroInEncoding,
		bool bDeriveZeroFromBorder)
	{
		HashPod(Hash, static_cast<uint8>(Kind));
		HashPod(Hash, ContentId);
		HashPod(Hash, SourceDims);
		HashPod(Hash, static_cast<uint8>(SourceChannel));
		HashPod(Hash, EncodingScale);
		HashPod(Hash, ZeroInEncoding);
		HashPod(Hash, static_cast<uint8>(bDeriveZeroFromBorder ? 1 : 0));
	}

	UE::DerivedData::FCacheKey MakeCacheKey(const FIoHash& Hash)
	{
		UE::DerivedData::FCacheKey CacheKey;
		CacheKey.Bucket = UE::DerivedData::FCacheBucket(TEXT("ArcTerrainCell"));
		CacheKey.Hash = Hash;
		return CacheKey;
	}

	const UE::DerivedData::FValueId& HeightfieldValueId()
	{
		static const UE::DerivedData::FValueId ValueId = UE::DerivedData::FValueId::FromName("HeightfieldCollision");
		return ValueId;
	}

	// Blob -> geometry. Heap work over a private buffer and nothing else: the FChaosArchive read owns its
	// context, builds an FHeightField with new, and reads the custom-version registry under its RW lock —
	// so it is safe on a DDC completion worker as well as on the game thread.
	Chaos::FHeightFieldPtr DeserializeHeightfield(const FSharedBuffer& Blob)
	{
		if (Blob.IsNull())
		{
			return Chaos::FHeightFieldPtr();
		}

		FMemoryReaderView Reader(Blob.GetView(), /*bIsPersistent =*/ true);
		int32 BlobVersion = 0;
		Reader << BlobVersion;
		if (Reader.IsError() || BlobVersion != GCollisionBlobVersion)
		{
			return Chaos::FHeightFieldPtr();
		}

		Chaos::FHeightFieldPtr Heightfield;
		{
			Chaos::FChaosArchive ChaosAr(Reader);
			ChaosAr << Heightfield;
		}
		if (Reader.IsError() || !Heightfield.IsValid())
		{
			return Chaos::FHeightFieldPtr();
		}
		return Heightfield;
	}

	TRefCountPtr<FArcTerrainHeightfieldRef> MakeHeightfieldRef(Chaos::FHeightFieldPtr&& Heightfield,
		const FVector& BaseScale)
	{
		if (!Heightfield.IsValid())
		{
			return nullptr;
		}
		FArcTerrainHeightfieldRef* Ref = new FArcTerrainHeightfieldRef();
		Ref->Heightfield = MoveTemp(Heightfield);
		Ref->BaseScale = BaseScale;
		// Blobs are written pre-apply (base scale), but re-assert defensively — the physics state
		// derives its final scale from this base.
		Ref->Heightfield->SetScale(Chaos::FVec3(BaseScale));
		return TRefCountPtr<FArcTerrainHeightfieldRef>(Ref);
	}
#endif // WITH_EDITOR
}

namespace ArcTerrainCollision
{
	TRefCountPtr<FArcTerrainHeightfieldRef> BuildHeightfield(
		const TArray<float>& Heights, int32 Resolution, const FVector& BaseScale)
	{
		if (Heights.Num() != Resolution * Resolution || Resolution < 2)
		{
			return nullptr;
		}

		// Row-major match verified against Chaos::FHeightField::FData::GetPoint (HeightField.h):
		// sample index = Y * NumCols + X with X = column -> geometry-local X, which is the row-major
		// (Y * Resolution + X) layout the evaluator emits — no flip, no transpose.
		TArray<Chaos::FReal> ChaosHeights;
		ChaosHeights.SetNumUninitialized(Heights.Num());
		for (int32 Index = 0; Index < Heights.Num(); ++Index)
		{
			ChaosHeights[Index] = static_cast<Chaos::FReal>(Heights[Index]);
		}

		// Internally requantized to uint16 across the height range; empty material indices = all
		// samples use material 0 (the component binds the engine default physical material).
		Chaos::FHeightField* Heightfield = new Chaos::FHeightField(
			MoveTemp(ChaosHeights), TArray<uint8>(), Resolution, Resolution, Chaos::FVec3(BaseScale));

		FArcTerrainHeightfieldRef* Ref = new FArcTerrainHeightfieldRef();
		Ref->Heightfield = Chaos::FHeightFieldPtr(Heightfield);
		Ref->BaseScale = BaseScale;
		return TRefCountPtr<FArcTerrainHeightfieldRef>(Ref);
	}

#if WITH_EDITOR

	FIoHash ComputeCellCollisionKey(const FArcTerrainConfig& Config, float BaseHeightCm,
		FArcTerrainCellKey Key, const FTransform& TerrainTransform,
		TConstArrayView<UArcTerrainStampComponent*> SortedStamps)
	{
		using namespace ArcTerrainCollisionBuilderPrivate;

		FBlake3 Hash;

		// Bumped 2026-08-06: the rect edge mask evaluates a rounded-box signed distance and the key
		// folds the new CornerRadius, so every cached heightfield's key layout changed.
		// Bumped 2026-08-06: the height source can derive its zero point from the raster's border
		// ring, which changes evaluated heights and adds a hashed field.
		// Bumped 2026-08-06: the rect edge mask composes a smoothstep over its pow ramp, so every
		// cached heightfield's evaluated heights inside the falloff band changed.
		// Bumped 2026-08-19: a stamp may carry an optional per-texel mask raster multiplied into the
		// falloff mask, which changes evaluated heights and adds hashed fields.
		static const FGuid FormatVersionKey(TEXT("BC208C40-077E-488B-8963-F96C49DCF696"));
		HashPod(Hash, FormatVersionKey);

		HashPod(Hash, Key.X);
		HashPod(Hash, Key.Y);
		HashPod(Hash, Config.CellSize());
		HashPod(Hash, Config.QuadsPerCell());
		HashPod(Hash, BaseHeightCm);

		for (UArcTerrainStampComponent* Stamp : SortedStamps)
		{
			if (Stamp == nullptr)
			{
				continue;
			}

			// Class name as a STRING (never through an FArchive — FArchive<<FName is a no-op trap).
			HashString(Hash, Stamp->GetClass()->GetPathName());

			// Stamp placement relative to the terrain frame, raw NON-quantized doubles: the
			// composite consumes exactly this relative transform, and terrain-only moves must not
			// invalidate the cache. FVector = 3 doubles, FQuat = 4 doubles — no padding.
			const FTransform StampToTerrain =
				Stamp->GetComponentTransform().GetRelativeTransform(TerrainTransform);
			const FVector Translation = StampToTerrain.GetTranslation();
			const FQuat Rotation = StampToTerrain.GetRotation();
			const FVector Scale = StampToTerrain.GetScale3D();
			HashPod(Hash, Translation);
			HashPod(Hash, Rotation);
			HashPod(Hash, Scale);

			HashPod(Hash, Stamp->Priority);
			HashPod(Hash, static_cast<uint8>(Stamp->BlendMode));
			HashPod(Hash, Stamp->MinMaxSoftness);
			HashPod(Hash, Stamp->CoverageXY);

			FArcStampSourceDesc Desc;
			const uint8 bHasDescriptor = Stamp->GetSourceDescriptor(Desc) ? 1 : 0;
			HashPod(Hash, bHasDescriptor);
			if (!bHasDescriptor)
			{
				continue;
			}

			HashSourceFields(Hash, Desc.Kind, Desc.ContentId, Desc.SourceDims, Desc.SourceChannel,
				Desc.EncodingScale, Desc.ZeroInEncoding, Desc.bDeriveZeroFromBorder);

			HashSourceFields(Hash, Desc.MaskKind, Desc.MaskContentId, Desc.MaskSourceDims,
				Desc.MaskSourceChannel, /*EncodingScale*/ 1.0f, /*ZeroInEncoding*/ 0.0f,
				/*bDeriveZeroFromBorder*/ false);

			// Falloff mask fields scale BOTH the height pass and the carve passes.
			HashPod(Hash, Desc.EdgeFalloffBand);
			HashPod(Hash, Desc.EdgeSharpness);
			HashPod(Hash, static_cast<uint8>(Desc.bCircleMask ? 1 : 0));
			HashPod(Hash, Desc.CircleCenterOffset);
			HashPod(Hash, Desc.CircleRadius);
			HashPod(Hash, Desc.CircleFalloffBand);
			HashPod(Hash, Desc.CornerRadius);

			// Only CARVE layers reach the height tile; pure weight layers are deliberately excluded
			// so weight-only edits don't invalidate cached collision. The RESOLUTION GATE must be
			// IDENTICAL to CompositeCell's: a carve layer whose channel doesn't resolve against the
			// terrain's channel set is dropped from the composite entirely — hashing it anyway (or
			// omitting the resolved identity) would return stale cached collision after a ChannelSet
			// edit flips the layer's resolution.
			for (const TPair<FName, FArcStampLayerDesc>& LayerPair : Desc.Layers)
			{
				const FArcStampLayerDesc& Layer = LayerPair.Value;
				if (!Layer.bContributeToHeight || Layer.HeightAmplitude == 0.0f)
				{
					continue;
				}
				const int32 ChannelIndex =
					Config.ChannelSet ? Config.ChannelSet->FindChannelIndex(LayerPair.Key) : INDEX_NONE;
				if (ChannelIndex == INDEX_NONE)
				{
					continue; // dropped by the composite -> must not contribute to the key either
				}
				HashString(Hash, LayerPair.Key.ToString());
				HashSourceFields(Hash, Layer.Kind, Layer.ContentId, Layer.SourceDims, Layer.SourceChannel,
					Layer.EncodingScale, Layer.ZeroInEncoding, false);
				HashPod(Hash, Layer.HeightAmplitude);
			}
		}

		return Hash.Finalize();
	}

	FIoHash ComputeCollisionPatchCacheKey(const FIoHash& RecipeKey, int32 CollisionQuads,
		int32 PatchExtentM)
	{
		using namespace ArcTerrainCollisionBuilderPrivate;

		FBlake3 Hash;
		static const FGuid BlobFormatVersionKey(TEXT("3F17C0B8-5D42-4A9E-B36C-7E10D4A82F55"));
		HashPod(Hash, BlobFormatVersionKey);
		HashPod(Hash, RecipeKey);
		HashPod(Hash, CollisionQuads);
		HashPod(Hash, PatchExtentM);
		return Hash.Finalize();
	}

	TRefCountPtr<FArcTerrainHeightfieldRef> TryGetFromDDC(const FIoHash& Key, const FVector& BaseScale)
	{
		using namespace ArcTerrainCollisionBuilderPrivate;
		using namespace UE::DerivedData;

		FCacheGetRequest Request =
			{ UE::FSharedString(TEXT("ArcTerrainHeightfieldCollision")), MakeCacheKey(Key), ECachePolicy::Default };
		FRequestOwner RequestOwner(EPriority::Blocking);

		FSharedBuffer Buffer;
		GetCache().Get(MakeArrayView(&Request, 1), RequestOwner,
			[&Buffer](FCacheGetResponse&& Response)
			{
				if (Response.Status == EStatus::Ok)
				{
					Buffer = Response.Record.GetValue(HeightfieldValueId()).GetData().Decompress();
				}
			});
		RequestOwner.Wait();

		return MakeHeightfieldRef(DeserializeHeightfield(Buffer), BaseScale);
	}

	struct FAsyncHeightfieldGet
	{
		explicit FAsyncHeightfieldGet(UE::DerivedData::EPriority Priority)
			: Owner(Priority)
		{
		}

		UE::DerivedData::FRequestOwner Owner;
		Chaos::FHeightFieldPtr Heightfield;
		std::atomic<bool> bComplete { false };
	};

	FAsyncHeightfieldGetPtr BeginGetFromDDC(const FIoHash& Key,
		const TSharedPtr<FArcTerrainCollisionCompletionMailbox, ESPMode::ThreadSafe>& Mailbox,
		const FArcTerrainCollisionCompletionToken& Token)
	{
		using namespace ArcTerrainCollisionBuilderPrivate;
		using namespace UE::DerivedData;

		TSharedRef<FAsyncHeightfieldGet, ESPMode::ThreadSafe> State =
			MakeShared<FAsyncHeightfieldGet, ESPMode::ThreadSafe>(EPriority::Normal);
		State->Owner.KeepAlive();

		const FCacheGetRequest Request =
			{ UE::FSharedString(TEXT("ArcTerrainHeightfieldCollision")), MakeCacheKey(Key), ECachePolicy::Default };
		GetCache().Get(MakeArrayView(&Request, 1), State->Owner,
			[State, Mailbox, Token](FCacheGetResponse&& Response)
			{
				if (Response.Status == EStatus::Ok)
				{
					State->Heightfield = DeserializeHeightfield(
						Response.Record.GetValue(HeightfieldValueId()).GetData().Decompress());
				}
				State->bComplete.store(true, std::memory_order_release);
				if (!Mailbox.IsValid())
				{
					return;
				}
				const int32 Previous = Mailbox->Pending.fetch_add(1, std::memory_order_acq_rel);
				if (Previous >= FArcTerrainCollisionCompletionMailbox::Capacity)
				{
					Mailbox->Pending.fetch_sub(1, std::memory_order_relaxed);
					Mailbox->bOverflowWake.store(true, std::memory_order_release);
					return;
				}
				Mailbox->Queue.Enqueue(Token);
			});
		return State;
	}

	bool IsGetComplete(const FAsyncHeightfieldGetPtr& Request)
	{
		return Request.IsValid() && Request->bComplete.load(std::memory_order_acquire);
	}

	TRefCountPtr<FArcTerrainHeightfieldRef> ConsumeFromDDC(const FAsyncHeightfieldGetPtr& Request,
		const FVector& BaseScale)
	{
		using namespace ArcTerrainCollisionBuilderPrivate;

		if (!IsGetComplete(Request))
		{
			return nullptr;
		}
		return MakeHeightfieldRef(MoveTemp(Request->Heightfield), BaseScale);
	}

	void PutToDDC(const FIoHash& Key, const FArcTerrainHeightfieldRef& Ref)
	{
		using namespace ArcTerrainCollisionBuilderPrivate;
		using namespace UE::DerivedData;

		if (!Ref.Heightfield.IsValid())
		{
			return;
		}

		FLargeMemoryWriter Writer(0, /*bIsPersistent =*/ true);
		int32 BlobVersion = GCollisionBlobVersion;
		Writer << BlobVersion;
		{
			// Landscape WriteRuntimeData pattern (LandscapeCollision.cpp:1324-1352), single geometry.
			Chaos::FChaosArchive ChaosAr(Writer);
			Chaos::FHeightFieldPtr Heightfield = Ref.Heightfield;
			ChaosAr << Heightfield;
		}

		FValue Value = FValue::Compress(FSharedBuffer::MakeView(Writer.GetData(), Writer.TotalSize()));
		FCacheRecordBuilder RecordBuilder(MakeCacheKey(Key));
		RecordBuilder.AddValue(HeightfieldValueId(), Value);

		FRequestOwner RequestOwner(EPriority::Normal);
		const FCachePutRequest PutRequest =
			{ UE::FSharedString(TEXT("ArcTerrainHeightfieldCollision")), RecordBuilder.Build(), ECachePolicy::Default };
		GetCache().Put(MakeArrayView(&PutRequest, 1), RequestOwner);
		// Keep the async put alive past this scope (MeshPartition pattern).
		RequestOwner.KeepAlive();
	}

#endif // WITH_EDITOR
}
