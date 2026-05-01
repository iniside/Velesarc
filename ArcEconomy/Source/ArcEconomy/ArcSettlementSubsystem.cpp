// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSettlementSubsystem.h"
#include "Mass/ArcEconomyFragments.h"
#include "Mass/EntityFragments.h"
#include "MassEntityQuery.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"

const TArray<FMassEntityHandle> UArcSettlementSubsystem::EmptyHandleArray;

void UArcSettlementSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UArcSettlementSubsystem::Deinitialize()
{
    SettlementToBuildings.Empty();
    SettlementToNPCs.Empty();
    NPCDelegates.Empty();
    Super::Deinitialize();
}

void UArcSettlementSubsystem::RegisterBuilding(FMassEntityHandle SettlementHandle, FMassEntityHandle BuildingHandle)
{
    SettlementToBuildings.FindOrAdd(SettlementHandle).AddUnique(BuildingHandle);
}

void UArcSettlementSubsystem::UnregisterBuilding(FMassEntityHandle SettlementHandle, FMassEntityHandle BuildingHandle)
{
    TArray<FMassEntityHandle>* Buildings = SettlementToBuildings.Find(SettlementHandle);
    if (Buildings)
    {
        Buildings->RemoveSingle(BuildingHandle);
    }
}

const TArray<FMassEntityHandle>& UArcSettlementSubsystem::GetBuildingsForSettlement(FMassEntityHandle SettlementHandle) const
{
    const TArray<FMassEntityHandle>* Buildings = SettlementToBuildings.Find(SettlementHandle);
    return Buildings ? *Buildings : EmptyHandleArray;
}

void UArcSettlementSubsystem::RegisterNPC(FMassEntityHandle SettlementHandle, FMassEntityHandle NPCHandle)
{
    SettlementToNPCs.FindOrAdd(SettlementHandle).AddUnique(NPCHandle);
}

void UArcSettlementSubsystem::UnregisterNPC(FMassEntityHandle SettlementHandle, FMassEntityHandle NPCHandle)
{
    TArray<FMassEntityHandle>* NPCs = SettlementToNPCs.Find(SettlementHandle);
    if (NPCs)
    {
        NPCs->RemoveSingle(NPCHandle);
    }
}

const TArray<FMassEntityHandle>& UArcSettlementSubsystem::GetNPCsForSettlement(FMassEntityHandle SettlementHandle) const
{
    const TArray<FMassEntityHandle>* NPCs = SettlementToNPCs.Find(SettlementHandle);
    return NPCs ? *NPCs : EmptyHandleArray;
}

FArcEconomyNPCDelegates& UArcSettlementSubsystem::GetOrCreateNPCDelegates(FMassEntityHandle NPCHandle)
{
    return NPCDelegates.FindOrAdd(NPCHandle);
}

FArcEconomyNPCDelegates* UArcSettlementSubsystem::FindNPCDelegates(FMassEntityHandle NPCHandle)
{
    return NPCDelegates.Find(NPCHandle);
}

void UArcSettlementSubsystem::RemoveNPCDelegates(FMassEntityHandle NPCHandle)
{
    NPCDelegates.Remove(NPCHandle);
}

FMassEntityHandle UArcSettlementSubsystem::FindNearestSettlement(FMassEntityManager& EntityManager, const FVector& Location) const
{
    FMassEntityHandle NearestHandle;
    float NearestDistSq = TNumericLimits<float>::Max();

    FMassEntityQuery SettlementQuery(EntityManager.AsShared());
    SettlementQuery.AddRequirement<FArcSettlementFragment>(EMassFragmentAccess::ReadOnly);
    SettlementQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);

    FMassExecutionContext TempContext(EntityManager, 0.0f);
    SettlementQuery.ForEachEntityChunk(TempContext, [&Location, &NearestHandle, &NearestDistSq](FMassExecutionContext& Ctx)
    {
        const TConstArrayView<FArcSettlementFragment> Settlements = Ctx.GetFragmentView<FArcSettlementFragment>();
        const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
        const int32 NumEntities = Ctx.GetNumEntities();

        for (int32 Index = 0; Index < NumEntities; ++Index)
        {
            const FVector SettlementPos = Transforms[Index].GetTransform().GetLocation();
            const float RadiusSq = FMath::Square(Settlements[Index].SettlementRadius);
            const float DistSq = FVector::DistSquared(Location, SettlementPos);

            if (DistSq <= RadiusSq && DistSq < NearestDistSq)
            {
                NearestDistSq = DistSq;
                NearestHandle = Ctx.GetEntity(Index);
            }
        }
    });

    return NearestHandle;
}
