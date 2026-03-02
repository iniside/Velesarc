#include "ArcMassObserveDistanceChanged.h"

#include "MassSignalSubsystem.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "Containers/Ticker.h"
#include "GameFramework/Actor.h"

FArcMassObserveDistanceChangedTask::FArcMassObserveDistanceChangedTask()
{
}

bool FArcMassObserveDistanceChangedTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(TransformFragment);
	
	return true;
}

void FArcMassObserveDistanceChangedTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<FTransformFragment>();
}

EStateTreeRunStatus FArcMassObserveDistanceChangedTask::EnterState(FStateTreeExecutionContext& Context
																   , const FStateTreeTransitionResult& Transition) const
{
	FTransformFragment& Transform = Context.GetExternalData(TransformFragment);
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	if (InstanceData.TargetActor)
	{
		InstanceData.TickDelegate = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
			[Transform, Target = InstanceData.TargetActor, Distance = InstanceData.DistanceThreshold
				, WeakContext = Context.MakeWeakExecutionContext(), Entity = MassCtx.GetEntity(), SignalSubsystem](float DeltaTime)
			{
				const FStateTreeStrongExecutionContext StrongCtx = WeakContext.MakeStrongExecutionContext();
				FInstanceDataType* InstanceDataPtr = StrongCtx.GetInstanceDataPtr<FInstanceDataType>();
				if (!InstanceDataPtr)
				{
					return false;
				}
					
				if (!Target)
				{
					return false;
				}
				
				const FVector TargetLocation = Target->GetActorLocation();
					
				float TargetDistance = 0;
				if (InstanceDataPtr->bUseCompareLocation)
				{
					TargetDistance = FVector::Dist(TargetLocation, InstanceDataPtr->CompareLocation);
				}
				else
				{
					TargetDistance = FVector::Dist(Transform.GetTransform().GetLocation(), TargetLocation);
				}
				
				if (TargetDistance > Distance)
				{
					StrongCtx.BroadcastDelegate(InstanceDataPtr->OnDistanceChanged);
					SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
				}
				return true;
			}));
	}
	else if (InstanceData.TargetEntity.GetEntityHandle().IsValid())
	{
		const FTransformFragment* TargetTransform = MassCtx.GetEntityManager().GetFragmentDataPtr<FTransformFragment>(InstanceData.TargetEntity.GetEntityHandle());
		if (TargetTransform)
		{
			InstanceData.TickDelegate = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
				[Transform, TargetTransform, Distance = InstanceData.DistanceThreshold
					, WeakContext = Context.MakeWeakExecutionContext(), Entity = MassCtx.GetEntity(), SignalSubsystem](float DeltaTime)
				{
					const FStateTreeStrongExecutionContext StrongCtx = WeakContext.MakeStrongExecutionContext();
					FInstanceDataType* InstanceDataPtr = StrongCtx.GetInstanceDataPtr<FInstanceDataType>();
					if (!InstanceDataPtr)
					{
						return false;
					}
					
					if (!TargetTransform)
					{
						return false;
					}
				
					float TargetDistance = 0;
					if (InstanceDataPtr->bUseCompareLocation)
					{
						TargetDistance = FVector::Dist(TargetTransform->GetTransform().GetLocation(), InstanceDataPtr->CompareLocation);
					}
					else
					{
						TargetDistance = FVector::Dist(Transform.GetTransform().GetLocation(), TargetTransform->GetTransform().GetLocation());
					}
						
					if (TargetDistance > Distance)
					{
						StrongCtx.BroadcastDelegate(InstanceDataPtr->OnDistanceChanged);
						SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
					}
					return true;
				}));
		}
	}
	return EStateTreeRunStatus::Running;
}

void FArcMassObserveDistanceChangedTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	FTSTicker::RemoveTicker(InstanceData.TickDelegate);
}

#if WITH_EDITOR
FText FArcMassObserveDistanceChangedTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		return FText::Format(NSLOCTEXT("ArcAI", "ObserveDistanceDesc", "Observe Distance Changed (Threshold={0})"), FText::AsNumber(InstanceData->DistanceThreshold));
	}
	return FMassStateTreeTaskBase::GetDescription(ID, InstanceDataView, BindingLookup, Formatting);
}
#endif
