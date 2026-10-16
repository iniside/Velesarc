// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Mass/EntityHandle.h"

struct FArcAttributeHandlerContext;
struct FArcPendingAttributeEvent;
struct FArcAggregator;
struct FMassEntityManager;
struct FMassExecutionContext;
class UArcAttributeHandlerConfig;

namespace ArcAttributes
{
	ARCMASSABILITIES_API bool NotifyAttributeChanged(
		FArcAttributeHandlerContext& Context,
		const FArcAttributeRef& AttrRef,
		FArcAttribute& Attr,
		float OldValue,
		float NewValue);

	ARCMASSABILITIES_API void NotifyAttributeExecuted(
		FArcAttributeHandlerContext& Context,
		const FArcAttributeRef& AttrRef,
		FArcAttribute& Attr,
		float OldValue,
		float NewValue);

	/** Writes NewValue to both Base+Current (SetValueDirect) and, when the value changed,
	 *  publishes ArcEvent.Attribute.Changed on UArcGameEventSubsystem.
	 *  For no-aggregator resources whose committed change must update both fields. */
	ARCMASSABILITIES_API void SetAttributeDirectAndNotify(
		const FArcAttributeHandlerContext& Context,
		const FArcAttributeRef& AttributeRef,
		FArcAttribute& Attribute,
		float NewValue);

	ARCMASSABILITIES_API void EnsureAggregationPolicy(
		FArcAggregator& Aggregator,
		const FArcAttributeRef& AttributeRef,
		const UArcAttributeHandlerConfig* HandlerConfig);

	ARCMASSABILITIES_API void RecalculateLiveDependents(
		FMassEntityManager& EntityManager,
		FMassExecutionContext* MassContext,
		FMassEntityHandle Entity,
		const UArcAttributeHandlerConfig* HandlerConfig,
		const FArcAttributeRef& ChangedAttribute,
		TArray<FArcPendingAttributeEvent>& OutEvents);

	ARCMASSABILITIES_API void RecalculateLiveDependents(
		const FArcAttributeHandlerContext& Context,
		const FArcAttributeRef& ChangedAttribute);

	/** Publishes ArcEvent.Attribute.Changed events, pushes FArcAttributeChangedFragment sparse elements,
	 *  and signals AttributeChanged for each entity that changed.
	 *  No-op when Events is empty. */
	ARCMASSABILITIES_API void FlushAttributeChangeEvents(
		FMassEntityManager& EntityManager,
		FMassExecutionContext& Context,
		TConstArrayView<FArcPendingAttributeEvent> Events);
}
