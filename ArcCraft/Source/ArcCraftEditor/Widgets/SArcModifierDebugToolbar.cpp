/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "Widgets/SArcModifierDebugToolbar.h"

#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/AppStyle.h"
#include "Containers/Ticker.h"
#include "SGameplayTagCombo.h"
#include "SGameplayTagContainerCombo.h"
#include "ArcCraft/Rarity/ArcCraftRarity.h"
#include "ArcCraft/Rarity/ArcCraftRulesData.h"

#define LOCTEXT_NAMESPACE "SArcModifierDebugToolbar"

namespace ArcModifierDebugToolbarImpl
{
	static const float DebounceDelaySeconds = 0.1f;
	static const float RMin = 0.0f;
	static const float FallbackRMax = 2.0f;

	/** Highest ladder step of the rules record, so the slider reaches the top rarity. */
	static float GetRMax()
	{
		float MaxStep = 0.0f;
		if (const FArcCraftRulesData* Rules = ArcCraftRarity::GetRules())
		{
			for (const FArcCraftRarityStep& LadderStep : Rules->Ladder)
			{
				MaxStep = FMath::Max(MaxStep, LadderStep.Step);
			}
		}
		return MaxStep > 0.0f ? MaxStep : FallbackRMax;
	}
} // namespace ArcModifierDebugToolbarImpl

void SArcModifierDebugToolbar::Construct(const FArguments& InArgs)
{
	CurrentContext = InArgs._InitialContext;
	OnDebugContextChanged = InArgs._OnDebugContextChanged;

	// Slot list container — populated by RebuildSlotList()
	TSharedRef<SVerticalBox> SlotListBoxRef = SNew(SVerticalBox);
	SlotListBox = SlotListBoxRef;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(6.0f, 4.0f))
		[
			SNew(SVerticalBox)

			// ── Top row: Live Preview checkbox ───────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this]() -> ECheckBoxState
					{
						return CurrentContext.bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged(this, &SArcModifierDebugToolbar::OnLivePreviewToggled)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("LivePreview", "Live Preview"))
						.Font(FAppStyle::GetFontStyle("SmallFont"))
					]
				]

				// Output rarity
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(12.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("OutputRarityLabel", "Output rarity:"))
					.ToolTipText(LOCTEXT("OutputRarityTip", "Rarity whose band the stat previews and conversions roll in; empty = full catalog range"))
					.Font(FAppStyle::GetFontStyle("SmallFont"))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SBox)
					.MinDesiredWidth(140.0f)
					[
						SNew(SGameplayTagCombo)
						.Filter(TEXT("Item.Rarity"))
						.Tag_Lambda([this]() -> FGameplayTag
						{
							return CurrentContext.OutputRarity;
						})
						.OnTagChanged_Lambda([this](const FGameplayTag NewTag)
						{
							CurrentContext.OutputRarity = NewTag;
							ArmDebounce();
						})
					]
				]

				// Skill margin
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SkillMarginLabel", "Skill margin:"))
					.ToolTipText(LOCTEXT("SkillMarginTip", "Crafter skill above the recipe's required level"))
					.Font(FAppStyle::GetFontStyle("SmallFont"))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(72.0f)
					[
						SNew(SSpinBox<float>)
						.Value_Lambda([this]() -> float
						{
							return CurrentContext.SkillMargin;
						})
						.MinValue(0.0f)
						.Delta(1.0f)
						.OnValueChanged_Lambda([this](float NewValue)
						{
							CurrentContext.SkillMargin = FMath::Max(0.0f, NewValue);
							ArmDebounce();
						})
						.Font(FAppStyle::GetFontStyle("SmallFont"))
					]
				]
			]

			// ── Legacy R slider (visible only when no input slots) ─
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(SBox)
				.Visibility_Lambda([this]() -> EVisibility
				{
					return CurrentContext.InputSlots.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
				})
				[
					SNew(SHorizontalBox)

					// R label
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("RLabel", "R:"))
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.IsEnabled_Lambda([this]() -> bool { return CurrentContext.bEnabled; })
					]

					// R slider
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(SBox)
						.WidthOverride(120.0f)
						[
							SNew(SSlider)
							.Value_Lambda([this]() -> float { return CurrentContext.AverageR; })
							.MinValue(ArcModifierDebugToolbarImpl::RMin)
							.MaxValue(ArcModifierDebugToolbarImpl::GetRMax())
							.OnValueChanged(this, &SArcModifierDebugToolbar::OnAverageRSliderChanged)
							.IsEnabled_Lambda([this]() -> bool { return CurrentContext.bEnabled; })
						]
					]

					// R value display
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 12.0f, 0.0f)
					[
						SNew(SBox)
						.WidthOverride(36.0f)
						[
							SNew(STextBlock)
							.Text_Lambda([this]() -> FText
							{
								return FText::AsNumber(CurrentContext.AverageR, &FNumberFormattingOptions().SetMaximumFractionalDigits(2));
							})
							.Font(FAppStyle::GetFontStyle("SmallFont"))
							.IsEnabled_Lambda([this]() -> bool { return CurrentContext.bEnabled; })
						]
					]
				]
			]

			// ── Slot list (rebuilt on add/remove) ────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SlotListBoxRef
			]

			// ── Add Slot / Clear All buttons ─────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("AddSlot", "+ Add Slot"))
					.OnClicked_Lambda([this]() -> FReply
					{
						OnAddSlot();
						return FReply::Handled();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("ClearAll", "Clear All"))
					.IsEnabled_Lambda([this]() -> bool { return !CurrentContext.InputSlots.IsEmpty(); })
					.OnClicked_Lambda([this]() -> FReply
					{
						OnClearAllSlots();
						return FReply::Handled();
					})
				]
			]
		]
	];

	RebuildSlotList();
}

