// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Effects/ArcEffectTask.h"
#include "MovementModifier.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcEffectTask_Mover.generated.h"

class UMoverComponent;
struct FArcActiveEffect;

USTRUCT(BlueprintType, meta = (DisplayName = "Persistent Root / Speed Modifier"))
struct ARCABILITYKIT_API FArcEffectTask_PersistentMoverModifier : public FArcEffectTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Mover", meta = (BaseStruct = "/Script/Mover.MovementModifierBase", ExcludeBaseStruct))
	FInstancedStruct Modifier;

	FGuid SourceId;
	TWeakObjectPtr<UMoverComponent> MoverComponent;
	FMovementModifierHandle ModifierHandle;

	virtual void OnApplication(FArcActiveEffect& OwningEffect) override;
	virtual void OnRemoved(FArcActiveEffect& OwningEffect, EArcEffectRemovalReason Reason) override;
	virtual void OnExecute(FArcActiveEffect& OwningEffect) override;
	virtual void OnInhibitionChanged(FArcActiveEffect& OwningEffect, bool bInhibited) override;

private:
	void QueueOwnedModifier(FArcActiveEffect& OwningEffect);
	void CancelOwnedModifier();
};

USTRUCT(BlueprintType, meta = (DisplayName = "Queue Mover Modifier"))
struct ARCABILITYKIT_API FArcEffectTask_QueueMoverModifier : public FArcEffectTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Mover", meta = (BaseStruct = "/Script/Mover.MovementModifierBase", ExcludeBaseStruct))
	FInstancedStruct Modifier;

	virtual void OnExecute(FArcActiveEffect& OwningEffect) override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Queue Mover Layered Move"))
struct ARCABILITYKIT_API FArcEffectTask_QueueMoverLayeredMove : public FArcEffectTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Mover", meta = (BaseStruct = "/Script/Mover.LayeredMoveBase", ExcludeBaseStruct))
	FInstancedStruct LayeredMove;

	virtual void OnExecute(FArcActiveEffect& OwningEffect) override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Queue Mover Instant Effect"))
struct ARCABILITYKIT_API FArcEffectTask_QueueMoverInstantEffect : public FArcEffectTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Mover", meta = (BaseStruct = "/Script/Mover.InstantMovementEffect", ExcludeBaseStruct))
	FInstancedStruct InstantEffect;

	virtual void OnExecute(FArcActiveEffect& OwningEffect) override;
};
