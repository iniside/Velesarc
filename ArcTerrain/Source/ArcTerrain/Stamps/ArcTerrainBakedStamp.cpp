// Copyright Lukasz Baran. All Rights Reserved.

#include "Stamps/ArcTerrainBakedStamp.h"

#if WITH_EDITOR
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#endif

#define LOCTEXT_NAMESPACE "ArcTerrainBakedStamp"

UArcTerrainBakedStamp::FOnArcTerrainBakeRequested UArcTerrainBakedStamp::OnBakeRequested;

namespace ArcTerrainBakedStamp_Local
{
	/** Build a game-thread provider that fills a COPY of a baked field's buffer + dims. */
	TFunction<bool(TArray<float>&, FIntPoint&)> MakeProvider(const FArcTerrainBakedField& Field)
	{
		// Capture a copy: the descriptor may outlive the current stamp state, and the source cache
		// calls the provider on the game thread at most once per ContentId.
		FArcTerrainBakedField FieldCopy = Field;
		return [FieldCopy](TArray<float>& OutValues, FIntPoint& OutDims) -> bool
		{
			if (!FieldCopy.IsValid())
			{
				return false;
			}
			OutValues = FieldCopy.Values;
			OutDims = FieldCopy.Dims;
			return true;
		};
	}
}

void UArcTerrainBakedStamp::Bake()
{
	if (!OnBakeRequested.IsBound())
	{
#if WITH_EDITOR
		FNotificationInfo Info(LOCTEXT("BakeHandlerMissing",
			"Bake handler not available (ArcEditorTools module not loaded)"));
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
#endif // WITH_EDITOR
		return;
	}

	// Modify() BEFORE the handler mutates BakedField/BakedLayers so the transaction snapshots the
	// pre-bake state (undo restores the previous bake).
	Modify();

	// Handler fills BakedField/BakedLayers + fresh ContentIds; returns false leaving them untouched.
	if (OnBakeRequested.Execute(*this))
	{
		NotifyStampChanged();
	}
	else
	{
		// No notify on failure — the stored bake is unchanged, recompositing it would be stale noise.
#if WITH_EDITOR
		FNotificationInfo Info(LOCTEXT("BakeFailed",
			"Terrain Baked Stamp bake failed — see the log for details."));
		Info.ExpireDuration = 8.0f;
		TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
		if (Item.IsValid())
		{
			Item->SetCompletionState(SNotificationItem::CS_Fail);
		}
#endif // WITH_EDITOR
	}
}

bool UArcTerrainBakedStamp::GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const
{
	bool bProducedAny = false;

	// --- Height source ---
	if (BakedField.IsValid())
	{
		OutDesc.Kind = EArcStampSourceKind::BakedField;
		OutDesc.ContentId = BakedField.ContentId;
		OutDesc.SourceDims = BakedField.Dims;
		OutDesc.BakedProvider = ArcTerrainBakedStamp_Local::MakeProvider(BakedField);
		// Baked fields are single-channel; HeightScale decodes texels into stamp-local cm.
		OutDesc.SourceChannel = EArcTerrainSourceChannel::R;
		OutDesc.EncodingScale = HeightScale;
		OutDesc.ZeroInEncoding = 0.0f;
		bProducedAny = true;
	}

	// --- Weight layers ---
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
		const FArcTerrainBakedField& BakedLayer = BakedLayers.FindChecked(LayerName);

		// A layer whose name is unmapped, or maps to None, is dropped ENTIRELY — no weight pass
		// and no carve.
		const FName* MappedChannel = LayerChannelMap.Find(LayerName);
		const FName ChannelName = MappedChannel != nullptr ? *MappedChannel : NAME_None;
		if (ChannelName.IsNone())
		{
			continue;
		}

		FArcStampLayerDesc Layer;
		Layer.Kind = EArcStampSourceKind::BakedField;
		Layer.ContentId = BakedLayer.ContentId;
		Layer.SourceDims = BakedLayer.Dims;
		Layer.BakedProvider = ArcTerrainBakedStamp_Local::MakeProvider(BakedLayer);
		Layer.SourceChannel = EArcTerrainSourceChannel::R;
		Layer.EncodingScale = 1.0f;
		Layer.ZeroInEncoding = 0.0f;

		OutDesc.Layers.Emplace(ChannelName, MoveTemp(Layer));
		bProducedAny = true;
	}

	if (!bProducedAny)
	{
		return false;
	}

	OutDesc.EdgeFalloffBand = EdgeFalloffBand;
	OutDesc.EdgeSharpness = EdgeSharpness;
	OutDesc.bCircleMask = bCircleMask;
	OutDesc.CircleCenterOffset = CircleCenterOffset;
	OutDesc.CircleRadius = CircleRadius;
	OutDesc.CircleFalloffBand = CircleFalloffBand;
	return true;
}

#undef LOCTEXT_NAMESPACE
