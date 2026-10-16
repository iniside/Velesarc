// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Effects/ArcEffectContext.h"
#include "Effects/ArcAttributeCapture.h"
#include "Effects/ArcEffectTypes.h"
#include "Effects/ArcExtraCapture.h"
#include "Tags/ArcGameplayTagRequirements.h"
#include "StructUtils/SharedStruct.h"
#include "StructUtils/InstancedStruct.h"
#include "Templates/Function.h"
#include "ArcEffectSpec.generated.h"

class UArcEffectDefinition;
class FGCObject;
struct FArcEffectApplyRequest;

USTRUCT()
struct ARCMASSABILITIES_API FArcCapturedExtra
{
	GENERATED_BODY()

	/** The def this slot answers to (source resolver + attribute + bSnapshot). */
	FArcExtraCaptureDef Definition;

	/** Snapshot result. Valid only when Definition.bSnapshot && bHasValue. */
	float SnapshotValue = 0.f;

	/** True if the snapshot resolved successfully. Only meaningful when Definition.bSnapshot=true.
	 *  For live entries always false (resolved on demand via AttemptResolveExtraCapture). */
	bool bHasValue = false;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcResolvedScopedMod
{
	GENERATED_BODY()

	FArcAttributeCaptureDef CaptureDef;
	EArcModifierOp Operation = EArcModifierOp::Add;
	float Magnitude = 0.f;
	FGameplayTag SetByCallerTag;
	FArcGameplayTagRequirements SourceTagRequirements;
	FArcGameplayTagRequirements TargetTagRequirements;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcEffectSpec
{
	GENERATED_BODY()

	FArcEffectSpec();
	FArcEffectSpec(const FArcEffectSpec& Other);
	FArcEffectSpec(FArcEffectSpec&& Other);
	FArcEffectSpec& operator=(const FArcEffectSpec& Other);
	FArcEffectSpec& operator=(FArcEffectSpec&& Other);

	UPROPERTY()
	TObjectPtr<UArcEffectDefinition> Definition = nullptr;

	UPROPERTY()
	FArcEffectContext Context;

	TArray<float> ResolvedMagnitudes;

	TMap<FGameplayTag, float> SetByCallerMagnitudes;

	float Duration = 0.f;

	FGameplayTagContainer CapturedSourceTags;
	FGameplayTagContainer CapturedTargetTags;

	UPROPERTY()
	TArray<FInstancedStruct> ExecutionStates;

	FArcEvaluateParameters MakeEvaluateParameters() const
	{
		FArcEvaluateParameters EvalParams;
		EvalParams.SourceTags = CapturedSourceTags;
		EvalParams.TargetTags = CapturedTargetTags;
		return EvalParams;
	}

	// -------------------------------------------------------------------------
	// CapturedAttributes accessors (def-unique)
	// -------------------------------------------------------------------------

	const FArcCapturedAttribute* FindCapturedAttribute(const FArcAttributeCaptureDef& Def) const
	{
		return CapturedAttributes.FindByPredicate([&Def](const FArcCapturedAttribute& E)
		{
			return E.Definition == Def;
		});
	}

	FArcCapturedAttribute* FindMutableCapturedAttribute(const FArcAttributeCaptureDef& Def)
	{
		return CapturedAttributes.FindByPredicate([&Def](const FArcCapturedAttribute& E)
		{
			return E.Definition == Def;
		});
	}

	void AddCapturedAttribute(const FArcCapturedAttribute& Captured)
	{
		CapturedAttributes.Add(Captured);
	}

	void ReserveCapturedAttributes(int32 Count)
	{
		CapturedAttributes.Reserve(Count);
	}

	int32 NumCapturedAttributes() const
	{
		return CapturedAttributes.Num();
	}

	void ForEachCapturedAttribute(TFunctionRef<void(const FArcCapturedAttribute&)> Fn) const
	{
		for (const FArcCapturedAttribute& Captured : CapturedAttributes)
		{
			Fn(Captured);
		}
	}

	// -------------------------------------------------------------------------
	// CapturedExtras accessors (def-unique; live-resolve test needs mutable)
	// -------------------------------------------------------------------------

	const FArcCapturedExtra* FindCapturedExtra(const FArcExtraCaptureDef& Def) const
	{
		return CapturedExtras.FindByPredicate([&Def](const FArcCapturedExtra& E)
		{
			return E.Definition == Def;
		});
	}

	FArcCapturedExtra* FindMutableCapturedExtra(const FArcExtraCaptureDef& Def)
	{
		return CapturedExtras.FindByPredicate([&Def](const FArcCapturedExtra& E)
		{
			return E.Definition == Def;
		});
	}

	bool ContainsCapturedExtra(const FArcExtraCaptureDef& Def) const
	{
		return CapturedExtras.ContainsByPredicate([&Def](const FArcCapturedExtra& E)
		{
			return E.Definition == Def;
		});
	}

	void AddCapturedExtra(const FArcCapturedExtra& Slot)
	{
		CapturedExtras.Add(Slot);
	}

	int32 NumCapturedExtras() const
	{
		return CapturedExtras.Num();
	}

	void ForEachCapturedExtra(TFunctionRef<void(const FArcCapturedExtra&)> Fn) const
	{
		for (const FArcCapturedExtra& Extra : CapturedExtras)
		{
			Fn(Extra);
		}
	}

	// -------------------------------------------------------------------------
	// ResolvedScopedModifiers accessors (NOT def-unique — N mods may share one def)
	// -------------------------------------------------------------------------

	void AddResolvedScopedModifier(const FArcResolvedScopedMod& Mod)
	{
		ResolvedScopedModifiers.Add(Mod);
	}

	void ForEachScopedModifierForDef(const FArcAttributeCaptureDef& Def, TFunctionRef<void(const FArcResolvedScopedMod&)> Fn) const
	{
		for (const FArcResolvedScopedMod& Mod : ResolvedScopedModifiers)
		{
			if (Mod.CaptureDef == Def)
			{
				Fn(Mod);
			}
		}
	}

	void ForEachResolvedScopedModifier(TFunctionRef<void(const FArcResolvedScopedMod&)> Fn) const
	{
		for (const FArcResolvedScopedMod& Mod : ResolvedScopedModifiers)
		{
			Fn(Mod);
		}
	}

	void ForEachMutableResolvedScopedModifier(TFunctionRef<void(FArcResolvedScopedMod&)> Fn)
	{
		for (FArcResolvedScopedMod& Mod : ResolvedScopedModifiers)
		{
			Fn(Mod);
		}
	}

	int32 NumResolvedScopedModifiers() const
	{
		return ResolvedScopedModifiers.Num();
	}

	void ResetResolvedScopedModifiers()
	{
		ResolvedScopedModifiers.Reset();
	}

private:

	TArray<FArcCapturedAttribute> CapturedAttributes;

	/** Flat, deduped array of resolved extra captures across all executions.
	 *  Looked up by Def-equality via AttemptResolveExtraCapture — no positional indices. */
	TArray<FArcCapturedExtra> CapturedExtras;

	TArray<FArcResolvedScopedMod> ResolvedScopedModifiers;

	TSharedPtr<FGCObject> ReferenceOwner;
};

template<>
struct TIsTriviallyRelocatable<FArcEffectSpec>
{
	static inline constexpr bool Value = false;
};

namespace ArcEffects
{
	ARCMASSABILITIES_API FSharedStruct MakeSpec(UArcEffectDefinition* Definition, const FArcEffectContext& Context);

	ARCMASSABILITIES_API FSharedStruct MakeSpec(const FArcEffectApplyRequest& Request);

	ARCMASSABILITIES_API void RebindSpecTarget(FArcEffectSpec& Spec, FMassEntityManager& EntityManager, FMassEntityHandle Target);

	ARCMASSABILITIES_API void CaptureAttributes(FArcEffectSpec& Spec, FMassEntityManager& EntityManager,
	                                              FMassEntityHandle Source, FMassEntityHandle Target);

	ARCMASSABILITIES_API void ResolveScopedModifiers(FArcEffectSpec& Spec);

	ARCMASSABILITIES_API void CaptureExtras(FArcEffectSpec& Spec, FMassEntityManager& EntityManager);
}
