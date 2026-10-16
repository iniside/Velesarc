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
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "Widgets/ArcModifierDebugContext.h"
#include "ArcCraftEditorDebugSettings.generated.h"

/** Serializable mirror of FArcDebugIngredientSlot for config persistence. */
USTRUCT()
struct FArcDebugIngredientSlotSerialized
{
	GENERATED_BODY()

	UPROPERTY(config)
	FGameplayTagContainer Tags;

	UPROPERTY(config)
	float RarityStep = 1.0f;
};

/** Serializable mirror of FArcModifierDebugContext for per-record persistence. */
USTRUCT()
struct FArcModifierDebugContextSerialized
{
	GENERATED_BODY()

	UPROPERTY()
	float AverageR = 1.0f;

	UPROPERTY()
	FGameplayTagContainer IngredientTags;

	UPROPERTY()
	FGameplayTagContainer SlotTags;

	UPROPERTY()
	FGameplayTag OutputRarity;

	UPROPERTY()
	float SkillMargin = 0.0f;

	UPROPERTY()
	bool bEnabled = false;

	UPROPERTY(config)
	TArray<FArcDebugIngredientSlotSerialized> InputSlots;
};

/**
 * Per-project per-user editor settings for ArcCraft debug toolbar state.
 * Persists the last debug context (R, tags, enabled) for each record.
 * Appears in Project Settings → Arc Game → ArcCraft Editor Debug.
 */
UCLASS(Config=EditorPerProjectUserSettings, DefaultConfig)
class UArcCraftEditorDebugSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName(TEXT("Arc Game")); }

	/** Debug context keyed by record id. */
	UPROPERTY(Config)
	TMap<FPrimaryAssetId, FArcModifierDebugContextSerialized> RecordDebugContexts;

	/** Persist a debug context for the given record. No-op if RecordId is invalid. */
	static void SaveDebugContext(const FPrimaryAssetId& RecordId, const FArcModifierDebugContext& Ctx);

	/** Load a previously saved debug context for the given record.
	 *  Returns a default-constructed context if RecordId is invalid or no entry exists. */
	static FArcModifierDebugContext LoadDebugContext(const FPrimaryAssetId& RecordId);
};
