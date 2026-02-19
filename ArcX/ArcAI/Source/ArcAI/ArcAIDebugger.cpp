#include "ArcAIDebugger.h"

#include "DrawDebugHelpers.h"
#include "MassEntityFragments.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "MassGameplayDebugTypes.h"
#include "SlateIM.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "NeedsSystem/ArcNeedsFragment.h"
#include "Perception/ArcMassPerception.h"
#include "Perception/ArcMassSightPerception.h"
#include "Tasks/ArcMassSelectApproachSlotTask.h"

namespace ArcAI
{
}
void FArcAIDebuggerWidget::Draw()
{
	SlateIM::AutoSize();
	SlateIM::HAlign(HAlign_Left);
	SlateIM::VAlign(VAlign_Top);
	if (SlateIM::Button(TEXT("Select Entity")))
	{
		
	}
	
	bool bShowSpatialHashLast = bShowSpatialHash;
	SlateIM::CheckBox(TEXT("Show Spatial Hash"), bShowSpatialHash);
	
	if (bShowSpatialHashLast != bShowSpatialHash)
	{
		IConsoleVariable* DebugDrawSpatialHash = IConsoleManager::Get().FindConsoleVariable(TEXT("arc.mass.DebugDrawSpatialHash"));
		DebugDrawSpatialHash->Set(bShowSpatialHash);	
	}
	
	bool bShowPerceptionLast = bShowPerception;
	SlateIM::CheckBox(TEXT("Show Perception"), bShowPerception);
	
	if (bShowPerceptionLast != bShowPerception)
	{
		IConsoleVariable* DebugDrawPerception = IConsoleManager::Get().FindConsoleVariable(TEXT("arc.ai.DrawPerception"));
		DebugDrawPerception->Set(bShowPerception);	
	}
	
	if (!World.IsValid())
	{
		return;
	}
	
	APlayerController* PC = nullptr;
	UGameInstance* GI = World.Get()->GetGameInstance();
	const TArray<ULocalPlayer*>& LocalPlayers = GI->GetLocalPlayers();
	if (LocalPlayers.Num() > 0)
	{
		PC = LocalPlayers[0]->GetPlayerController(World.Get());
	}
	
	if (!PC)
	{
		return;
		
	}
	
	APawn* P = PC->GetPawn();
	if (!P)
	{
		return;
	}
	
	UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MES)
	{
		return;
	}
	
	SlateIM::CheckBox(TEXT("Show Dynamic Slots"), bShowDynamicSlots);
	if (bShowDynamicSlots)
	{
		UArcDynamicSlotComponent* DynamicSlotComp = P->FindComponentByClass<UArcDynamicSlotComponent>();
		if (DynamicSlotComp)
		{
			for (const FArcDynamicSlotInfo& Slot : DynamicSlotComp->Slots)
			{
				FVector WorldLocation = P->GetTransform().TransformPosition(Slot.SlotLocation.GetLocation());
				DrawDebugSphere(World.Get(), WorldLocation, 20.f, 8, FColor::Blue, false, -1.f, 0, 1.f);
			}
		}
	}
	FMassEntityManager& EntityManager = MES->GetMutableEntityManager();
	float Distance = 0;
	if (CurrentlySelectedEntity >= 0 && CurrentlySelectedEntity < Entities.Num())
	{
		FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entities[CurrentlySelectedEntity]);
		if (TransformFragment)
		{
			FVector EntityLocation = TransformFragment->GetTransform().GetLocation();
			FVector PlayerLocation = P->GetActorLocation();
			Distance = FVector::Dist(EntityLocation, PlayerLocation);	
		}
	}
	
	SlateIM::Text(FString::Printf(TEXT("Distance to selected entity: %.2f"), Distance));
	
	SlateIM::Fill();
	SlateIM::HAlign(HAlign_Fill);
	SlateIM::VAlign(VAlign_Fill);
	SlateIM::BeginTabGroup(TEXT("ExampleContent"));
		SlateIM::BeginTabStack();
		if (SlateIM::BeginTab(TEXT("Basics")))
		{	
			SlateIM::BeginVerticalStack();
			SlateIM::HAlign(HAlign_Left);
			SlateIM::VAlign(VAlign_Top);
			SlateIM::Text(TEXT("Arc AI Debugger - Basics Tab"));
			DrawEntityList();
			
			SlateIM::HAlign(HAlign_Fill);
			SlateIM::VAlign(VAlign_Fill);
			
			if (CurrentlySelectedEntity != INDEX_NONE)
			{
				if (const FArcMassSightPerceptionResult* PerceptionSenseResult = EntityManager.GetFragmentDataPtr<FArcMassSightPerceptionResult>(Entities[CurrentlySelectedEntity]))
				{
					SlateIM::Text(FString::Printf(TEXT("Sight Perceived Entities: %d"), PerceptionSenseResult->PerceivedEntities.Num()));
					for (const FArcPerceivedEntity& PE : PerceptionSenseResult->PerceivedEntities)
					{
						if (const FMassActorFragment* TargetActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(PE.Entity))
						{
							SlateIM::Text(FString::Printf(TEXT(" - Entity: %s, Actor: %sf"), *PE.Entity.DebugGetDescription(), *GetNameSafe(TargetActorFragment->Get())));
							if (TargetActorFragment->Get())
							{
								DrawDebugSphere(World.Get(), TargetActorFragment->Get()->GetActorLocation(), 30.f, 8, FColor::Green, false, -1.f, 0, 1.f);		
							}
						}
						SlateIM::Text(FString::Printf(TEXT(" - Entity: %s, Distance: %.2f"), *PE.Entity.DebugGetDescription(), PE.Distance));
						SlateIM::Text(FString::Printf(TEXT(" - Entity: %s, Strength: %.2f"), *PE.Entity.DebugGetDescription(), PE.Strength));
						SlateIM::Text(FString::Printf(TEXT(" - Entity: %s, TimeSinceLastPerceived: %.2f"), *PE.Entity.DebugGetDescription(), PE.TimeSinceLastPerceived));
												
						DrawDebugSphere(World.Get(), PE.LastKnownLocation, 30.f, 8, FColor::Yellow, false, -1.f, 0, 1.f);
					}
				}
				else
				{
					SlateIM::Text(TEXT("No Perception Data"));
				}
				
				SlateIM::Text(TEXT("Needs: "));
				if (const FArcHungerNeedFragment* HungerNeedFragment = EntityManager.GetFragmentDataPtr<FArcHungerNeedFragment>(Entities[CurrentlySelectedEntity]))
				{
					SlateIM::Text(FString::Printf(TEXT(" - Hunger Need: %.2f ChangeRate %.2f"), HungerNeedFragment->CurrentValue, HungerNeedFragment->ChangeRate));
				}
				if (const FArcThirstNeedFragment* ThirstNeedFragment = EntityManager.GetFragmentDataPtr<FArcThirstNeedFragment>(Entities[CurrentlySelectedEntity]))
				{
					SlateIM::Text(FString::Printf(TEXT(" - Thirst Need: %.2f ChangeRate %.2f"), ThirstNeedFragment->CurrentValue, ThirstNeedFragment->ChangeRate));
				}
				if (const FArcFatigueNeedFragment* FatigueNeedFragment = EntityManager.GetFragmentDataPtr<FArcFatigueNeedFragment>(Entities[CurrentlySelectedEntity]))
				{
					SlateIM::Text(FString::Printf(TEXT(" - Fatigue Need: %.2f ChangeRate %.2f"), FatigueNeedFragment->CurrentValue, FatigueNeedFragment->ChangeRate));
				}
			}
			SlateIM::EndVerticalStack();
		}
		SlateIM::EndTab();
		
		SlateIM::EndTabStack();
	SlateIM::EndTabGroup();
}

