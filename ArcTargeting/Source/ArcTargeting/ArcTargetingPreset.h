#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcTargetingPreset.generated.h"

/**
 * Asset describing a complete targeting pipeline.
 *
 * One ordered list of nodes; each node reads/writes the shared Candidates array;
 * order in the array is execution order. The BaseStruct meta filter constrains
 * entries to FArcTargetingNode subtypes. Runtime is uniform — no per-phase loops,
 * no phase concept at all.
 */
UCLASS(BlueprintType)
class ARCTARGETING_API UArcTargetingPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Pipeline",
		meta = (BaseStruct = "/Script/ArcTargeting.ArcTargetingNode"))
	TArray<FInstancedStruct> Nodes;

#if WITH_EDITOR
	virtual void PostLoad() override;
#endif
};
