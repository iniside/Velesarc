// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Mass/EntityHandle.h"
#include "ArcLinkableGuidSubsystem.generated.h"

/** World subsystem that indexes FArcLinkableGuidFragment GUIDs to entity handles at runtime. */
UCLASS()
class ARCMASS_API UArcLinkableGuidSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** Resolve a LinkGuid to the entity handle that owns it. Returns an invalid handle if not found. */
    FMassEntityHandle ResolveGuid(const FGuid& Guid) const;

    /** Register a GUID-to-handle mapping. Called by the observer processor. */
    void RegisterEntity(const FGuid& Guid, FMassEntityHandle Handle);

    /** Remove a GUID mapping. Called by the deinit observer processor. */
    void UnregisterEntity(const FGuid& Guid);

private:
    TMap<FGuid, FMassEntityHandle> GuidToEntityMap;
};
