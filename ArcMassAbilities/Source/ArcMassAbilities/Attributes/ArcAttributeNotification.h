// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"

struct FArcAttributeHandlerContext;

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
}
