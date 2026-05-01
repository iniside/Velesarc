#pragma once
#include "AISystem.h"
#include "MassStateTreeTypes.h"
#include "AITypes.h"
#include "MassProcessor.h"
#include "MassActorSubsystem.h"
#include "MassNavigationFragments.h"
#include "MassTranslator.h"

#include "ArcMassDrawDebugSphereTask.generated.h"

class AAIController;
class AActor;
class UNavigationQueryFilter;
class UAITask_MoveTo;
class IGameplayTaskOwnerInterface;

/** Instance data for FArcMassDrawDebugSphereTask. Holds the debug sphere location, radius, and display duration. */
USTRUCT()
struct FArcMassDrawDebugSphereTaskInstanceData
{
	GENERATED_BODY()

	/** World-space location at which to draw the debug sphere. */
	UPROPERTY(EditAnywhere, Category = Input)
	FVector Location = FVector::ZeroVector;

	/** Radius of the debug sphere in world units. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Radius = 20.f;

	/** How long (in seconds) the debug sphere persists in the world. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Duration = 20.f;
};

/**
 * Instant StateTree task that draws a debug sphere at a given location.
 * EnterState draws the sphere and immediately returns Succeeded. Useful for visualizing
 * positions during StateTree debugging.
 */
USTRUCT(meta = (DisplayName = "Arc Mass Draw Debug Sphere", Category = "AI|Action", ToolTip = "Instant debug task that draws a sphere at the specified location and immediately succeeds."))
struct FArcMassDrawDebugSphereTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
	
public:
	using FInstanceDataType = FArcMassDrawDebugSphereTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FArcMassDrawDebugSphereTask();

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};