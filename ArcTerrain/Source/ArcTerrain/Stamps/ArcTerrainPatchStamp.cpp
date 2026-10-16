// Copyright Lukasz Baran. All Rights Reserved.

#include "Stamps/ArcTerrainPatchStamp.h"
#include "Engine/Texture2D.h"

namespace ArcTerrainPatchStamp_Local
{
#if WITH_EDITOR
	FIntPoint GetSourceDims(const UTexture2D* Texture)
	{
		return Texture != nullptr && Texture->Source.IsValid()
			? FIntPoint(Texture->Source.GetSizeX(), Texture->Source.GetSizeY())
			: FIntPoint::ZeroValue;
	}

	/** Fill the shared falloff parameters onto a stamp source descriptor. */
	void FillFalloff(FArcStampSourceDesc& OutDesc, const UArcTerrainPatchStamp& Stamp)
	{
		OutDesc.EdgeFalloffBand = Stamp.EdgeFalloffBand;
		OutDesc.EdgeSharpness = Stamp.EdgeSharpness;
		OutDesc.bCircleMask = Stamp.bCircleMask;
		OutDesc.CircleCenterOffset = Stamp.CircleCenterOffset;
		OutDesc.CircleRadius = Stamp.CircleRadius;
		OutDesc.CircleFalloffBand = Stamp.CircleFalloffBand;
	}
#endif // WITH_EDITOR
}

bool UArcTerrainPatchStamp::GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const
{
#if WITH_EDITOR
	bool bProducedAny = false;

	// --- Height source ---
	UTexture2D* HeightTex = nullptr;
	EArcTerrainSourceChannel HeightChannel = EArcTerrainSourceChannel::R;
	float EncodingScale = 1000.0f;
	float ZeroInEncoding = 0.5f;

	if (SourceMode == EArcTerrainPatchSourceMode::DataAsset)
	{
		if (PatchAsset != nullptr)
		{
			HeightTex = PatchAsset->HeightTexture;
			HeightChannel = PatchAsset->HeightSourceChannel;
			EncodingScale = bOverrideHeightEncoding ? OverrideHeightEncodingScale : PatchAsset->HeightEncodingScale;
			ZeroInEncoding = bOverrideHeightEncoding ? OverrideHeightZeroInEncoding : PatchAsset->HeightZeroInEncoding;
		}
	}
	else
	{
		HeightTex = HeightTexture;
		HeightChannel = HeightSourceChannel;
		EncodingScale = HeightEncodingScale;
		ZeroInEncoding = HeightZeroInEncoding;
	}

	if (HeightTex != nullptr)
	{
		OutDesc.Kind = EArcStampSourceKind::Texture;
		OutDesc.ContentId = HeightTex->Source.GetId();
		OutDesc.Texture = HeightTex;
		OutDesc.SourceDims = ArcTerrainPatchStamp_Local::GetSourceDims(HeightTex);
		OutDesc.SourceChannel = HeightChannel;
		OutDesc.EncodingScale = EncodingScale;
		OutDesc.ZeroInEncoding = ZeroInEncoding;
		bProducedAny = true;
	}

	// --- Weight layers ---
	if (SourceMode == EArcTerrainPatchSourceMode::DataAsset)
	{
		if (PatchAsset != nullptr)
		{
			for (const FArcTerrainPatchWeightLayer& WeightLayer : PatchAsset->WeightLayers)
			{
				if (WeightLayer.Texture == nullptr)
				{
					continue;
				}

				// A layer whose LayerName is unmapped, or maps to None, is dropped ENTIRELY — no
				// weight pass and no carve.
				const FName* MappedChannel = LayerChannelMap.Find(WeightLayer.LayerName);
				const FName ChannelName = MappedChannel != nullptr ? *MappedChannel : NAME_None;
				if (ChannelName.IsNone())
				{
					continue;
				}

				FArcStampLayerDesc Layer;
				Layer.Kind = EArcStampSourceKind::Texture;
				Layer.ContentId = WeightLayer.Texture->Source.GetId();
				Layer.Texture = WeightLayer.Texture;
				Layer.SourceDims = ArcTerrainPatchStamp_Local::GetSourceDims(WeightLayer.Texture);
				Layer.SourceChannel = WeightLayer.SourceChannel;
				Layer.EncodingScale = WeightLayer.EncodingScale;
				Layer.ZeroInEncoding = WeightLayer.ZeroInEncoding;
				Layer.bContributeToHeight = WeightLayer.bContributeToHeight;
				Layer.HeightAmplitude = WeightLayer.HeightAmplitude;

				OutDesc.Layers.Emplace(ChannelName, MoveTemp(Layer));
				bProducedAny = true;
			}
		}
	}
	else
	{
		for (const FArcTerrainPatchDirectWeight& Weight : DirectWeights)
		{
			// MP parity: an entry without a texture OR without a target channel is dropped
			// entirely — no weight pass and no carve.
			if (Weight.Texture == nullptr || Weight.TargetChannel.IsNone())
			{
				continue;
			}

			FArcStampLayerDesc Layer;
			Layer.Kind = EArcStampSourceKind::Texture;
			Layer.ContentId = Weight.Texture->Source.GetId();
			Layer.Texture = Weight.Texture;
			Layer.SourceDims = ArcTerrainPatchStamp_Local::GetSourceDims(Weight.Texture);
			Layer.SourceChannel = Weight.SourceChannel;
			Layer.EncodingScale = Weight.EncodingScale;
			Layer.ZeroInEncoding = Weight.ZeroInEncoding;
			Layer.bContributeToHeight = Weight.bContributeToHeight;
			Layer.HeightAmplitude = Weight.HeightAmplitude;

			OutDesc.Layers.Emplace(Weight.TargetChannel, MoveTemp(Layer));
			bProducedAny = true;
		}
	}

	if (!bProducedAny)
	{
		return false;
	}

	ArcTerrainPatchStamp_Local::FillFalloff(OutDesc, *this);
	return true;
#else // WITH_EDITOR
	// Source uploads are FTextureSource (editor-only) — no compositing outside the editor.
	return false;
#endif // WITH_EDITOR
}

