// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/PlacedEntities/ArcLinkableGuidSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcLinkableGuidSubsystem)

FMassEntityHandle UArcLinkableGuidSubsystem::ResolveGuid(const FGuid& Guid) const
{
    const FMassEntityHandle* Found = GuidToEntityMap.Find(Guid);
    return Found ? *Found : FMassEntityHandle();
}

void UArcLinkableGuidSubsystem::RegisterEntity(const FGuid& Guid, FMassEntityHandle Handle)
{
    GuidToEntityMap.Add(Guid, Handle);
}

void UArcLinkableGuidSubsystem::UnregisterEntity(const FGuid& Guid)
{
    GuidToEntityMap.Remove(Guid);
}