SArcModifierDebugToolbar::~SArcModifierDebugToolbar()
{
	if (DebounceHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(DebounceHandle);
		DebounceHandle.Reset();
	}
}

void SArcModifierDebugToolbar::RebuildSlotList()
{
	if (!SlotListBox.IsValid())
	{
		return;
	}

	SlotListBox->ClearChildren();

	for (int32 SlotIdx = 0; SlotIdx < CurrentContext.InputSlots.Num(); ++SlotIdx)
	{
		SlotListBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 2.0f)
		[
			BuildSlotRow(SlotIdx)
		];
	}
}

TSharedRef<SWidget> SArcModifierDebugToolbar::BuildSlotRow(int32 SlotIndex)
{
	// Capture by value for the index; lambdas re-check bounds before access.
	return SNew(SHorizontalBox)

		// Slot label
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("SlotLabel", "Slot {0}:"), FText::AsNumber(SlotIndex + 1)))
			.Font(FAppStyle::GetFontStyle("SmallFont"))
		]

		// Tag container combo
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 6.0f, 0.0f)
		[
			SNew(SBox)
			.MinDesiredWidth(160.0f)
			[
				SNew(SGameplayTagContainerCombo)
				.TagContainer_Lambda([this, SlotIndex]() -> FGameplayTagContainer
				{
					if (CurrentContext.InputSlots.IsValidIndex(SlotIndex))
					{
						return CurrentContext.InputSlots[SlotIndex].Tags;
					}
					return FGameplayTagContainer{};
				})
				.OnTagContainerChanged_Lambda([this, SlotIndex](const FGameplayTagContainer& NewTags)
				{
					OnSlotTagsChanged(NewTags, SlotIndex);
				})
			]
		]

		// R spin box
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 6.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(72.0f)
			[
				SNew(SSpinBox<float>)
				.Value_Lambda([this, SlotIndex]() -> float
				{
					if (CurrentContext.InputSlots.IsValidIndex(SlotIndex))
					{
						return CurrentContext.InputSlots[SlotIndex].RarityStep;
					}
					return 1.0f;
				})
				.MinValue(ArcModifierDebugToolbarImpl::RMin)
				.MaxValue(ArcModifierDebugToolbarImpl::GetRMax())
				.Delta(0.05f)
				.OnValueChanged_Lambda([this, SlotIndex](float NewValue)
				{
					OnSlotRarityStepChanged(NewValue, SlotIndex);
				})
				.Font(FAppStyle::GetFontStyle("SmallFont"))
			]
		]

		// Remove button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.Text(LOCTEXT("RemoveSlot", "X"))
			.ToolTipText(LOCTEXT("RemoveSlotTip", "Remove this ingredient slot"))
			.OnClicked_Lambda([this, SlotIndex]() -> FReply
			{
				OnRemoveSlot(SlotIndex);
				return FReply::Handled();
			})
		];
}

void SArcModifierDebugToolbar::OnAddSlot()
{
	CurrentContext.InputSlots.AddDefaulted();
	RebuildSlotList();
	ArmDebounce();
}

void SArcModifierDebugToolbar::OnRemoveSlot(int32 SlotIndex)
{
	if (CurrentContext.InputSlots.IsValidIndex(SlotIndex))
	{
		CurrentContext.InputSlots.RemoveAt(SlotIndex);
		RebuildSlotList();
		ArmDebounce();
	}
}

void SArcModifierDebugToolbar::OnClearAllSlots()
{
	CurrentContext.InputSlots.Reset();
	RebuildSlotList();
	ArmDebounce();
}

void SArcModifierDebugToolbar::OnSlotTagsChanged(const FGameplayTagContainer& NewTags, int32 SlotIndex)
{
	if (CurrentContext.InputSlots.IsValidIndex(SlotIndex))
	{
		CurrentContext.InputSlots[SlotIndex].Tags = NewTags;
		ArmDebounce();
	}
}

void SArcModifierDebugToolbar::OnSlotRarityStepChanged(float NewValue, int32 SlotIndex)
{
	if (CurrentContext.InputSlots.IsValidIndex(SlotIndex))
	{
		CurrentContext.InputSlots[SlotIndex].RarityStep = NewValue;
		ArmDebounce();
	}
}

void SArcModifierDebugToolbar::OnAverageRSliderChanged(float NewValue)
{
	CurrentContext.AverageR = NewValue;
	ArmDebounce();
}

void SArcModifierDebugToolbar::OnLivePreviewToggled(ECheckBoxState NewState)
{
	CurrentContext.bEnabled = (NewState == ECheckBoxState::Checked);
	ArmDebounce();
}

void SArcModifierDebugToolbar::ArmDebounce()
{
	// Remove any pending ticker so we restart the delay.
	if (DebounceHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(DebounceHandle);
		DebounceHandle.Reset();
	}

	DebounceHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateSP(this, &SArcModifierDebugToolbar::OnDebounce),
		ArcModifierDebugToolbarImpl::DebounceDelaySeconds);
}

bool SArcModifierDebugToolbar::OnDebounce(float /*DeltaTime*/)
{
	DebounceHandle.Reset();
	OnDebugContextChanged.ExecuteIfBound(CurrentContext);
	// Return false so the ticker fires only once.
	return false;
}

void SArcModifierDebugToolbar::SetDebugContext(const FArcModifierDebugContext& InContext)
{
	CurrentContext = InContext;
	RebuildSlotList();
	// Remove any pending debounce — the caller already knows the context.
	if (DebounceHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(DebounceHandle);
		DebounceHandle.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
