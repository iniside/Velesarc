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

#include "ArcCraftEditorDebugSettings.h"

void UArcCraftEditorDebugSettings::SaveDebugContext(const FPrimaryAssetId& RecordId, const FArcModifierDebugContext& Ctx)
{
	if (!RecordId.IsValid())
	{
		return;
	}

	UArcCraftEditorDebugSettings* Settings = GetMutableDefault<UArcCraftEditorDebugSettings>();
	check(Settings != nullptr);

	FArcModifierDebugContextSerialized& Entry = Settings->RecordDebugContexts.FindOrAdd(RecordId);
	Entry.AverageR = Ctx.AverageR;
	Entry.IngredientTags = Ctx.IngredientTags;
	Entry.SlotTags = Ctx.SlotTags;
	Entry.OutputRarity = Ctx.OutputRarity;
	Entry.SkillMargin = Ctx.SkillMargin;
	Entry.bEnabled = Ctx.bEnabled;

	Entry.InputSlots.Reset();
	Entry.InputSlots.Reserve(Ctx.InputSlots.Num());
	for (const FArcDebugIngredientSlot& Slot : Ctx.InputSlots)
	{
		FArcDebugIngredientSlotSerialized& SerialSlot = Entry.InputSlots.AddDefaulted_GetRef();
		SerialSlot.Tags = Slot.Tags;
		SerialSlot.RarityStep = Slot.RarityStep;
	}

	Settings->SaveConfig();
}

FArcModifierDebugContext UArcCraftEditorDebugSettings::LoadDebugContext(const FPrimaryAssetId& RecordId)
{
	FArcModifierDebugContext Result;
	if (!RecordId.IsValid())
	{
		return Result;
	}

	const UArcCraftEditorDebugSettings* Settings = GetDefault<UArcCraftEditorDebugSettings>();
	check(Settings != nullptr);

	const FArcModifierDebugContextSerialized* Found = Settings->RecordDebugContexts.Find(RecordId);
	if (Found == nullptr)
	{
		return Result;
	}

	Result.AverageR = Found->AverageR;
	Result.IngredientTags = Found->IngredientTags;
	Result.SlotTags = Found->SlotTags;
	Result.OutputRarity = Found->OutputRarity;
	Result.SkillMargin = Found->SkillMargin;
	Result.bEnabled = Found->bEnabled;

	Result.InputSlots.Reset();
	Result.InputSlots.Reserve(Found->InputSlots.Num());
	for (const FArcDebugIngredientSlotSerialized& SerialSlot : Found->InputSlots)
	{
		FArcDebugIngredientSlot& Slot = Result.InputSlots.AddDefaulted_GetRef();
		Slot.Tags = SerialSlot.Tags;
		Slot.RarityStep = SerialSlot.RarityStep;
	}

	return Result;
}
