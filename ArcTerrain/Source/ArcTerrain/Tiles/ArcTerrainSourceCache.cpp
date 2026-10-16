// Copyright Lukasz Baran. All Rights Reserved.

#include "Tiles/ArcTerrainSourceCache.h"
#include "ArcTerrainModule.h"
#include "ArcTerrainSettings.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformTime.h"
#include "Math/Float16.h"
#include "RHICommandList.h"
#include "RenderingThread.h"

namespace ArcTerrainSourceCachePrivate
{
#if WITH_EDITORONLY_DATA
	// Memory channel index of a logical RGBA channel for a given source format (BGRA swizzle).
	int32 InterleavedChannelIndex(ETextureSourceFormat Format, EArcTerrainSourceChannel Channel)
	{
		const int32 Logical = static_cast<int32>(Channel); // R=0 G=1 B=2 A=3
		if (Format == TSF_BGRA8)
		{
			const int32 BGRAMap[4] = {2, 1, 0, 3};
			return BGRAMap[Logical];
		}
		return Logical;
	}

	// Convert one mip of editor source data to a single-channel float raster. Values are the RAW
	// channel values (unorm formats normalized to [0,1]); NO sRGB conversion and NO encoding decode
	// here — the composite shader applies (texel - Zero) * Scale. Single-channel formats ignore the
	// requested channel (grayscale semantics). Returns false on unsupported formats (e.g. TSF_BGRE8).
	bool ConvertSourceToFloats(UTexture2D& Texture, EArcTerrainSourceChannel Channel,
		TArray<float>& OutValues, FIntPoint& OutDims)
	{
		FTextureSource& Source = Texture.Source;
		if (!Source.IsValid())
		{
			return false;
		}

		const ETextureSourceFormat Format = Source.GetFormat();
		if (!FArcTerrainSourceCache::IsDecodableSourceFormat(Format))
		{
			UE_LOGF(LogArcTerrain, Warning,"ArcTerrainSourceCache: unsupported texture source format %d on '%ls'",
				static_cast<int32>(Format), *Texture.GetName());
			return false;
		}

		TArray64<uint8> MipData;
		if (!Source.GetMipData(MipData, 0))
		{
			return false;
		}

		OutDims = FIntPoint(Source.GetSizeX(), Source.GetSizeY());
		const int64 NumTexels = static_cast<int64>(OutDims.X) * OutDims.Y;
		if (NumTexels <= 0)
		{
			return false;
		}
		OutValues.SetNumUninitialized(NumTexels);

		const int32 Ch = InterleavedChannelIndex(Format, Channel);

		switch (Format)
		{
		case TSF_G8:
		{
			const uint8* Data = MipData.GetData();
			for (int64 Index = 0; Index < NumTexels; ++Index)
			{
				OutValues[Index] = static_cast<float>(Data[Index]) / 255.0f;
			}
			return true;
		}
		case TSF_BGRA8:
		{
			const uint8* Data = MipData.GetData();
			for (int64 Index = 0; Index < NumTexels; ++Index)
			{
				OutValues[Index] = static_cast<float>(Data[Index * 4 + Ch]) / 255.0f;
			}
			return true;
		}
		case TSF_G16:
		{
			const uint16* Data = reinterpret_cast<const uint16*>(MipData.GetData());
			for (int64 Index = 0; Index < NumTexels; ++Index)
			{
				OutValues[Index] = static_cast<float>(Data[Index]) / 65535.0f;
			}
			return true;
		}
		case TSF_RGBA16:
		{
			const uint16* Data = reinterpret_cast<const uint16*>(MipData.GetData());
			for (int64 Index = 0; Index < NumTexels; ++Index)
			{
				OutValues[Index] = static_cast<float>(Data[Index * 4 + Ch]) / 65535.0f;
			}
			return true;
		}
		case TSF_RGBA16F:
		{
			const FFloat16* Data = reinterpret_cast<const FFloat16*>(MipData.GetData());
			for (int64 Index = 0; Index < NumTexels; ++Index)
			{
				OutValues[Index] = Data[Index * 4 + Ch].GetFloat();
			}
			return true;
		}
		case TSF_R16F:
		{
			const FFloat16* Data = reinterpret_cast<const FFloat16*>(MipData.GetData());
			for (int64 Index = 0; Index < NumTexels; ++Index)
			{
				OutValues[Index] = Data[Index].GetFloat();
			}
			return true;
		}
		case TSF_RGBA32F:
		{
			const float* Data = reinterpret_cast<const float*>(MipData.GetData());
			for (int64 Index = 0; Index < NumTexels; ++Index)
			{
				OutValues[Index] = Data[Index * 4 + Ch];
			}
			return true;
		}
		case TSF_R32F:
		{
			const float* Data = reinterpret_cast<const float*>(MipData.GetData());
			for (int64 Index = 0; Index < NumTexels; ++Index)
			{
				OutValues[Index] = Data[Index];
			}
			return true;
		}
		default:
			// IsDecodableSourceFormat admitted a format this switch does not handle.
			checkNoEntry();
			return false;
		}
	}

