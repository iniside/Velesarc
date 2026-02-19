// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPerception.h"
#include "ArcMassPerceptionSystem.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTemplateRegistry.h"
#include "MassExecutionContext.h"
#include "ArcMass/ArcMassSpatialHashSubsystem.h"

TAutoConsoleVariable<bool> CVarArcDebugDrawPerception(
	TEXT("arc.ai.DrawPerception"),
	 false,
	TEXT("Toggles debug drawing for the Mass spatial hash grid (0 = off, 1 = on)"));


UE_DEFINE_GAMEPLAY_TAG(TAG_AI_Perception_Sense_Sight, "AI.Perception.Sense.Sight");
UE_DEFINE_GAMEPLAY_TAG(TAG_AI_Perception_Sense_Hearing, "AI.Perception.Sense.Hearing");

UE_DEFINE_GAMEPLAY_TAG(TAG_AI_Perception_Stimuli_Sight, "AI.Perception.Stimuli.Sight");
UE_DEFINE_GAMEPLAY_TAG(TAG_AI_Perception_Stimuli_Hearing, "AI.Perception.Stimuli.Hearing");

//----------------------------------------------------------------------
// UArcMassPerceptionObserver
//----------------------------------------------------------------------

UArcMassPerceptionObserver::UArcMassPerceptionObserver()
    : ObserverQuery(*this)
{
  //  ObservedType = FArcMassPerceivableTag::StaticStruct();
    ObservedOperations = EMassObservedOperationFlags::Remove;
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
}

void UArcMassPerceptionObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcMassPerceptionObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
   // UWorld* World = EntityManager.GetWorld();
   // if (!World)
   // {
   //     return;
   // }
//
   // UArcMassPerceptionSubsystem* PerceptionSubsystem = World->GetSubsystem<UArcMassPerceptionSubsystem>();
   // if (!PerceptionSubsystem)
   // {
   //     return;
   // }
//
   // TArray<FMassEntityHandle> DestroyedEntities;
//
   // ObserverQuery.ForEachEntityChunk(Context,
   //     [&DestroyedEntities](FMassExecutionContext& Ctx)
   //     {
   //         for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
   //         {
   //             DestroyedEntities.Add(Ctx.GetEntity(EntityIt));
   //         }
   //     });
//
   // if (DestroyedEntities.IsEmpty())
   // {
   //     return;
   // }

    // Clean up sight results
    //{
    //    FMassEntityQuery CleanupQuery;
    //    CleanupQuery.AddRequirement<FArcMassSightPerceptionResult>(EMassFragmentAccess::ReadWrite);
	//
    //    CleanupQuery.ForEachEntityChunk(Context,
    //        [&DestroyedEntities, PerceptionSubsystem](FMassExecutionContext& Ctx)
    //        {
    //            TArrayView<FArcMassSightPerceptionResult> ResultList = Ctx.GetMutableFragmentView<FArcMassSightPerceptionResult>();
	//
    //            for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
    //            {
    //                FArcMassSightPerceptionResult& Result = ResultList[EntityIt];
    //                FMassEntityHandle PerceiverEntity = Ctx.GetEntity(EntityIt);
	//
    //                for (int32 i = Result.PerceivedEntities.Num() - 1; i >= 0; --i)
    //                {
    //                    if (DestroyedEntities.Contains(Result.PerceivedEntities[i].Entity))
    //                    {
    //                        PerceptionSubsystem->BroadcastEntityLostFromPerception(
    //                            PerceiverEntity,
    //                            Result.PerceivedEntities[i].Entity,
    //                            TAG_AI_Perception_Sense_Sight);
	//
    //                        Result.PerceivedEntities.RemoveAtSwap(i);
    //                    }
    //                }
    //            }
    //        });
    //}

    // Clean up hearing results
    //{
    //    FMassEntityQuery CleanupQuery;
    //    CleanupQuery.AddRequirement<FArcMassHearingPerceptionResult>(EMassFragmentAccess::ReadWrite);
	//
    //    CleanupQuery.ForEachEntityChunk(Context,
    //        [&DestroyedEntities, PerceptionSubsystem](FMassExecutionContext& Ctx)
    //        {
    //            TArrayView<FArcMassHearingPerceptionResult> ResultList = Ctx.GetMutableFragmentView<FArcMassHearingPerceptionResult>();
	//
    //            for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
    //            {
    //                FArcMassHearingPerceptionResult& Result = ResultList[EntityIt];
    //                FMassEntityHandle PerceiverEntity = Ctx.GetEntity(EntityIt);
	//
    //                for (int32 i = Result.PerceivedEntities.Num() - 1; i >= 0; --i)
    //                {
    //                    if (DestroyedEntities.Contains(Result.PerceivedEntities[i].Entity))
    //                    {
    //                        PerceptionSubsystem->BroadcastEntityLostFromPerception(
    //                            PerceiverEntity,
    //                            Result.PerceivedEntities[i].Entity,
    //                            TAG_AI_Perception_Sense_Hearing);
	//
    //                        Result.PerceivedEntities.RemoveAtSwap(i);
    //                    }
    //                }
    //            }
    //        });
    //}
}
