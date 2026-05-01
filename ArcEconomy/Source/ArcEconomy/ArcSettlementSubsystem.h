// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityTypes.h"
#include "ArcSettlementSubsystem.generated.h"

struct FMassEntityManager;
struct FTransformFragment;
struct FArcSettlementFragment;

DECLARE_MULTICAST_DELEGATE(FArcOnNPCRoleChanged);

struct FArcEconomyNPCDelegates
{
    FArcOnNPCRoleChanged OnRoleChanged;
};

/**
 * World subsystem providing economy infrastructure.
 * Maintains indexes of settlement -> building/NPC entity handles.
 */
UCLASS()
class ARCECONOMY_API UArcSettlementSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ---- Building Index ----

    void RegisterBuilding(FMassEntityHandle SettlementHandle, FMassEntityHandle BuildingHandle);
    void UnregisterBuilding(FMassEntityHandle SettlementHandle, FMassEntityHandle BuildingHandle);
    const TArray<FMassEntityHandle>& GetBuildingsForSettlement(FMassEntityHandle SettlementHandle) const;

    // ---- NPC Index ----

    void RegisterNPC(FMassEntityHandle SettlementHandle, FMassEntityHandle NPCHandle);
    void UnregisterNPC(FMassEntityHandle SettlementHandle, FMassEntityHandle NPCHandle);
    const TArray<FMassEntityHandle>& GetNPCsForSettlement(FMassEntityHandle SettlementHandle) const;

    // ---- Spatial Discovery ----

    /**
     * Finds the nearest settlement entity whose FTransformFragment position is within
     * SettlementRadius of the given location. Returns invalid handle if none found.
     */
    FMassEntityHandle FindNearestSettlement(FMassEntityManager& EntityManager, const FVector& Location) const;

    // ---- NPC Delegates ----

    FArcEconomyNPCDelegates& GetOrCreateNPCDelegates(FMassEntityHandle NPCHandle);
    FArcEconomyNPCDelegates* FindNPCDelegates(FMassEntityHandle NPCHandle);
    void RemoveNPCDelegates(FMassEntityHandle NPCHandle);

    // ---- Trade Config ----

    /** Global scaling factor for caravan travel cost. TravelCost = Distance * TravelCostScale. */
    UPROPERTY(EditAnywhere, Category = "Trade")
    float TravelCostScale = 0.01f;

private:
    TMap<FMassEntityHandle, TArray<FMassEntityHandle>> SettlementToBuildings;
    TMap<FMassEntityHandle, TArray<FMassEntityHandle>> SettlementToNPCs;
    TMap<FMassEntityHandle, FArcEconomyNPCDelegates> NPCDelegates;

    static const TArray<FMassEntityHandle> EmptyHandleArray;
};
