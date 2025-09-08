#include "ArcStateTreeMassEntityLocationPropertyFunction.h"

#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeExecutionContext.h"

void FArcStateTreeMassEntityLocationPropertyFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	UMassEntitySubsystem* MassSubsystem = MassCtx.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(MassCtx.GetEntity());

	const FVector::FReal Dist = FVector::Dist(InstanceData.Input.GetCachedEntityPosition(), TransformFragment->GetTransform().GetLocation());
	
	InstanceData.Output.EndOfPathPosition = InstanceData.Input.GetCachedEntityPosition();
	InstanceData.Output.EndOfPathIntent = InstanceData.EndOfPathIntent;

	DrawDebugSphere(Context.GetWorld(), InstanceData.Input.GetCachedEntityPosition(), 32.f, 10, FColor::Red, false, 20.f);
}
