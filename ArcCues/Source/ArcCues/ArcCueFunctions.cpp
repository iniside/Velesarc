// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCueFunctions.h"

#include "ArcCueSubsystem.h"

namespace ArcCues
{
	void ExecuteCue(UWorld* World, FGameplayTag CueTag, const FArcCueParameters& Params)
	{
		UArcCueSubsystem* Subsystem = UArcCueSubsystem::Get(World);
		if (Subsystem != nullptr)
		{
			Subsystem->ExecuteCue(CueTag, Params);
		}
	}

	void ExecuteCue(UWorld* World, UArcCueDefinition* Definition, const FArcCueParameters& Params)
	{
		UArcCueSubsystem* Subsystem = UArcCueSubsystem::Get(World);
		if (Subsystem != nullptr)
		{
			Subsystem->ExecuteCue(Definition, Params);
		}
	}

	FArcCueHandle AddCue(UWorld* World, FGameplayTag CueTag, const FArcCueParameters& Params)
	{
		UArcCueSubsystem* Subsystem = UArcCueSubsystem::Get(World);
		if (Subsystem == nullptr)
		{
			return FArcCueHandle::Invalid();
		}
		return Subsystem->AddCue(CueTag, Params);
	}

	FArcCueHandle AddCue(UWorld* World, UArcCueDefinition* Definition, const FArcCueParameters& Params)
	{
		UArcCueSubsystem* Subsystem = UArcCueSubsystem::Get(World);
		if (Subsystem == nullptr)
		{
			return FArcCueHandle::Invalid();
		}
		return Subsystem->AddCue(Definition, Params);
	}

	void SendCueEvent(UWorld* World, FArcCueHandle Handle, const FArcCueEvent& Event)
	{
		UArcCueSubsystem* Subsystem = UArcCueSubsystem::Get(World);
		if (Subsystem != nullptr)
		{
			Subsystem->SendCueEvent(Handle, Event);
		}
	}

	void SendCueStateTreeEvent(UWorld* World, FArcCueHandle Handle, const FStateTreeEvent& Event)
	{
		UArcCueSubsystem* Subsystem = UArcCueSubsystem::Get(World);
		if (Subsystem != nullptr)
		{
			Subsystem->SendCueStateTreeEvent(Handle, Event);
		}
	}

	void RemoveCue(UWorld* World, FArcCueHandle Handle)
	{
		UArcCueSubsystem* Subsystem = UArcCueSubsystem::Get(World);
		if (Subsystem != nullptr)
		{
			Subsystem->RemoveCue(Handle);
		}
	}

	void RemoveCuesByTag(UWorld* World, FGameplayTag CueTag, FMassEntityHandle TargetEntity)
	{
		UArcCueSubsystem* Subsystem = UArcCueSubsystem::Get(World);
		if (Subsystem != nullptr)
		{
			Subsystem->RemoveCuesByTag(CueTag, TargetEntity);
		}
	}

	void RemoveAllCues(UWorld* World, FMassEntityHandle TargetEntity)
	{
		UArcCueSubsystem* Subsystem = UArcCueSubsystem::Get(World);
		if (Subsystem != nullptr)
		{
			Subsystem->RemoveAllCues(TargetEntity);
		}
	}

	void RemoveCuesByOwner(UWorld* World, FMassEntityHandle OwnerEntity)
	{
		UArcCueSubsystem* Subsystem = UArcCueSubsystem::Get(World);
		if (Subsystem != nullptr)
		{
			Subsystem->RemoveCuesByOwner(OwnerEntity);
		}
	}
}