	// Mean of the one-texel border ring: row 0, row Dims.Y-1, column 0 and column Dims.X-1, corners
	// counted once. Degenerate dims (< 2 on either axis) make the ring the whole raster.
	float ComputeBorderMean(TConstArrayView<float> Values, FIntPoint Dims)
	{
		if (Values.Num() == 0 || static_cast<int64>(Dims.X) * Dims.Y != Values.Num())
		{
			return 0.0f;
		}

		double Sum = 0.0;
		int64 Count = 0;

		if (Dims.X < 2 || Dims.Y < 2)
		{
			for (float Value : Values)
			{
				Sum += Value;
			}
			Count = Values.Num();
		}
		else
		{
			const int32 LastRowStart = (Dims.Y - 1) * Dims.X;
			for (int32 X = 0; X < Dims.X; ++X)
			{
				Sum += Values[X];
				Sum += Values[LastRowStart + X];
			}
			for (int32 Y = 1; Y < Dims.Y - 1; ++Y)
			{
				const int32 RowStart = Y * Dims.X;
				Sum += Values[RowStart];
				Sum += Values[RowStart + Dims.X - 1];
			}
			Count = 2 * static_cast<int64>(Dims.X) + 2 * (static_cast<int64>(Dims.Y) - 2);
		}

		return static_cast<float>(Sum / static_cast<double>(Count));
	}
#endif // WITH_EDITORONLY_DATA
}

bool FArcTerrainSourceCache::IsDecodableSourceFormat(ETextureSourceFormat Format)
{
	switch (Format)
	{
	case TSF_G8:
	case TSF_BGRA8:
	case TSF_G16:
	case TSF_RGBA16:
	case TSF_RGBA16F:
	case TSF_R16F:
	case TSF_RGBA32F:
	case TSF_R32F:
		return true;
	default:
		return false;
	}
}

bool FArcTerrainSourceCache::IsSingleChannelSourceFormat(ETextureSourceFormat Format)
{
	return Format == TSF_G8 || Format == TSF_G16 || Format == TSF_R16F || Format == TSF_R32F;
}

bool FArcTerrainSourceCache::IsEightBitSourceFormat(ETextureSourceFormat Format)
{
	return Format == TSF_G8 || Format == TSF_BGRA8 || Format == TSF_BGRE8;
}

#if WITH_EDITORONLY_DATA
FArcTerrainSourceTexturePtr FArcTerrainSourceCache::GetOrCreateFromTexture(
	UTexture2D& Texture, EArcTerrainSourceChannel SourceChannel, const FGuid& ContentId)
{
	check(IsInGameThread());
	if (!ContentId.IsValid())
	{
		return nullptr;
	}

	const FCacheKey Key{ContentId, static_cast<uint8>(SourceChannel)};
	if (FCacheEntry* Existing = Entries.Find(Key))
	{
		Existing->LastUsedFrame = GFrameCounter;
		return Existing->Texture;
	}

	TArray<float> Values;
	FIntPoint Dims = FIntPoint::ZeroValue;
	if (!ArcTerrainSourceCachePrivate::ConvertSourceToFloats(Texture, SourceChannel, Values, Dims))
	{
		return nullptr;
	}
	const float BorderMean = ArcTerrainSourceCachePrivate::ComputeBorderMean(Values, Dims);
	return CreateAndUpload(Key, MoveTemp(Values), Dims, BorderMean);
}
#endif // WITH_EDITORONLY_DATA

