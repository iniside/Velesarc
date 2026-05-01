// Copyright Lukasz Baran. All Rights Reserved.

#include "Attributes/ArcAttributeNotification.h"
#include "Attributes/ArcAttributeHandlerConfig.h"

namespace ArcAttributes
{

bool NotifyAttributeChanged(
	FArcAttributeHandlerContext& Context,
	const FArcAttributeRef& AttrRef,
	FArcAttribute& Attr,
	float OldValue,
	float NewValue)
{
	if (OldValue == NewValue)
	{
		return true;
	}

	if (Context.HandlerConfig)
	{
		const FArcAttributeHandler* Handler = Context.HandlerConfig->FindHandler(AttrRef);
		if (Handler && !const_cast<FArcAttributeHandler*>(Handler)->PreChange(Context, Attr, NewValue))
		{
			return false;
		}
	}

	Attr.CurrentValue = NewValue;

	if (Context.HandlerConfig)
	{
		const FArcAttributeHandler* Handler = Context.HandlerConfig->FindHandler(AttrRef);
		if (Handler)
		{
			const_cast<FArcAttributeHandler*>(Handler)->PostChange(Context, Attr, OldValue, NewValue);
		}
	}

	return true;
}

void NotifyAttributeExecuted(
	FArcAttributeHandlerContext& Context,
	const FArcAttributeRef& AttrRef,
	FArcAttribute& Attr,
	float OldValue,
	float NewValue)
{
	if (OldValue == NewValue)
	{
		return;
	}

	if (Context.HandlerConfig)
	{
		const FArcAttributeHandler* Handler = Context.HandlerConfig->FindHandler(AttrRef);
		if (Handler)
		{
			const_cast<FArcAttributeHandler*>(Handler)->PostExecute(Context, Attr, OldValue, NewValue);
		}
	}
}

} // namespace ArcAttributes
