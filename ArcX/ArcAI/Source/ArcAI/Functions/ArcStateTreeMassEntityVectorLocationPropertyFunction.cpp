#include "ArcStateTreeMassEntityVectorLocationPropertyFunction.h"

#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassStateTreeExecutionContext.h"

void FArcStateTreeMassEntityVectorLocationPropertyFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	
	FMassEntityManager& EntityManager = MassCtx.GetEntityManager();
	FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(MassCtx.GetEntity());

	const FVector::FReal Dist = FVector::Dist(InstanceData.Input.GetCachedEntityPosition(), TransformFragment->GetTransform().GetLocation());
	
	InstanceData.Output = InstanceData.Input.GetCachedEntityPosition();

	DrawDebugSphere(Context.GetWorld(), InstanceData.Input.GetCachedEntityPosition(), 32.f, 10, FColor::Red, false, 20.f);
}