FArcTerrainSourceTexturePtr FArcTerrainSourceCache::GetOrCreateFromProvider(
	const FGuid& ContentId, const TFunction<bool(TArray<float>&, FIntPoint&)>& Provider)
{
	check(IsInGameThread());
	if (!ContentId.IsValid() || !Provider)
	{
		return nullptr;
	}

	const FCacheKey Key{ContentId, 0};
	if (FCacheEntry* Existing = Entries.Find(Key))
	{
		Existing->LastUsedFrame = GFrameCounter;
		return Existing->Texture;
	}

	TArray<float> Values;
	FIntPoint Dims = FIntPoint::ZeroValue;
	if (!Provider(Values, Dims)
		|| Dims.X <= 0 || Dims.Y <= 0
		|| Values.Num() != Dims.X * Dims.Y)
	{
		return nullptr;
	}
	return CreateAndUpload(Key, MoveTemp(Values), Dims, 0.0f);
}

#if WITH_EDITORONLY_DATA
FArcTerrainSourceCpuFloatsPtr FArcTerrainSourceCache::GetOrCreateCpuFloatsFromTexture(
	UTexture2D& Texture, EArcTerrainSourceChannel SourceChannel, const FGuid& ContentId)
{
	check(IsInGameThread());
	if (!ContentId.IsValid())
	{
		return nullptr;
	}

	const FCacheKey Key{ContentId, static_cast<uint8>(SourceChannel)};
	if (FCpuCacheEntry* Existing = CpuEntries.Find(Key))
	{
		Existing->LastUsedFrame = GFrameCounter;
		if (bCpuStatsEnabled)
		{
			++CpuStatHits;
		}
		return Existing->Floats;
	}
	if (bCpuStatsEnabled)
	{
		++CpuStatMisses;
	}

	const double DecodeStartSeconds = bCpuStatsEnabled ? FPlatformTime::Seconds() : 0.0;
	TArray<float> Values;
	FIntPoint Dims = FIntPoint::ZeroValue;
	if (!ArcTerrainSourceCachePrivate::ConvertSourceToFloats(Texture, SourceChannel, Values, Dims))
	{
		return nullptr;
	}
	const float BorderMean = ArcTerrainSourceCachePrivate::ComputeBorderMean(Values, Dims);
	if (bCpuStatsEnabled)
	{
		FCpuStatsDecode& Decode = CpuStatDecodes.FindOrAdd(Key);
		Decode.Dims = Dims;
		Decode.Bytes = static_cast<int64>(Dims.X) * Dims.Y * sizeof(float);
		++Decode.Decodes;
		Decode.DecodeMs += (FPlatformTime::Seconds() - DecodeStartSeconds) * 1000.0;
	}
	return InsertCpuFloats(Key, MoveTemp(Values), Dims, BorderMean);
}
#endif // WITH_EDITORONLY_DATA

