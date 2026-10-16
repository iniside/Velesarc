// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Mass/EntityHandle.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemId.h"
#include "ArcMassQuickBarAutoSelectSubsystem.generated.h"

struct FMassEntityManager;
class UArcMassItemEventSubsystem;

/**
 * UArcMassQuickBarAutoSelectSubsystem
 *
 * Thin delegate adapter: consumes UArcMassItemEventSubsystem's generic
 * OnItemSlotChanged rail (which the drain fires for assign, clear, and move
 * alike — same pattern as UArcMassItemAttachmentSubsystem) and forwards to
 * ArcMassItems::QuickBar::AutoSelect::HandleSlotChanged. Replaces the
 * signal-driven UArcMassQuickBarAutoSelectProcessor, which iterated
 * FArcMassItemEventFragment::Events while its downstream handlers could
 * mutate that same array mid-iteration.
 */
UCLASS()
class ARCMASSITEMSRUNTIME_API UArcMassQuickBarAutoSelectSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	bool EnsureCached();

	FMassEntityManager* EntityManager = nullptr;
	UArcMassItemEventSubsystem* EventSubsystem = nullptr;

	FDelegateHandle HandleSlotChanged;

	void OnItemSlotChanged(FMassEntityHandle Entity, FArcItemId ItemId, FGameplayTag NewSlot, const UScriptStruct* StoreType);
};
