// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Fragments/ArcMassItemEventFragment.h"

struct FMassEntityManager;
class UScriptStruct;

namespace ArcMassItems
{
    // RAII transaction for batching item-event emissions on a single
    // (Entity, StoreType) pair. Game-thread only.
    //
    // Outermost transactions:
    //  - On destruction: prune events from prior frames out of the entity's
    //    FArcMassItemEventFragment, append this transaction's pending events
    //    stamped with GFrameCounter, then fire one signal per unique event
    //    kind in this batch.
    //
    // Nested transactions (same Entity + StoreType already has an outer):
    //  - Forward Emit() to the outer transaction. Destructor is a no-op.
    //
    // Nested transactions on a different (Entity, StoreType) become their
    // own outermost transactions. MoveItem-between-entities is two
    // independent outers; commits are NOT atomic across entities.
    class ARCMASSITEMSRUNTIME_API FArcMassItemTransaction
    {
    public:
        FArcMassItemTransaction(
            FMassEntityManager& InEntityManager,
            FMassEntityHandle InEntity,
            const UScriptStruct* InStoreType);

        ~FArcMassItemTransaction();

        void Emit(FArcMassItemEvent Event);

        FArcMassItemTransaction(const FArcMassItemTransaction&) = delete;
        FArcMassItemTransaction& operator=(const FArcMassItemTransaction&) = delete;
        FArcMassItemTransaction(FArcMassItemTransaction&&) = delete;
        FArcMassItemTransaction& operator=(FArcMassItemTransaction&&) = delete;

    private:
        FMassEntityManager& EntityManager;
        FMassEntityHandle Entity;
        const UScriptStruct* StoreType = nullptr;

        // Outermost-only state.
        TArray<FArcMassItemEvent> Pending;
        bool bIsOutermost = false;
        FArcMassItemTransaction* OuterPtr = nullptr;
    };
}
