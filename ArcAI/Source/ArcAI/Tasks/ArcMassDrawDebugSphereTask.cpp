#include "ArcMassDrawDebugSphereTask.h"

#include "AIController.h"
#include "DrawDebugHelpers.h"
#include "MassStateTreeExecutionContext.h"
#include "MassStateTreeFragments.h"
#include "MoverMassTranslators.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "VisualLogger/VisualLogger.h"


FArcMassDrawDebugSphereTask::FArcMassDrawDebugSphereTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FArcMassDrawDebugSphereTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	DrawDebugSphere(Context.GetWorld(), InstanceData.Location, InstanceData.Radius, 12, FColor::Red, false, InstanceData.Duration);
	
	return EStateTreeRunStatus::Succeeded;
}

#if WITH_EDITOR
FText FArcMassDrawDebugSphereTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData)
		{
			return FText::Format(NSLOCTEXT("ArcAI", "DrawDebugSphereDesc", "Draw Debug Sphere (R={0})"), FText::AsNumber(InstanceData->Radius));
		}
	}
	return NSLOCTEXT("ArcAI", "DrawDebugSphereDescDefault", "Draw Debug Sphere");
}
#endif
