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

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/ArcModifierDebugContext.h"
#include "Containers/Ticker.h"

class SVerticalBox;

DECLARE_DELEGATE_OneParam(FArcOnDebugContextChanged, const FArcModifierDebugContext&);

/**
 * Debug toolbar for the modifier asset editor.
 * Contains: multi-slot ingredient simulator (tag container + R per slot),
 * legacy R slider (shown only when no slots exist), "Live preview" checkbox.
 * Changes are debounced ~100ms before emitting OnDebugContextChanged.
 */
class SArcModifierDebugToolbar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcModifierDebugToolbar)
		: _InitialContext()
	{}
		SLATE_ARGUMENT(FArcModifierDebugContext, InitialContext)
		SLATE_EVENT(FArcOnDebugContextChanged, OnDebugContextChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SArcModifierDebugToolbar() override;

	/** Returns the current debug context. */
	const FArcModifierDebugContext& GetContext() const { return CurrentContext; }

	/** Programmatically push a loaded context (e.g. restored from settings). Does NOT fire the debounce. */
	void SetDebugContext(const FArcModifierDebugContext& InContext);

private:
	void OnAverageRSliderChanged(float NewValue);
	void OnLivePreviewToggled(ECheckBoxState NewState);

	/** Rebuilds the slot-list widget from CurrentContext.InputSlots. */
	void RebuildSlotList();

	/** Builds one slot row widget for the given slot index. */
	TSharedRef<SWidget> BuildSlotRow(int32 SlotIndex);

	void OnAddSlot();
	void OnRemoveSlot(int32 SlotIndex);
	void OnClearAllSlots();
	void OnSlotTagsChanged(const FGameplayTagContainer& NewTags, int32 SlotIndex);
	void OnSlotRarityStepChanged(float NewValue, int32 SlotIndex);

	/** Arms or re-arms the debounce ticker; fires OnDebugContextChanged after ~100ms. */
	void ArmDebounce();

	/** Ticker callback; emits the delegate and removes itself. */
	bool OnDebounce(float DeltaTime);

	FArcModifierDebugContext CurrentContext;
	FArcOnDebugContextChanged OnDebugContextChanged;
	FTSTicker::FDelegateHandle DebounceHandle;

	/** Slot list container — rebuilt whenever the slot count changes. */
	TSharedPtr<SVerticalBox> SlotListBox;
};