void FArcAIDebuggerWidget::DrawEntityList()
{
	if (!World.IsValid())
	{
		return;
	}
		
	UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>();
	FMassEntityManager& EntityManager = MES->GetMutableEntityManager();
	
	if (bRefreshEntityList)
	{
		FMassExecutionContext ExecutionContext(EntityManager);
		FMassEntityQuery Query(EntityManager.AsShared());
		Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
		Query.ForEachEntityChunk(ExecutionContext, [this](FMassExecutionContext& Context)
		{
			Entities.Append(Context.GetEntities().GetData(), Context.GetEntities().Num());
			TConstArrayView<FTransformFragment> InLocations = Context.GetFragmentView<FTransformFragment>();
			Locations.Reserve(Locations.Num() + InLocations.Num());
			for (const FTransformFragment& TransformFragment : InLocations)
			{
				Locations.Add(TransformFragment.GetTransform().GetLocation());
			}
		});
		
		
		EntityNames.Reserve(Entities.Num());
	
		for (int32 Idx = 0; Idx < Entities.Num(); ++Idx)
		{
			EntityNames.Add(Entities[Idx].DebugGetDescription());
		}
		
		bRefreshEntityList = false;
	}
	
	for (int32 Idx = 0; Idx < Entities.Num(); ++Idx)
	{
		FTransformFragment& TransformFragment = EntityManager.GetFragmentDataChecked<FTransformFragment>(Entities[Idx]);
		DrawDebugString(World.Get(), TransformFragment.GetTransform().GetLocation(), Entities[Idx].DebugGetDescription(), nullptr, FColor::Red, -1.f, false);
	}
	
		
	if (EntityNames.Num() > 0)
	{
		SlateIM::SelectionList(EntityNames, CurrentlySelectedEntity, false);
		if (CurrentlySelectedEntity >= 0 && CurrentlySelectedEntity < Entities.Num())
		{
			FTransformFragment& TransformFragment = EntityManager.GetFragmentDataChecked<FTransformFragment>(Entities[CurrentlySelectedEntity]);
			DrawDebugCapsule(World.Get(), TransformFragment.GetTransform().GetLocation(), 50.f, 40.f, FQuat::Identity, FColor::Green, false, -1.f, 0, 1.f);
		}
	}
}

void FArcAIDebuggerWidget::OnPieEnd()
{
	World = nullptr;
	Entities.Reset();
	Locations.Reset();
	EntityNames.Reset();
	CurrentlySelectedEntity = INDEX_NONE;
}

void FArcAIDebuggerWindowWidget::DrawWindow(float DeltaTime)
{
	DebuggerWidget.Draw();
}
