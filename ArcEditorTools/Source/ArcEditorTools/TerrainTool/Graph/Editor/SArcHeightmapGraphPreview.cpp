// Copyright Lukasz Baran. All Rights Reserved.

#include "TerrainTool/Graph/Editor/SArcHeightmapGraphPreview.h"

#include "Engine/Texture2D.h"
#include "Slate/DeferredCleanupSlateBrush.h"
#include "TextureResource.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SArcHeightmapGraphPreview"

void SArcHeightmapGraphPreview::Construct(const FArguments& InArgs)
{
	// Seed the option list so the combo is never backed by an empty array before SetResult fires.
	// The combo is hidden until a result arrives (Visibility_Lambda below).
	PreviewOptions.Add(MakeShared<FName>(NAME_None));
	SelectedOption = PreviewOptions[0];

	ChildSlot
	[
		SNew(SVerticalBox)

		// ---- Layer selector ----
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 4.0f, 4.0f, 2.0f)
		[
			SAssignNew(SelectorCombo, SComboBox<TSharedPtr<FName>>)
			.OptionsSource(&PreviewOptions)
			.Visibility_Lambda([this]() -> EVisibility
			{
				// Show only once at least one result has been pushed (PreviewBrush becomes valid
				// after the first SetField call from SetResult/RenderSelected).
				return (PreviewBrush.IsValid() || bEvaluating) ? EVisibility::Visible : EVisibility::Collapsed;
			})
			.OnGenerateWidget_Lambda([](TSharedPtr<FName> Item) -> TSharedRef<SWidget>
			{
				const FText Label = (Item.IsValid() && !Item->IsNone())
					? FText::FromName(*Item)
					: LOCTEXT("LayerHeight", "Height");
				return SNew(STextBlock).Text(Label);
			})
			.OnSelectionChanged_Lambda([this](TSharedPtr<FName> NewSelection, ESelectInfo::Type /*SelectInfo*/)
			{
				if (NewSelection.IsValid())
				{
					SelectedOption = NewSelection;
					RenderSelected();
				}
			})
			[
				// Content widget — the currently-selected item's label shown in the collapsed combo.
				SNew(STextBlock)
				.Text_Lambda([this]() -> FText
				{
					if (SelectedOption.IsValid() && !SelectedOption->IsNone())
					{
						return FText::FromName(*SelectedOption);
					}
					return LOCTEXT("LayerHeight", "Height");
				})
			]
		]

		// ---- Preview image area ----
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SOverlay)
			// Output image — ScaleToFit keeps the field's true W:H aspect ratio inside the tab
			// (the tab is a narrow column, so HAlign/VAlign_Fill on a raw SImage stretched it).
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image_Lambda([this]() -> const FSlateBrush*
					{
						return FDeferredCleanupSlateBrush::TrySlateBrush(PreviewBrush);
					})
				]
			]
			// Empty-state prompt — hidden once a field is rendered or while evaluating.
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PreviewEmpty", "Press \"Evaluate Preview\" to render the graph output."))
				.Visibility_Lambda([this]() -> EVisibility
				{
					return (!PreviewBrush.IsValid() && !bEvaluating) ? EVisibility::Visible : EVisibility::Collapsed;
				})
			]
			// Evaluating overlay — indeterminate progress bar shown while a worker eval is in flight.
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				.Visibility_Lambda([this]() -> EVisibility
				{
					return bEvaluating ? EVisibility::Visible : EVisibility::Collapsed;
				})
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PreviewEvaluating", "Evaluating graph…"))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.WidthOverride(160.0f)
					[
						SNew(SProgressBar)
					]
				]
			]
		]
	];
}