#if WITH_EDITOR
void UArcTerrainPatchStamp::NotifyPatchAssetChanged(UArcTerrainPatchAsset& ChangedAsset)
{
	if (!IsValid(this)
		|| HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed)
		|| IsTemplate()
		|| !IsRegistered()
		|| SourceMode != EArcTerrainPatchSourceMode::DataAsset
		|| PatchAsset != &ChangedAsset)
	{
		return;
	}

	NotifyStampChanged();
}

void UArcTerrainPatchStamp::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName ChangedName = (PropertyChangedEvent.Property != nullptr)
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;

	if (ChangedName == GET_MEMBER_NAME_CHECKED(UArcTerrainPatchStamp, PatchAsset) && PatchAsset != nullptr)
	{
		// Merge-populate LayerChannelMap: add a key for each weight layer (never clobber existing values),
		// prefilling the value when DefaultChannelName (or, failing that, LayerName) is a declared channel.
		// Modify() BEFORE the map mutation so undo captures the pre-populate state.
		Modify();

		const TArray<FName> Channels = GetTerrainChannels();
		for (const FArcTerrainPatchWeightLayer& WeightLayer : PatchAsset->WeightLayers)
		{
			if (WeightLayer.LayerName.IsNone() || LayerChannelMap.Contains(WeightLayer.LayerName))
			{
				continue;
			}

			FName Value = NAME_None;
			if (!WeightLayer.DefaultChannelName.IsNone() && Channels.Contains(WeightLayer.DefaultChannelName))
			{
				Value = WeightLayer.DefaultChannelName;
			}
			else if (Channels.Contains(WeightLayer.LayerName))
			{
				Value = WeightLayer.LayerName;
			}
			LayerChannelMap.Add(WeightLayer.LayerName, Value);
		}
	}

	// Base routes invalidation (NotifyStampChanged) and re-home on AffectedTerrain change.
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR
