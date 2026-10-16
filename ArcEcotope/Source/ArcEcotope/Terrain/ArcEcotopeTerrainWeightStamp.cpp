// Copyright Lukasz Baran. All Rights Reserved.

#include "Terrain/ArcEcotopeTerrainWeightStamp.h"

#include "ArcEcotopeModule.h"
#include "Hash/Blake3.h"

namespace ArcEcotopeTerrainWeightStamp_Local
{
	TFunction<bool(TArray<float>&, FIntPoint&)> MakeProvider(TSharedRef<const FArcTerrainBakedField> Field)
	{
		return [Field](TArray<float>& OutValues, FIntPoint& OutDims) -> bool
		{
			if (!Field->IsValid())
			{
				return false;
			}
			OutValues = Field->Values;
			OutDims = Field->Dims;
			return true;
		};
	}
}

namespace ArcEcotopeTerrainWeight
{
	FGuid ComputeFieldContentId(const TConstArrayView<float> Values, const FIntPoint& Dims)
	{
		// Salts the digest so a future change to what a texel means still invalidates identical bytes.
		static const FGuid FormatVersionKey(TEXT("3E7C41A9-58B2-4F06-9D13-C82A6B5E7F04"));

		FBlake3 Hash;
		Hash.Update(&FormatVersionKey, sizeof(FormatVersionKey));
		const int32 DimsX = Dims.X;
		const int32 DimsY = Dims.Y;
		const int32 Count = Values.Num();
		Hash.Update(&DimsX, sizeof(DimsX));
		Hash.Update(&DimsY, sizeof(DimsY));
		Hash.Update(&Count, sizeof(Count));
		if (Count > 0)
		{
			Hash.Update(Values.GetData(), static_cast<uint64>(Count) * sizeof(float));
		}

		const FBlake3Hash Digest = Hash.Finalize();
		uint32 Words[4] = {0, 0, 0, 0};
		FMemory::Memcpy(Words, Digest.GetBytes(), sizeof(Words));
		Words[0] |= 1u;
		return FGuid(Words[0], Words[1], Words[2], Words[3]);
	}
}

void UArcEcotopeTerrainWeightStamp::RefreshProviderFields()
{
	ProviderFields.Reset();
	ProviderFields.Reserve(BakedLayers.Num());

	TMap<FGuid, TSharedPtr<const FArcTerrainBakedField>> FieldsByContentId;
	FieldsByContentId.Reserve(BakedLayers.Num());
	for (const TPair<FName, FArcTerrainBakedField>& LayerPair : BakedLayers)
	{
		if (!LayerPair.Value.ContentId.IsValid())
		{
			ProviderFields.Add(LayerPair.Key, MakeShared<FArcTerrainBakedField>(LayerPair.Value));
			continue;
		}

		TSharedPtr<const FArcTerrainBakedField>& Shared = FieldsByContentId.FindOrAdd(LayerPair.Value.ContentId);
		if (!Shared.IsValid())
		{
			Shared = MakeShared<FArcTerrainBakedField>(LayerPair.Value);
		}
		ProviderFields.Add(LayerPair.Key, Shared);
	}

	MaskProviderField.Reset();
	if (UnionMask.IsValid())
	{
		MaskProviderField = MakeShared<FArcTerrainBakedField>(UnionMask);
	}

	SuppressionProviderField.Reset();
	if (!SuppressedChannels.IsEmpty() && UnionMask.IsValid())
	{
		TSharedRef<FArcTerrainBakedField> ZeroField = MakeShared<FArcTerrainBakedField>();
		ZeroField->Dims = UnionMask.Dims;
		ZeroField->Values.Init(0.0f, UnionMask.Values.Num());
		ZeroField->ContentId = ArcEcotopeTerrainWeight::ComputeFieldContentId(ZeroField->Values, ZeroField->Dims);
		SuppressionProviderField = ZeroField;
	}
}

void UArcEcotopeTerrainWeightStamp::ClearBakedLayers()
{
	if (BakedLayers.IsEmpty() && LayerChannelMap.IsEmpty() && SuppressedChannels.IsEmpty()
		&& UnionMask.Values.IsEmpty() && !UnionMask.ContentId.IsValid())
	{
		return;
	}

	Modify();
	BakedLayers.Reset();
	LayerChannelMap.Reset();
	SuppressedChannels.Reset();
	UnionMask.Reset();
	RefreshProviderFields();
	NotifyStampChanged();
}

