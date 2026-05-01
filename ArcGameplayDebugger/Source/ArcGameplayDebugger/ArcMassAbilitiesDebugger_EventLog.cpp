// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassAbilitiesDebugger.h"

#if ARC_MASSABILITIES_DEBUG_EVENTLOG

#include "imgui.h"
#include "Events/ArcEffectEventSubsystem.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Effects/ArcEffectDefinition.h"
#include "Attributes/ArcAttribute.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

namespace ArcMassAbilitiesDebugger_EventLog
{
	static UWorld* GetDebugWorld()
	{
		if (!GEngine || !GEngine->GameViewport)
		{
			return nullptr;
		}
		return GEngine->GameViewport->GetWorld();
	}
}

void FArcMassAbilitiesDebugger::SubscribeEvents(FMassEntityHandle Entity)
{
	UWorld* World = ArcMassAbilitiesDebugger_EventLog::GetDebugWorld();
	if (!World)
	{
		return;
	}

	UArcEffectEventSubsystem* EventSub = World->GetSubsystem<UArcEffectEventSubsystem>();
	if (!EventSub)
	{
		return;
	}

	if (bEventLogSubscribed)
	{
		UnsubscribeEvents();
	}

	AnyAttrHandle = EventSub->SubscribeAnyAttributeChanged(Entity,
		FArcOnAnyAttributeChanged::FDelegate::CreateLambda(
			[this](FMassEntityHandle InEntity, FArcAttributeRef Attribute, float OldValue, float NewValue)
			{
				FString Desc = FString::Printf(TEXT("[%s.%s] %.2f -> %.2f"),
					Attribute.FragmentType ? *Attribute.FragmentType->GetName() : TEXT("?"),
					*Attribute.PropertyName.ToString(), OldValue, NewValue);
				PushEvent(EEventType::AttributeChanged, InEntity, Desc);
			}
		));

	AbilityActivatedHandle = EventSub->SubscribeAbilityActivated(Entity,
		FArcOnAbilityActivated::FDelegate::CreateLambda(
			[this](FMassEntityHandle InEntity, UArcAbilityDefinition* Definition)
			{
				FString Desc = FString::Printf(TEXT("Activated: %s"),
					Definition ? *Definition->GetName() : TEXT("?"));
				PushEvent(EEventType::AbilityActivated, InEntity, Desc);
			}
		));

	EffectAppliedHandle = EventSub->SubscribeEffectApplied(Entity,
		FArcOnEffectApplied::FDelegate::CreateLambda(
			[this](FMassEntityHandle InEntity, UArcEffectDefinition* Definition)
			{
				FString Desc = FString::Printf(TEXT("Applied: %s"),
					Definition ? *Definition->GetName() : TEXT("?"));
				PushEvent(EEventType::EffectApplied, InEntity, Desc);
			}
		));

	EffectExecutedHandle = EventSub->SubscribeEffectExecuted(Entity,
		FArcOnEffectExecuted::FDelegate::CreateLambda(
			[this](FMassEntityHandle InEntity, UArcEffectDefinition* Definition)
			{
				FString Desc = FString::Printf(TEXT("Executed: %s"),
					Definition ? *Definition->GetName() : TEXT("?"));
				PushEvent(EEventType::EffectExecuted, InEntity, Desc);
			}
		));

	TagCountChangedHandle = EventSub->SubscribeTagCountChanged(Entity,
		FArcOnTagCountChanged::FDelegate::CreateLambda(
			[this](FMassEntityHandle InEntity, FGameplayTag Tag, int32 OldCount, int32 NewCount)
			{
				FString Desc = FString::Printf(TEXT("[%s] %d -> %d"),
					*Tag.ToString(), OldCount, NewCount);
				PushEvent(EEventType::TagCountChanged, InEntity, Desc);
			}
		));

	SubscribedEntity = Entity;
	bEventLogSubscribed = true;
}

void FArcMassAbilitiesDebugger::UnsubscribeEvents()
{
	UWorld* World = ArcMassAbilitiesDebugger_EventLog::GetDebugWorld();
	if (World)
	{
		UArcEffectEventSubsystem* EventSub = World->GetSubsystem<UArcEffectEventSubsystem>();
		if (EventSub)
		{
			EventSub->UnsubscribeAnyAttributeChanged(SubscribedEntity, AnyAttrHandle);
			EventSub->UnsubscribeAbilityActivated(SubscribedEntity, AbilityActivatedHandle);
			EventSub->UnsubscribeEffectApplied(SubscribedEntity, EffectAppliedHandle);
			EventSub->UnsubscribeEffectExecuted(SubscribedEntity, EffectExecutedHandle);
			EventSub->UnsubscribeTagCountChanged(SubscribedEntity, TagCountChangedHandle);
		}
	}

	AnyAttrHandle.Reset();
	AbilityActivatedHandle.Reset();
	EffectAppliedHandle.Reset();
	EffectExecutedHandle.Reset();
	TagCountChangedHandle.Reset();
	SubscribedEntity = FMassEntityHandle();
	bEventLogSubscribed = false;
}

void FArcMassAbilitiesDebugger::PushEvent(EEventType Type, FMassEntityHandle Entity, FString Description)
{
	UWorld* World = ArcMassAbilitiesDebugger_EventLog::GetDebugWorld();
	double Timestamp = World ? World->GetTimeSeconds() : 0.0;

	FEventLogEntry* Entry = nullptr;
	if (EventLog.Num() < MaxEventLogEntries)
	{
		Entry = &EventLog.AddDefaulted_GetRef();
	}
	else
	{
		Entry = &EventLog[EventLogHead];
		EventLogHead = (EventLogHead + 1) % MaxEventLogEntries;
	}

	Entry->Timestamp = Timestamp;
	Entry->Type = Type;
	Entry->Entity = Entity;
	Entry->Description = MoveTemp(Description);

	EventLogCount = FMath::Min(EventLogCount + 1, MaxEventLogEntries);
}

