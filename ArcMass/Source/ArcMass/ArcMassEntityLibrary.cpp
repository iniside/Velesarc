// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEntityLibrary.h"

#include "ArcMass/Physics/ArcMassPhysicsEntityLink.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ArcMass/Spawner/ArcMassSpawnerSubsystem.h"
#include "MassAgentComponent.h"
#include "Engine/World.h"

#include "MassEntitySubsystem.h"
#include "Blueprint/BlueprintExceptionInfo.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassEntityLibrary)

#define LOCTEXT_NAMESPACE "UArcMassEntityLibrary"

void UArcMassEntityLibrary::GetEntityFragment(const UObject* WorldContextObject,
	const FMassEntityHandle& Entity, EArcMassResult& ExecResult, int32& OutFragment)
{
	// Stub - should never be called directly; CustomThunk routes to execGetEntityFragment.
	checkNoEntry();
}

DEFINE_FUNCTION(UArcMassEntityLibrary::execGetEntityFragment)
{
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_STRUCT_REF(FMassEntityHandle, Entity);
	P_GET_ENUM_REF(EArcMassResult, ExecResult);

	// Read wildcard OutFragment output.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	const FStructProperty* FragmentProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* FragmentPtr = Stack.MostRecentPropertyAddress;

	P_FINISH;

	ExecResult = EArcMassResult::NotValid;

	if (!FragmentProp || !FragmentPtr)
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::AbortExecution,
			LOCTEXT("GetFragment_InvalidValue", "Failed to resolve the fragment output for Get Entity Fragment")
		);
		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
		return;
	}

	if (!WorldContextObject)
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::AbortExecution,
			LOCTEXT("GetFragment_NoWorld", "Invalid world context for Get Entity Fragment")
		);
		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
		return;
	}

	P_NATIVE_BEGIN;

	const UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		ExecResult = EArcMassResult::NotValid;
		return;
	}

	const UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		ExecResult = EArcMassResult::NotValid;
		return;
	}

	const FMassEntityManager& EntityManager = EntitySubsystem->GetEntityManager();

	if (!EntityManager.IsEntityValid(Entity))
	{
		ExecResult = EArcMassResult::NotValid;
		return;
	}

	const UScriptStruct* RequestedStruct = FragmentProp->Struct;

	const uint8* FragmentMemory = nullptr;
	if (UE::Mass::IsSparse(RequestedStruct))
	{
		FConstStructView SparseView = EntityManager.GetSparseElementDataForEntity(RequestedStruct, Entity);
		if (SparseView.IsValid())
		{
			FragmentMemory = SparseView.GetMemory();
		}
	}
	else
	{
		FStructView RegularView = EntityManager.GetFragmentDataStruct(Entity, RequestedStruct);
		if (RegularView.IsValid())
		{
			FragmentMemory = RegularView.GetMemory();
		}
	}

	if (FragmentMemory != nullptr)
	{
		RequestedStruct->CopyScriptStruct(FragmentPtr, FragmentMemory);
		ExecResult = EArcMassResult::Valid;
	}
	else
	{
		ExecResult = EArcMassResult::NotValid;
	}

	P_NATIVE_END;
}


FMassEntityHandle UArcMassEntityLibrary::GetEntityFromComponent(const UMassAgentComponent* Component)
{
	return Component->GetEntityHandle();
}

void UArcMassEntityLibrary::DestroyEntity(const UObject* WorldContextObject, const FMassEntityHandle& InEntity)
{
	if (!WorldContextObject)
	{
		return;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	if (EntityManager.IsEntityValid(InEntity))
	{
		EntityManager.Defer().DestroyEntity(InEntity);
	}
}

FMassEntityHandle UArcMassEntityLibrary::ResolveHitToEntity(const FHitResult& HitResult, EArcMassResult& ExecResult)
{
	FMassEntityHandle Handle = ArcMassPhysicsEntityLink::ResolveHitFromPhysicsObject(HitResult);
	if (Handle.IsValid())
	{
		ExecResult = EArcMassResult::Valid;
		return Handle;
	}
	
	Handle = ArcMassPhysicsEntityLink::ResolveHit(HitResult);
	ExecResult = Handle.IsValid() ? EArcMassResult::Valid : EArcMassResult::NotValid;
	return Handle;
}

FString UArcMassEntityLibrary::ToString(const FMassEntityHandle& InHandle)
{
	return InHandle.DebugGetDescription();
}

void UArcMassEntityLibrary::SpawnSingleEntity(UObject* ContextObject, UMassEntityConfigAsset* InConfig, FVector InLocation)
{
	UArcMassEntityConfigAsset* ArcCfg = Cast<UArcMassEntityConfigAsset>(InConfig);
	if (!ArcCfg)
	{
		UE_LOGF(LogTemp, Warning,"UArcMassEntityLibrary::SpawnSingleEntity: InConfig is not a UArcMassEntityConfigAsset — spawn aborted.");
		return;
	}

	UArcMassSpawnerSubsystem* SpawnerSystem = UWorld::GetSubsystem<UArcMassSpawnerSubsystem>(ContextObject->GetWorld());
	if (!SpawnerSystem)
	{
		UE_LOGF(LogTemp, Warning,"UArcMassEntityLibrary::SpawnSingleEntity: UArcMassSpawnerSubsystem not available — spawn aborted.");
		return;
	}

	TArray<FMassEntityHandle> OutHandles;
	SpawnerSystem->SpawnEntities(ArcCfg, 1, FTransform(InLocation), OutHandles);
}

#undef LOCTEXT_NAMESPACE