FArcTerrainSourceCpuFloatsPtr FArcTerrainSourceCache::GetOrCreateCpuFloatsFromProvider(
	const FGuid& ContentId, const TFunction<bool(TArray<float>&, FIntPoint&)>& Provider)
{
	check(IsInGameThread());
	if (!ContentId.IsValid() || !Provider)
	{
		return nullptr;
	}

	const FCacheKey Key{ContentId, 0};
	if (FCpuCacheEntry* Existing = CpuEntries.Find(Key))
	{
		Existing->LastUsedFrame = GFrameCounter;
		if (bCpuStatsEnabled)
		{
			++CpuStatHits;
		}
		return Existing->Floats;
	}
	if (bCpuStatsEnabled)
	{
		++CpuStatMisses;
	}

	const double DecodeStartSeconds = bCpuStatsEnabled ? FPlatformTime::Seconds() : 0.0;
	TArray<float> Values;
	FIntPoint Dims = FIntPoint::ZeroValue;
	if (!Provider(Values, Dims)
		|| Dims.X <= 0 || Dims.Y <= 0
		|| Values.Num() != Dims.X * Dims.Y)
	{
		return nullptr;
	}
	if (bCpuStatsEnabled)
	{
		FCpuStatsDecode& Decode = CpuStatDecodes.FindOrAdd(Key);
		Decode.Dims = Dims;
		Decode.Bytes = static_cast<int64>(Dims.X) * Dims.Y * sizeof(float);
		++Decode.Decodes;
		Decode.DecodeMs += (FPlatformTime::Seconds() - DecodeStartSeconds) * 1000.0;
	}
	return InsertCpuFloats(Key, MoveTemp(Values), Dims, 0.0f);
}

FArcTerrainSourceCpuFloatsPtr FArcTerrainSourceCache::InsertCpuFloats(
	const FCacheKey& Key, TArray<float>&& Values, FIntPoint Dims, float BorderMean)
{
	TSharedPtr<FArcTerrainSourceCpuFloats, ESPMode::ThreadSafe> Entry =
		MakeShared<FArcTerrainSourceCpuFloats, ESPMode::ThreadSafe>();
	Entry->Values = MoveTemp(Values);
	Entry->Dims = Dims;
	Entry->SizeBytes = static_cast<int64>(Dims.X) * Dims.Y * sizeof(float);
	Entry->BorderMean = BorderMean;

	FCpuCacheEntry& Inserted = CpuEntries.Add(Key);
	Inserted.Floats = Entry;
	Inserted.LastUsedFrame = GFrameCounter;
	TotalCpuBytes += Entry->SizeBytes;

	EnforceCpuBudget();
	return Entry;
}

FArcTerrainSourceTexturePtr FArcTerrainSourceCache::CreateAndUpload(
	const FCacheKey& Key, TArray<float>&& Values, FIntPoint Dims, float BorderMean)
{
	FArcTerrainSourceTexturePtr Entry = MakeShared<FArcTerrainSourceTexture, ESPMode::ThreadSafe>();
	Entry->Dims = Dims;
	Entry->SizeBytes = static_cast<int64>(Dims.X) * Dims.Y * sizeof(float);
	Entry->BorderMean = BorderMean;

	// Full float R32 upload (MVP; R16F packing deferred). The command is enqueued before any
	// composite command referencing this entry, so FIFO render-command order guarantees TextureRHI
	// is set by consume time.
	ENQUEUE_RENDER_COMMAND(ArcTerrainUploadStampSource)(
		[Entry, UploadValues = MoveTemp(Values), Dims](FRHICommandListImmediate& RHICmdList)
		{
			const FRHITextureCreateDesc Desc =
				FRHITextureCreateDesc::Create2D(TEXT("ArcTerrainStampSource"), Dims, PF_R32_FLOAT)
					.SetFlags(ETextureCreateFlags::ShaderResource)
					.SetInitialState(ERHIAccess::SRVMask);
			FTextureRHIRef Texture = RHICmdList.CreateTexture(Desc);

			const FUpdateTextureRegion2D Region(0, 0, 0, 0, Dims.X, Dims.Y);
			RHICmdList.UpdateTexture2D(Texture, /*MipIndex = */0, Region,
				/*SourcePitch = */static_cast<uint32>(Dims.X * sizeof(float)),
				reinterpret_cast<const uint8*>(UploadValues.GetData()));

			Entry->TextureRHI = Texture;
		});

	FCacheEntry& Inserted = Entries.Add(Key);
	Inserted.Texture = Entry;
	Inserted.LastUsedFrame = GFrameCounter;
	TotalBytes += Entry->SizeBytes;

	EnforceBudget();
	return Entry;
}

