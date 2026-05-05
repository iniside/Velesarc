// Copyright Lukasz Baran. All Rights Reserved.

// Note: file kept at legacy path "ArcMassNetIdFragment.h"; the type was renamed in
// Phase 6 of the per-entity Iris replication refactor (NetId int32 -> NetHandle FNetRefHandle).

#pragma once

#include "CoreMinimal.h"
#include "Iris/ReplicationSystem/NetRefHandle.h"
#include "MassEntityTypes.h"
#include "ArcMassNetIdFragment.generated.h"

/**
 * Stores the FNetRefHandle assigned to a Mass entity's UArcMassEntityVessel
 * by UArcMassEntityReplicationSubsystem::RegisterEntity. Written by the start
 * observer (UArcMassEntityReplicationStartObserver) and consumed by the
 * FArcMassEntityHandleNetSerializer to translate FMassEntityHandle <-> vessel
 * references on the wire.
 *
 * FNetRefHandle is plain bits (no UObject pointers / containers), so the
 * fragment remains trivially copyable and needs no UPROPERTY reflection
 * (the type is not a USTRUCT) or TMassFragmentTraits specialization.
 */
USTRUCT()
struct ARCMASSREPLICATIONRUNTIME_API FArcMassEntityNetHandleFragment : public FMassFragment
{
	GENERATED_BODY()

	UE::Net::FNetRefHandle NetHandle;
};
