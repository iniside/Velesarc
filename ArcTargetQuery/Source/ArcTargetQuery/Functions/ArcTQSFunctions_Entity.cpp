// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSFunctions_Entity.h"

#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTQSFunctions_Entity)

namespace ArcTQSFunctions_Private
{
	static const FTransformFragment* FindTransform(const FArcTQSFunctionContext& Context, FMassEntityHandle Entity)
	{
		if (!Context.EntityManager || !Entity.IsValid() || !Context.EntityManager->IsEntityActive(Entity))
		{
			return nullptr;
		}
		return Context.EntityManager->GetFragmentDataPtr<FTransformFragment>(Entity);
	}
}

void FArcTQSFunction_EntityLocation::Execute(const FArcTQSFunctionContext& Context)
{
	const FTransformFragment* Transform = ArcTQSFunctions_Private::FindTransform(Context, Entity);
	Result = Transform ? Transform->GetTransform().GetLocation() : Fallback;
}

void FArcTQSFunction_EntityForward::Execute(const FArcTQSFunctionContext& Context)
{
	const FTransformFragment* Transform = ArcTQSFunctions_Private::FindTransform(Context, Entity);
	Result = Transform ? Transform->GetTransform().GetRotation().GetForwardVector() : Fallback;
}

void FArcTQSFunction_EntityIsValid::Execute(const FArcTQSFunctionContext& Context)
{
	Result = Context.EntityManager && Entity.IsValid() && Context.EntityManager->IsEntityActive(Entity);
}