bool UArcEcotopeTerrainWeightStamp::GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const
{
	bool bProducedAny = false;

	// TMap iteration order is insertion/hash-history dependent. Composite pass order is observable
	// when multiple baked sources map to the same terrain channel, so canonicalize by the ORIGINAL
	// baked-layer name (never by mapped channel or ContentId).
	TArray<FName> SortedLayerNames;
	SortedLayerNames.Reserve(BakedLayers.Num());
	for (const TPair<FName, FArcTerrainBakedField>& LayerPair : BakedLayers)
	{
		if (LayerPair.Value.IsValid())
		{
			SortedLayerNames.Add(LayerPair.Key);
		}
	}
	SortedLayerNames.Sort(FNameLexicalLess());

	for (const FName LayerName : SortedLayerNames)
	{
		const TSharedPtr<const FArcTerrainBakedField>* ProviderField = ProviderFields.Find(LayerName);
		if (ProviderField == nullptr || !ProviderField->IsValid() || !(*ProviderField)->IsValid())
		{
			UE_LOGF(LogArcEcotope, Warning,"%ls: baked layer '%ls' has no provider mirror — call RefreshProviderFields() after writing BakedLayers. Skipping the whole stamp.",
				*GetPathName(), *LayerName.ToString());
			return false;
		}

		// A layer whose name is unmapped, or maps to None, is dropped ENTIRELY — no weight pass
		// and no carve.
		const FName* MappedChannel = LayerChannelMap.Find(LayerName);
		const FName ChannelName = MappedChannel != nullptr ? *MappedChannel : NAME_None;
		UE_CLOGF(ChannelName.IsNone(), LogArcEcotope, Warning,"%ls: baked layer '%ls' has a %ls channel mapping and is dropped entirely.",
			*GetPathName(), *LayerName.ToString(),
			MappedChannel == nullptr ? TEXT("missing") : TEXT("None"));
		if (ChannelName.IsNone())
		{
			continue;
		}

		FArcStampLayerDesc Layer;
		Layer.Kind = EArcStampSourceKind::BakedField;
		Layer.ContentId = (*ProviderField)->ContentId;
		Layer.SourceDims = (*ProviderField)->Dims;
		Layer.BakedProvider = ArcEcotopeTerrainWeightStamp_Local::MakeProvider(ProviderField->ToSharedRef());
		Layer.SourceChannel = EArcTerrainSourceChannel::R;
		Layer.EncodingScale = 1.0f;
		Layer.ZeroInEncoding = 0.0f;

		OutDesc.Layers.Emplace(ChannelName, MoveTemp(Layer));
		bProducedAny = true;
	}

	if (!SuppressedChannels.IsEmpty())
	{
		if (!SuppressionProviderField.IsValid() || !SuppressionProviderField->IsValid())
		{
			UE_LOGF(LogArcEcotope, Warning,"%ls: suppressed channels have no zero-field mirror — call RefreshProviderFields() after writing SuppressedChannels. Skipping the whole stamp.",
				*GetPathName());
			return false;
		}

		TArray<FName> SortedSuppressedChannels(SuppressedChannels);
		SortedSuppressedChannels.Sort(FNameLexicalLess());
		for (const FName ChannelName : SortedSuppressedChannels)
		{
			if (ChannelName.IsNone())
			{
				continue;
			}

			FArcStampLayerDesc Layer;
			Layer.Kind = EArcStampSourceKind::BakedField;
			Layer.ContentId = SuppressionProviderField->ContentId;
			Layer.SourceDims = SuppressionProviderField->Dims;
			Layer.BakedProvider =
				ArcEcotopeTerrainWeightStamp_Local::MakeProvider(SuppressionProviderField.ToSharedRef());
			Layer.SourceChannel = EArcTerrainSourceChannel::R;
			Layer.EncodingScale = 1.0f;
			Layer.ZeroInEncoding = 0.0f;

			OutDesc.Layers.Emplace(ChannelName, MoveTemp(Layer));
			bProducedAny = true;
		}
	}

	if (!bProducedAny)
	{
		return false;
	}

	if (!MaskProviderField.IsValid() || !MaskProviderField->IsValid())
	{
		UE_LOGF(LogArcEcotope, Warning,"%ls: baked layers carry no union mask — call RefreshProviderFields() after writing UnionMask. Skipping the whole stamp.",
			*GetPathName());
		return false;
	}

	OutDesc.MaskKind = EArcStampSourceKind::BakedField;
	OutDesc.MaskContentId = MaskProviderField->ContentId;
	OutDesc.MaskSourceDims = MaskProviderField->Dims;
	OutDesc.MaskSourceChannel = EArcTerrainSourceChannel::R;
	OutDesc.MaskBakedProvider =
		ArcEcotopeTerrainWeightStamp_Local::MakeProvider(MaskProviderField.ToSharedRef());

	return true;
}

void UArcEcotopeTerrainWeightStamp::PostLoad()
{
	Super::PostLoad();
	RefreshProviderFields();
}

void UArcEcotopeTerrainWeightStamp::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	RefreshProviderFields();
}

#if WITH_EDITOR
void UArcEcotopeTerrainWeightStamp::PostEditImport()
{
	Super::PostEditImport();
	RefreshProviderFields();
}

void UArcEcotopeTerrainWeightStamp::PostEditUndo()
{
	RefreshProviderFields();
	Super::PostEditUndo();
}
#endif // WITH_EDITOR
