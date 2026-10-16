// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectExecution.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectDurationCalculation.h"

void UArcEffectDefinition::PostInitProperties()
{
	Super::PostInitProperties();
	if (!EffectId.IsValid() && !HasAnyFlags(RF_ClassDefaultObject))
	{
		EffectId = FGuid::NewGuid();
	}
}

void UArcEffectDefinition::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	if (DuplicateMode == EDuplicateMode::Normal)
	{
		EffectId = FGuid::NewGuid();
	}
	BuildCaptureLayout();
}

void UArcEffectDefinition::RegenerateEffectId()
{
	EffectId = FGuid::NewGuid();
	MarkPackageDirty();
}

FPrimaryAssetId UArcEffectDefinition::GetPrimaryAssetId() const
{
	if (EffectType.IsValid())
	{
		return FPrimaryAssetId(EffectType, GetFName());
	}
	return Super::GetPrimaryAssetId();
}

void UArcEffectDefinition::PostLoad()
{
	Super::PostLoad();

	uint8 RawDuration = static_cast<uint8>(StackingPolicy.DurationType);
	if (RawDuration == 3)
	{
		StackingPolicy.DurationType = EArcEffectDuration::Duration;
		StackingPolicy.Periodicity = EArcEffectPeriodicity::Periodic;
		MarkPackageDirty();
	}
	// Migrate deprecated positional FArcScopedModifier::CaptureIndex -> CaptureDef (identity).
	bool bScopedModMigrated = false;
	for (FArcEffectExecutionEntry& Entry : Executions)
	{
		if (!Entry.CustomExecution.IsValid()) { continue; }
		FArcEffectExecution* Exec = Entry.CustomExecution.GetMutablePtr<FArcEffectExecution>();
		if (!Exec) { continue; }
		for (FArcScopedModifier& Mod : Exec->ScopedModifiers)
		{
			if (!Mod.CaptureDef.Attribute.IsValid() && Exec->Captures.IsValidIndex(Mod.CaptureIndex))
			{
				Mod.CaptureDef = Exec->Captures[Mod.CaptureIndex];
				bScopedModMigrated = true;
			}
		}
	}
	if (bScopedModMigrated) { MarkPackageDirty(); }

	BuildCaptureLayout();
}

void UArcEffectDefinition::GatherIdentityTagsLocked() const
{
	IdentityTags.Reset();
	for (const FInstancedStruct& CompStruct : Components)
	{
		if (const FArcEffectComponent* Comp = CompStruct.GetPtr<FArcEffectComponent>())
		{
			Comp->GatherIdentityTags(IdentityTags);
		}
	}
	bIdentityTagsBuilt.store(true, std::memory_order_release);
}

const FGameplayTagContainer& UArcEffectDefinition::GetEffectTags() const
{
	if (!bIdentityTagsBuilt.load(std::memory_order_acquire))
	{
		FScopeLock Lock(&IdentityTagsLock);
		if (!bIdentityTagsBuilt.load(std::memory_order_relaxed))
		{
			GatherIdentityTagsLocked();
		}
	}
	return IdentityTags;
}

void UArcEffectDefinition::BuildCaptureLayout()
{
	{
		FScopeLock Lock(&IdentityTagsLock);
		GatherIdentityTagsLocked();
	}
	CaptureLayout.UniqueCaptures.Reset();
	CaptureLayout.UniqueExtras.Reset();

	TArray<FArcAttributeCaptureDef> GatheredCaptures;
	TArray<FArcExtraCaptureDef> GatheredExtras;

	for (const FArcEffectExecutionEntry& Entry : Executions)
	{
		if (const FArcEffectExecution* Exec = Entry.CustomExecution.GetPtr<FArcEffectExecution>())
		{
			Exec->GatherCaptures(GatheredCaptures, GatheredExtras);
		}
	}

	if (const FArcEffectDurationCalculation* DurationCalc = DurationCalculation.GetPtr<FArcEffectDurationCalculation>())
	{
		DurationCalc->GatherCaptures(GatheredCaptures, GatheredExtras);
	}

	for (const FArcAttributeCaptureDef& CapDef : GatheredCaptures)
	{
		CaptureLayout.UniqueCaptures.AddUnique(CapDef);
	}

	for (const FArcExtraCaptureDef& Def : GatheredExtras)
	{
		CaptureLayout.UniqueExtras.AddUnique(Def);
	}
}

#if WITH_EDITOR
void UArcEffectDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	BuildCaptureLayout();
}
#endif
