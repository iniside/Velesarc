#pragma once

#include "CoreMinimal.h"
#include "ArcTargetingPipelineTypes.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/WeakObjectPtr.h"

class AActor;
class UObject;

/**
 * Request-local context carried by reference through every targeting node.
 *
 * Members fall into three groups:
 *   1. Seed inputs — SourceLocation/SourceDirection are populated by the caller
 *      before the pipeline runs. Origin nodes may overwrite them; all other
 *      nodes are free to read them at any point. The framework does not treat
 *      them as a cursor that auto-advances between nodes.
 *   2. Output — `Candidates` is appended by producer nodes, narrowed by filter
 *      nodes, and scored by scoring nodes.
 *   3. Extension point — `ExtraData` is the typed bag for project-specific
 *      payloads (ArcCore ASC handle, gun spread state, etc.) that the
 *      ArcTargeting plugin must not know about. Keyed by UScriptStruct so
 *      lookup is O(1) by type.
 *
 * Plain C++ struct (not USTRUCT). It is not serialized, not replicated, not
 * editor-visible. It is a transient runtime bag with a stack/request lifetime.
 */
struct ARCTARGETING_API FArcTargetingContext
{
	// Universal — no ArcCore / ArcMassAbilities / ArcGun types here.
	TWeakObjectPtr<AActor> SourceActor;
	TWeakObjectPtr<AActor> InstigatorActor;
	FVector SourceLocation = FVector::ZeroVector;
	FVector SourceDirection = FVector::ForwardVector;
	FName SourceSocketName = NAME_None;
	TWeakObjectPtr<UObject> SourceObject;
	FMassEntityHandle SourceEntity;

	// Output.
	TArray<FArcTargetCandidate> Candidates;

	// Extension point. Keyed by UScriptStruct, O(1) lookup by type.
	TMap<const UScriptStruct*, FInstancedStruct> ExtraData;

	template <typename T>
	const T* FindExtra() const
	{
		const FInstancedStruct* Found = ExtraData.Find(T::StaticStruct());
		return Found ? Found->GetPtr<T>() : nullptr;
	}

	template <typename T>
	T* FindMutableExtra()
	{
		FInstancedStruct* Found = ExtraData.Find(T::StaticStruct());
		return Found ? Found->GetMutablePtr<T>() : nullptr;
	}

	template <typename T>
	T& FindOrAddExtra()
	{
		FInstancedStruct& Slot = ExtraData.FindOrAdd(T::StaticStruct());
		if (!Slot.IsValid())
		{
			Slot.InitializeAs<T>();
		}
		return Slot.GetMutable<T>();
	}

	template <typename T>
	void RemoveExtra()
	{
		ExtraData.Remove(T::StaticStruct());
	}
};