void FArcTerrainSourceCache::EnforceBudget()
{
	const int64 BudgetBytes =
		static_cast<int64>(GetDefault<UArcTerrainSettings>()->SourceCacheMB) * 1024 * 1024;

	while (TotalBytes > BudgetBytes)
	{
		// Least-recently-used entry not held by an in-flight composite job (shared refcount == 1
		// means the cache is the only holder).
		const FCacheKey* OldestKey = nullptr;
		uint64 OldestFrame = MAX_uint64;
		for (const TPair<FCacheKey, FCacheEntry>& Pair : Entries)
		{
			if (Pair.Value.Texture.GetSharedReferenceCount() == 1 && Pair.Value.LastUsedFrame < OldestFrame)
			{
				OldestFrame = Pair.Value.LastUsedFrame;
				OldestKey = &Pair.Key;
			}
		}
		if (OldestKey == nullptr)
		{
			break; // everything is in flight — over budget until jobs drain
		}

		const FCacheKey KeyToRemove = *OldestKey;
		FCacheEntry Removed;
		Entries.RemoveAndCopyValue(KeyToRemove, Removed);
		TotalBytes -= Removed.Texture->SizeBytes;
		// Dropping the last shared ref releases the FTextureRHIRef; RHI resource deletion is
		// deferred/thread-safe from the game thread.
	}
}

void FArcTerrainSourceCache::EnforceCpuBudget()
{
	// Same MiB budget value as the GPU LRU, accounted SEPARATELY (VRAM vs RAM). Same in-flight
	// protection: an entry whose shared refcount shows an outside holder (a gathered payload riding
	// an async build) is never evicted.
	const int64 BudgetBytes =
		static_cast<int64>(GetDefault<UArcTerrainSettings>()->SourceCacheMB) * 1024 * 1024;

	while (TotalCpuBytes > BudgetBytes)
	{
		const FCacheKey* OldestKey = nullptr;
		uint64 OldestFrame = MAX_uint64;
		for (const TPair<FCacheKey, FCpuCacheEntry>& Pair : CpuEntries)
		{
			if (PinnedCpuKeys.Contains(Pair.Key))
			{
				continue;
			}
			if (Pair.Value.Floats.GetSharedReferenceCount() == 1 && Pair.Value.LastUsedFrame < OldestFrame)
			{
				OldestFrame = Pair.Value.LastUsedFrame;
				OldestKey = &Pair.Key;
			}
		}
		if (OldestKey == nullptr)
		{
			break; // everything is held by in-flight payloads — over budget until they drain
		}

		const FCacheKey KeyToRemove = *OldestKey;
		FCpuCacheEntry Removed;
		CpuEntries.RemoveAndCopyValue(KeyToRemove, Removed);
		TotalCpuBytes -= Removed.Floats->SizeBytes;
		if (bCpuStatsEnabled)
		{
			++CpuStatEvictions;
		}
	}
}

bool FArcTerrainSourceCache::PinCpuFloats(const FGuid& ContentId, EArcTerrainSourceChannel SourceChannel)
{
	check(IsInGameThread());

	const FCacheKey Key{ContentId, static_cast<uint8>(SourceChannel)};
	const FCpuCacheEntry* Entry = CpuEntries.Find(Key);
	if (Entry == nullptr || !Entry->Floats.IsValid())
	{
		return false;
	}
	if (PinnedCpuKeys.Contains(Key))
	{
		return true;
	}

	const UArcTerrainSettings* Settings = GetDefault<UArcTerrainSettings>();
	const int64 PinBudgetBytes =
		static_cast<int64>(FMath::Min(Settings->SourceCachePinMB, Settings->SourceCacheMB)) * 1024 * 1024;
	const int64 EntryBytes = Entry->Floats->SizeBytes;
	if (PinnedCpuBytes + EntryBytes > PinBudgetBytes)
	{
		if (bCpuStatsEnabled)
		{
			++CpuStatPinsRefused;
		}
		if (!bPinBudgetWarned)
		{
			bPinBudgetWarned = true;
			UE_LOGF(LogArcTerrain, Log,"ArcTerrainSourcePin refused pinned_bytes=%lld entry_bytes=%lld budget_bytes=%lld",
				static_cast<long long>(PinnedCpuBytes), static_cast<long long>(EntryBytes),
				static_cast<long long>(PinBudgetBytes));
		}
		return false;
	}

	PinnedCpuKeys.Add(Key);
	PinnedCpuBytes += EntryBytes;
	return true;
}

