// Copyright Lukasz Baran. All Rights Reserved.

#include "Events/ArcGameEventFunctions.h"
#include "Events/ArcGameEvent.h"
#include "Effects/ArcEffectTagSource.h"

namespace ArcGameEvents
{

void GatherEnvelopeTags(FMassEntityManager& EntityManager, FArcGameEvent& Event, const FArcEffectContext& Context)
{
	FArcEffectContext EventContext = Context;
	EventContext.EntityManager = &EntityManager;
	EventContext.SourceEntity = Event.Instigator;
	EventContext.TargetEntity = Event.Target;
	ArcEffectTagSources::GatherTags(EventContext, Event.SourceTags, Event.TargetTags);
}

}