void FArcMassAbilitiesDebugger::DrawEventLogTab()
{
	ImGui::Text("Event Log (%d entries)", EventLogCount);
	ImGui::SameLine();
	ImGui::Checkbox("Auto Scroll", &bEventLogAutoScroll);
	ImGui::SameLine();
	ImGui::Checkbox("Selected Entity Only", &bEventLogFilterBySelectedEntity);
	ImGui::SameLine();
	if (ImGui::Button("Clear"))
	{
		EventLog.Reset();
		EventLogHead = 0;
		EventLogCount = 0;
	}

	bool bAttrEnabled = (EventLogTypeFilter & (1 << 0)) != 0;
	bool bAbilityEnabled = (EventLogTypeFilter & (1 << 1)) != 0;
	bool bEffectEnabled = (EventLogTypeFilter & (1 << 2)) != 0;
	bool bExecuteEnabled = (EventLogTypeFilter & (1 << 3)) != 0;
	bool bTagEnabled = (EventLogTypeFilter & (1 << 4)) != 0;

	if (ImGui::Checkbox("Attr", &bAttrEnabled))
	{
		if (bAttrEnabled) { EventLogTypeFilter |= (1 << 0); } else { EventLogTypeFilter &= ~(1 << 0); }
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Ability", &bAbilityEnabled))
	{
		if (bAbilityEnabled) { EventLogTypeFilter |= (1 << 1); } else { EventLogTypeFilter &= ~(1 << 1); }
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Effect", &bEffectEnabled))
	{
		if (bEffectEnabled) { EventLogTypeFilter |= (1 << 2); } else { EventLogTypeFilter &= ~(1 << 2); }
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Execute", &bExecuteEnabled))
	{
		if (bExecuteEnabled) { EventLogTypeFilter |= (1 << 3); } else { EventLogTypeFilter &= ~(1 << 3); }
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Tag", &bTagEnabled))
	{
		if (bTagEnabled) { EventLogTypeFilter |= (1 << 4); } else { EventLogTypeFilter &= ~(1 << 4); }
	}

	ImGui::InputText("Filter", EventLogFilterBuf, IM_ARRAYSIZE(EventLogFilterBuf));

	FMassEntityHandle SelectedEntity;
	if (bEventLogFilterBySelectedEntity && CachedEntities.IsValidIndex(SelectedEntityIndex))
	{
		SelectedEntity = CachedEntities[SelectedEntityIndex].Entity;
	}

	FString TextFilter = FString(ANSI_TO_TCHAR(EventLogFilterBuf)).ToLower();

	if (ImGui::BeginChild("EventLogScroll", ImVec2(0, 0)))
	{
		int32 NumEntries = EventLog.Num();
		for (int32 i = 0; i < NumEntries; ++i)
		{
			int32 EntryIdx = (EventLogHead + i) % NumEntries;
			const FEventLogEntry& Entry = EventLog[EntryIdx];

			uint8 TypeBit = 0;
			const char* TypeStr = "";
			ImVec4 Color;

			switch (Entry.Type)
			{
				case EEventType::AttributeChanged:
					TypeBit = 1 << 0;
					TypeStr = "Attr";
					Color = ImVec4(0.3f, 0.8f, 1.0f, 1.0f);
					break;
				case EEventType::AbilityActivated:
					TypeBit = 1 << 1;
					TypeStr = "Ability";
					Color = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
					break;
				case EEventType::EffectApplied:
					TypeBit = 1 << 2;
					TypeStr = "Effect";
					Color = ImVec4(1.0f, 0.9f, 0.2f, 1.0f);
					break;
				case EEventType::EffectExecuted:
					TypeBit = 1 << 3;
					TypeStr = "Execute";
					Color = ImVec4(1.0f, 0.6f, 0.2f, 1.0f);
					break;
				case EEventType::TagCountChanged:
					TypeBit = 1 << 4;
					TypeStr = "Tag";
					Color = ImVec4(1.0f, 0.4f, 1.0f, 1.0f);
					break;
				default:
					TypeBit = 0;
					TypeStr = "?";
					Color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
					break;
			}

			if ((EventLogTypeFilter & TypeBit) == 0)
			{
				continue;
			}

			if (bEventLogFilterBySelectedEntity && SelectedEntity.IsSet() && !(Entry.Entity == SelectedEntity))
			{
				continue;
			}

			if (!TextFilter.IsEmpty() && !Entry.Description.ToLower().Contains(TextFilter))
			{
				continue;
			}

			FString Line = FString::Printf(TEXT("[%.2f][%s] E%d: %s"),
				Entry.Timestamp,
				ANSI_TO_TCHAR(TypeStr),
				Entry.Entity.Index,
				*Entry.Description);

			ImGui::TextColored(Color, "%s", TCHAR_TO_ANSI(*Line));
		}

		if (bEventLogAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
		{
			ImGui::SetScrollHereY(1.0f);
		}
		else if (bEventLogAutoScroll)
		{
			ImGui::SetScrollHereY(1.0f);
		}
	}
	ImGui::EndChild();
}

#endif // ARC_MASSABILITIES_DEBUG_EVENTLOG