void FArcTerrainSourceCache::UnpinAllCpuFloats()
{
	check(IsInGameThread());

	PinnedCpuKeys.Empty();
	PinnedCpuBytes = 0;
	bPinBudgetWarned = false;
	EnforceCpuBudget();
}

void FArcTerrainSourceCache::ShedPinnedCpuFloatsToBudget()
{
	check(IsInGameThread());

	const UArcTerrainSettings* Settings = GetDefault<UArcTerrainSettings>();
	const int64 PinBudgetBytes =
		static_cast<int64>(FMath::Min(Settings->SourceCachePinMB, Settings->SourceCacheMB)) * 1024 * 1024;

	while (PinnedCpuBytes > PinBudgetBytes && PinnedCpuKeys.Num() > 0)
	{
		FCacheKey OldestKey;
		uint64 OldestFrame = 0;
		int64 OldestBytes = 0;
		bool bFound = false;
		for (const FCacheKey& Key : PinnedCpuKeys)
		{
			const FCpuCacheEntry* Entry = CpuEntries.Find(Key);
			const uint64 Frame = (Entry != nullptr) ? Entry->LastUsedFrame : 0;
			if (!bFound || Frame < OldestFrame)
			{
				bFound = true;
				OldestFrame = Frame;
				OldestBytes = (Entry != nullptr && Entry->Floats.IsValid()) ? Entry->Floats->SizeBytes : 0;
				OldestKey = Key;
			}
		}

		PinnedCpuKeys.Remove(OldestKey);
		PinnedCpuBytes = FMath::Max<int64>(PinnedCpuBytes - OldestBytes, 0);
	}

	if (PinnedCpuKeys.Num() == 0)
	{
		PinnedCpuBytes = 0;
		bPinBudgetWarned = false;
	}
}

void FArcTerrainSourceCache::SetCpuStatsEnabled(bool bEnabled)
{
	bCpuStatsEnabled = bEnabled;
	CpuStatHits = 0;
	CpuStatMisses = 0;
	CpuStatEvictions = 0;
	CpuStatPinsRefused = 0;
	CpuStatDecodes.Empty();
}

FString FArcTerrainSourceCache::DescribeCpuStats() const
{
	FString Sources;
	for (const TPair<FCacheKey, FCpuStatsDecode>& Pair : CpuStatDecodes)
	{
		Sources += FString::Printf(
			TEXT("{src=%s ch=%u dims=%dx%d bytes=%lld decodes=%d decode_ms=%.3f}"),
			*Pair.Key.ContentId.ToString(EGuidFormats::Digits),
			static_cast<uint32>(Pair.Key.Channel),
			Pair.Value.Dims.X, Pair.Value.Dims.Y, static_cast<long long>(Pair.Value.Bytes),
			Pair.Value.Decodes, Pair.Value.DecodeMs);
	}

	return FString::Printf(
		TEXT("hits=%d misses=%d evictions=%d pins_refused=%d pinned_bytes=%lld cpu_bytes=%lld ")
		TEXT("decoded=%d sources=[%s]"),
		CpuStatHits, CpuStatMisses, CpuStatEvictions, CpuStatPinsRefused,
		static_cast<long long>(PinnedCpuBytes), static_cast<long long>(TotalCpuBytes),
		CpuStatDecodes.Num(), *Sources);
}

void FArcTerrainSourceCache::Empty()
{
	Entries.Empty();
	TotalBytes = 0;
	CpuEntries.Empty();
	TotalCpuBytes = 0;
	PinnedCpuKeys.Empty();
	PinnedCpuBytes = 0;
	bPinBudgetWarned = false;
}

void FArcTerrainSourceCache::ReEnforceBudgets()
{
	EnforceBudget();
	EnforceCpuBudget();
}
