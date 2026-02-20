// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEntityLibrary.h"

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
	FStructView FragmentView = EntityManager.GetFragmentDataStruct(Entity, RequestedStruct);

	if (FragmentView.IsValid())
	{
		RequestedStruct->CopyScriptStruct(FragmentPtr, FragmentView.GetMemory());
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

#undef LOCTEXT_NAMESPACE