void SArcHeightmapGraphPreview::SetField(const UE::Geometry::FSampledScalarField2f& Field)
{
	const int32 W = Field.Width();
	const int32 H = Field.Height();
	if (W < 1 || H < 1)
	{
		return;
	}

	const TArray64<float>& Vals = Field.GridValues.GridValues();
	float MinV = TNumericLimits<float>::Max();
	float MaxV = TNumericLimits<float>::Lowest();
	for (int64 Index = 0; Index < Vals.Num(); ++Index)
	{
		const float V = Vals[Index];
		MinV = FMath::Min(MinV, V);
		MaxV = FMath::Max(MaxV, V);
	}
	const float Range = MaxV - MinV;

	UTexture2D* NewTexture = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
	NewTexture->SRGB = false;
	NewTexture->CompressionSettings = TC_VectorDisplacementmap;

	FTexture2DMipMap& Mip = NewTexture->GetPlatformData()->Mips[0];
	uint8* Dst = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			const float Cell = Field.GridValues.At(X, Y);
			const uint8 G = (Range > UE_SMALL_NUMBER)
				? static_cast<uint8>(FMath::Clamp((Cell - MinV) / Range, 0.0f, 1.0f) * 255.0f)
				: 128;
			const int32 Idx = (Y * W + X) * 4;
			Dst[Idx + 0] = G;
			Dst[Idx + 1] = G;
			Dst[Idx + 2] = G;
			Dst[Idx + 3] = 255;
		}
	}
	Mip.BulkData.Unlock();
	NewTexture->UpdateResource();

	// FDeferredCleanupSlateBrush GC-roots NewTexture and defers release of the previously-held
	// brush/texture through the render pipeline, so reassigning here is safe even mid-frame.
	PreviewBrush = FDeferredCleanupSlateBrush::CreateBrush(NewTexture, FVector2D(W, H));

	Invalidate(EInvalidateWidgetReason::Paint);
}

void SArcHeightmapGraphPreview::SetResult(const FArcHeightmapEvalResult& Result)
{
	// Copy the fields so RenderSelected can reference them at any future point.
	PreviewHeight = Result.Height;
	PreviewLayers = Result.Layers;

	// Rebuild the option list: Height (NAME_None) first, then layer names in sorted order
	// for deterministic presentation regardless of TMap iteration order.
	TArray<FName> LayerNames;
	for (const TPair<FName, UE::Geometry::FSampledScalarField2f>& Pair : Result.Layers)
	{
		LayerNames.Add(Pair.Key);
	}
	LayerNames.Sort(FNameLexicalLess());

	// Remember the previously-selected name so we can restore it if it still exists.
	const FName PrevName = SelectedOption.IsValid() ? *SelectedOption : NAME_None;

	PreviewOptions.Reset();
	PreviewOptions.Add(MakeShared<FName>(NAME_None)); // Height entry
	for (const FName& LayerName : LayerNames)
	{
		PreviewOptions.Add(MakeShared<FName>(LayerName));
	}

	// Resolve the selection: keep previous if it still exists, else fall back to Height.
	SelectedOption = PreviewOptions[0]; // default: Height
	for (const TSharedPtr<FName>& Option : PreviewOptions)
	{
		if (Option.IsValid() && *Option == PrevName)
		{
			SelectedOption = Option;
			break;
		}
	}

	if (SelectorCombo.IsValid())
	{
		SelectorCombo->RefreshOptions();
		SelectorCombo->SetSelectedItem(SelectedOption);
	}

	RenderSelected();
}

void SArcHeightmapGraphPreview::RenderSelected()
{
	const bool bHeightSelected = !SelectedOption.IsValid() || SelectedOption->IsNone();
	if (bHeightSelected)
	{
		SetField(PreviewHeight);
		return;
	}

	const UE::Geometry::FSampledScalarField2f* LayerField = PreviewLayers.Find(*SelectedOption);
	if (LayerField != nullptr)
	{
		SetField(*LayerField);
	}
	else
	{
		// Layer no longer present — fall back to the heightfield.
		SetField(PreviewHeight);
	}
}

void SArcHeightmapGraphPreview::SetEvaluating(bool bInEvaluating)
{
	if (bEvaluating == bInEvaluating)
	{
		return;
	}
	bEvaluating = bInEvaluating;
	Invalidate(EInvalidateWidgetReason::Paint);
}

#undef LOCTEXT_NAMESPACE
