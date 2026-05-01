// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/EntityHandle.h"
#include "MassEntityTypes.h"
#include "GameplayTagContainer.h"
#include "Attributes/ArcAttribute.h"

#ifndef ARC_MASSABILITIES_DEBUG_EVENTLOG
#define ARC_MASSABILITIES_DEBUG_EVENTLOG WITH_MASSENTITY_DEBUG
#endif

class UArcEffectDefinition;
class UArcAbilityDefinition;
struct FMassEntityManager;
struct FArcAbilityCollectionFragment;
struct FArcEffectStackFragment;
struct FArcAggregatorFragment;
struct FArcOwnedTagsFragment;
struct FArcAbilityCooldownFragment;
struct FArcAbilityInputFragment;
struct FArcImmunityFragment;
struct FArcPendingAttributeOpsFragment;
struct FArcActiveAbility;
struct FArcActiveEffect;

class FArcMassAbilitiesDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	struct FEntityEntry
	{
		FMassEntityHandle Entity;
		FString Label;
		FVector Location = FVector::ZeroVector;
	};

	void RefreshEntityList();
	void DrawEntityListPanel();

	TArray<FEntityEntry> CachedEntities;
	int32 SelectedEntityIndex = INDEX_NONE;
	char EntityFilterBuf[256] = {};
	float LastRefreshTime = 0.f;

	void DrawAbilitiesTab(FMassEntityManager& Manager, FMassEntityHandle Entity);
	void DrawEffectsTab(FMassEntityManager& Manager, FMassEntityHandle Entity);
	void DrawAttributesTab(FMassEntityManager& Manager, FMassEntityHandle Entity);

	int32 AbilitySelectedIndex = INDEX_NONE;
	int32 AbilitySTSelectedFrameIdx = INDEX_NONE;
	uint16 AbilitySTSelectedStateIdx = MAX_uint16;
	bool bAbilitySTSelectedFrameGlobal = false;

	int32 EffectSelectedIndex = INDEX_NONE;
	int32 EffectSTSelectedFrameIdx = INDEX_NONE;
	uint16 EffectSTSelectedStateIdx = MAX_uint16;
	bool bEffectSTSelectedFrameGlobal = false;

	void DrawStateTreeIntrospection(
		const struct FStateTreeInstanceData& InstanceData,
		const class UStateTree* StateTree,
		int32& SelectedFrameIdx,
		uint16& SelectedStateIdx,
		bool& bSelectedFrameGlobal,
		const char* IdPrefix
	);

	void DrawStateTreeSelectedStateDetails(
		const struct FStateTreeInstanceData& InstanceData,
		const class UStateTree* StateTree,
		int32 SelectedFrameIdx,
		uint16 SelectedStateIdx,
		bool bSelectedFrameGlobal
	);

#if ARC_MASSABILITIES_DEBUG_EVENTLOG
	enum class EEventType : uint8
	{
		AttributeChanged,
		AbilityActivated,
		EffectApplied,
		EffectExecuted,
		TagCountChanged
	};

	struct FEventLogEntry
	{
		double Timestamp = 0.0;
		EEventType Type = EEventType::AttributeChanged;
		FMassEntityHandle Entity;
		FString Description;
	};

	static constexpr int32 MaxEventLogEntries = 512;
	TArray<FEventLogEntry> EventLog;
	int32 EventLogHead = 0;
	int32 EventLogCount = 0;
	bool bEventLogSubscribed = false;
	char EventLogFilterBuf[256] = {};
	bool bEventLogFilterBySelectedEntity = true;
	uint8 EventLogTypeFilter = 0xFF;
	bool bEventLogAutoScroll = true;

	FMassEntityHandle SubscribedEntity;
	FDelegateHandle AnyAttrHandle;
	FDelegateHandle AbilityActivatedHandle;
	FDelegateHandle EffectAppliedHandle;
	FDelegateHandle EffectExecutedHandle;
	FDelegateHandle TagCountChangedHandle;

	void SubscribeEvents(FMassEntityHandle Entity);
	void UnsubscribeEvents();
	void PushEvent(EEventType Type, FMassEntityHandle Entity, FString Description);
	void DrawEventLogTab();
#endif
};
