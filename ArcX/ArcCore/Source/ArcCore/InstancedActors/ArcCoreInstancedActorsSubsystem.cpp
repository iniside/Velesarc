// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCoreInstancedActorsSubsystem.h"

#include "InstancedActorsVisualizationProcessor.h"
#include "MassRepresentationFragments.h"
#include "Algo/NoneOf.h"
#include "EngineUtils.h"
#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityTraitBase.h"
#include "SmartObjectSubsystem.h"
#include "UObject/UObjectBase.h"

AArcCoreInstancedActorsManager::AArcCoreInstancedActorsManager()
{
	InstancedActorsDataClass = UArcCoreInstancedActorsData::StaticClass();
}

UInstancedActorsData* AArcCoreInstancedActorsManager::CreateNextInstanceActorData(TSubclassOf<AActor> ActorClass
	, const FInstancedActorsTagSet& AdditionalInstanceTags)
{
	//UArcCoreInstancedActorsSubsystem* Subsystem = GetWorld()->GetSubsystem<UArcCoreInstancedActorsSubsystem>();
	UArcCoreInstancedActorsSettings* Settings = GetMutableDefault<UArcCoreInstancedActorsSettings>();
	if (Settings)
	{
		if (TSubclassOf<UInstancedActorsData>* IADClass = Settings->ActorToDataClassMap.Find(ActorClass))
		{
			TSubclassOf<UInstancedActorsData> C = *IADClass;
			UClass* DD = nullptr;
			
			const FString InstanceDataNameStr = FString::Printf(TEXT("%s_%s"), *C->GetFName().ToString(), *ActorClass->GetFName().ToString());
			const FName UniqueName = MakeUniqueObjectName(this, C, FName(InstanceDataNameStr));

			UInstancedActorsData* NewInstanceData = NewObject<UInstancedActorsData>(this, C, UniqueName);

			// @todo it's conceivable the NextInstanceDataID will overflow. We need to use some handle system in place instead. 
			NewInstanceData->ID = NextInstanceDataID++;
			NewInstanceData->ActorClass = ActorClass;
			//NewInstanceData->AdditionalTags = AdditionalInstanceTags;

			check(Algo::NoneOf(PerActorClassInstanceData, [NewInstanceData](UInstancedActorsData* InstanceData)
				{
					return InstanceData->ID == NewInstanceData->ID;
				}));

			return NewInstanceData;	
		}
		
	}
	
	return Super::CreateNextInstanceActorData(ActorClass, AdditionalInstanceTags);
}

AActor* AArcCoreInstancedActorsManager::ArcFindActor(const FActorInstanceHandle& Handle)
{
	return FindActor(Handle);
}

void UArcMassSmartObjectOwnerTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	BuildContext.AddFragment<FArcSmartObjectOwnerFragment>();

	const FConstSharedStruct SmartObjectDefFragment = EntityManager.GetOrCreateConstSharedFragment(SmartObjectDefinition);
	BuildContext.AddConstSharedFragment(SmartObjectDefFragment);
}

void UArcCoreInstancedActorsData::ApplyInstanceDelta(FMassEntityManager& EntityManager, const FInstancedActorsDelta& InstanceDelta
													 , TArray<FInstancedActorsInstanceIndex>& OutEntitiesToRemove)
{
	Super::ApplyInstanceDelta(EntityManager, InstanceDelta, OutEntitiesToRemove);

	FMassEntityHandle Handle = GetEntityHandleForIndex(InstanceDelta.GetInstanceIndex());
	if (InstanceDelta.GetCurrentLifecyclePhaseIndex() == 5)
	{
	}
}

void UArcCoreInstancedActorsData::SwitchInstanceVisualizationWithContext(FMassExecutionContext& Context
	, FInstancedActorsInstanceIndex InstanceToSwitch, uint8 NewVisualizationIndex)
{
	if (!ensure(InstanceVisualizations.IsValidIndex(NewVisualizationIndex)) || !ensure(Entities.IsValidIndex(InstanceToSwitch.GetIndex())))
	{
		return;
	}

	FMassEntityManager& MassEntityManager = GetMassEntityManagerChecked();
	const FMassEntityHandle& EntityHandle = Entities[InstanceToSwitch.GetIndex()];
	if (!ensure(MassEntityManager.IsEntityValid(EntityHandle)))
	{
		return;
	}

	const FInstancedActorsVisualizationInfo& NewVisualization = InstanceVisualizations[NewVisualizationIndex];

	FInstancedActorsMeshSwitchFragment MeshSwitchFragment;
	MeshSwitchFragment.NewStaticMeshDescHandle = NewVisualization.MassStaticMeshDescHandle;

	Context.Defer().PushCommand<FMassCommandAddFragmentInstances>(EntityHandle, MeshSwitchFragment);
}

void UArcCoreInstancedActorsData::OnSpawnEntities()
{
	Super::OnSpawnEntities();

	FMassEntityManager& MassEntityManager = GetMassEntityManagerChecked();
	USmartObjectSubsystem* SmartObjectSubsystem = MassEntityManager.GetWorld()->GetSubsystem<USmartObjectSubsystem>();

	for (const FMassEntityHandle& Handle : Entities)
	{
		FArcSmartObjectOwnerFragment* SmartObjectOwnerFragment = MassEntityManager.GetFragmentDataPtr<FArcSmartObjectOwnerFragment>(Handle);
		FArcSmartObjectDefinitionSharedFragment* SmartObjectDefinitionFragment = MassEntityManager.GetConstSharedFragmentDataPtr<FArcSmartObjectDefinitionSharedFragment>(Handle);
		FTransformFragment* TransformFragment = MassEntityManager.GetFragmentDataPtr<FTransformFragment>(Handle);
		if (!SmartObjectDefinitionFragment || !SmartObjectDefinitionFragment->SmartObjectDefinition)
		{
			continue;
		}
		if (!TransformFragment)
		{
			continue;
		}
		if (!SmartObjectOwnerFragment)
		{
			continue;
		}

		SmartObjectOwnerFragment->SmartObjectHandle = SmartObjectSubsystem->CreateSmartObject(*SmartObjectDefinitionFragment->SmartObjectDefinition
			, TransformFragment->GetTransform(), FConstStructView::Make(Handle));
	}
}

UArcCoreInstancedActorsSubsystem::UArcCoreInstancedActorsSubsystem()
{
	InstancedActorsManagerClass = AArcCoreInstancedActorsManager::StaticClass();
}

void UArcGameFeatureAction_RegisterActorToInstanceData::OnGameFeatureRegistering()
{
	for (const TPair<TSubclassOf<AActor>, TSubclassOf<UInstancedActorsData>>& Pair : ActorToDataClassMap)
	{
		GetMutableDefault<UArcCoreInstancedActorsSettings>()->ActorToDataClassMap.FindOrAdd(Pair.Key) = Pair.Value;	
	}
	
}

void UArcGameFeatureAction_RegisterActorToInstanceData::OnGameFeatureUnregistering()
{
	for (const TPair<TSubclassOf<AActor>, TSubclassOf<UInstancedActorsData>>& Pair : ActorToDataClassMap)
	{
		GetMutableDefault<UArcCoreInstancedActorsSettings>()->ActorToDataClassMap.Remove(Pair.Key);	
	}
}