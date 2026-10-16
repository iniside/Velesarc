// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEnviroAttributeHandlers.h"

#include "Attributes/ArcAttributeNotification.h"
#include "Modifiers/ArcModifierFunctions.h"

namespace ArcEnviroAttributeHandlersPrivate
{
	void WriteLevel(FArcAttributeHandlerContext& Context, const FArcAttributeRef& LevelRef, float NewLevel)
	{
		FArcAttribute* LevelAttribute = ArcAttributes::ResolveAttribute(Context.EntityManager, Context.Entity, LevelRef);
		if (!LevelAttribute || LevelAttribute->GetCurrentValue() == NewLevel)
		{
			return;
		}
		ArcAttributes::SetAttributeDirectAndNotify(Context, LevelRef, *LevelAttribute, NewLevel);
	}
}

void FArcEnviroAttributeHandler_ConditionDelta::PostExecute(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) const
{
	using namespace ArcEnviroAttributeHandlersPrivate;

	const float Requested = NewValue;
	Attribute.InitValue(0.f);

	if (Requested == 0.f || (Op != EArcResourceOp::AddSigned && Requested < 0.f))
	{
		return;
	}

	FArcAttribute* ResourceAttribute = ArcAttributes::ResolveAttribute(Context.EntityManager, Context.Entity, Resource);
	if (!ResourceAttribute)
	{
		return;
	}

	const float Delta = Op == EArcResourceOp::Subtract ? -Requested : Requested;
	const float UpperBound = MaxConst.IsSet() ? MaxConst.GetValue() : TNumericLimits<float>::Max();
	const float NewResource = FMath::Clamp(ResourceAttribute->GetCurrentValue() + Delta, MinConst, UpperBound);
	ArcAttributes::SetAttributeDirectAndNotify(Context, Resource, *ResourceAttribute, NewResource);

	WriteLevel(Context, Level, static_cast<float>(Bands.GetTier(NewResource)));
	WriteLevel(Context, NegativeLevel, static_cast<float>(Bands.GetTier(-NewResource)));
}
