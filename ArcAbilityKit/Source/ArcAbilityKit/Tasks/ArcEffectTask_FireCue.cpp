// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcEffectTask_FireCue.h"
#include "Cues/ArcCueSourceContext.h"
#include "Effects/ArcEffectSpec.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "ArcCues/ArcCueFunctions.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityFragments.h"
#include "Engine/World.h"
#include "Sources/ArcAbilitySource_Item.h"

namespace ArcAbilityKitFireCueDetail
{
    FArcCueParameters BuildParams(FArcActiveEffect& OwningEffect, const FGameplayTagContainer& ExtraContextTags)
    {
        FArcCueParameters Params;

        const FArcEffectSpec* Spec = OwningEffect.Spec.GetPtr<FArcEffectSpec>();

        Params.OwnerEntity = OwningEffect.OwnerEntity;
        Params.TargetEntity = OwningEffect.OwnerEntity;
        Params.SourceEntity = Spec ? Spec->Context.SourceEntity : FMassEntityHandle();

        Params.Location = FTransform::Identity;
        if (OwningEffect.OwnerWorld && OwningEffect.OwnerEntity.IsValid())
        {
            UMassEntitySubsystem* MES = OwningEffect.OwnerWorld->GetSubsystem<UMassEntitySubsystem>();
            if (MES)
            {
                FMassEntityManager& EM = MES->GetMutableEntityManager();
                if (EM.IsEntityValid(OwningEffect.OwnerEntity))
                {
                    const FTransformFragment* TransformFrag = EM.GetFragmentDataPtr<FTransformFragment>(OwningEffect.OwnerEntity);
                    if (TransformFrag)
                    {
                        Params.Location = TransformFrag->GetTransform();
                    }
                }
            }
        }

        Params.Magnitude = (Spec && Spec->ResolvedMagnitudes.Num() > 0) ? Spec->ResolvedMagnitudes[0] : 0.f;

        Params.Tags = ExtraContextTags;
        if (Spec)
        {
            Params.Tags.AppendTags(Spec->CapturedSourceTags);
            Params.Tags.AppendTags(Spec->CapturedTargetTags);
        }

        Params.EffectSpec = Spec;

		FArcCueSourceContext SourceContext;
		SourceContext.SourceEffectDefinition = Spec != nullptr ? Spec->Definition : nullptr;
		SourceContext.SourceEffectHandle = OwningEffect.Handle;
		SourceContext.SourceEffectOwnerEntity = OwningEffect.OwnerEntity;
		if (Spec != nullptr)
		{
			SourceContext.SourceAbilityDefinition = Spec->Context.SourceAbility;
			SourceContext.SourceAbilityHandle = Spec->Context.SourceAbilityHandle;
			const FArcAbilitySource_Item* ItemSource = Spec->Context.SourceData.GetPtr<FArcAbilitySource_Item>();
			SourceContext.SourceItemDefinition = ItemSource != nullptr ? ItemSource->ItemDefinition : FPrimaryAssetId();
			if (Spec->Context.SourceAbility != nullptr || Spec->Context.SourceAbilityHandle.IsValid())
			{
				SourceContext.SourceAbilityOwnerEntity = Spec->Context.SourceEntity;
			}
		}
		Params.SourceContext = FInstancedStruct::Make<FArcCueSourceContext>(SourceContext);

        return Params;
    }
}

void FArcEffectTask_FireCue::OnApplication(FArcActiveEffect& OwningEffect)
{
    if (!CueTag.IsValid())
    {
        return;
    }

    UWorld* World = OwningEffect.OwnerWorld;
    if (!World)
    {
        return;
    }

    const FArcCueParameters Params = ArcAbilityKitFireCueDetail::BuildParams(OwningEffect, ExtraContextTags);

    if (bPersistent)
    {
        ActiveHandle = ArcCues::AddCue(World, CueTag, Params);
    }
    else
    {
        ArcCues::ExecuteCue(World, CueTag, Params);
    }
}

void FArcEffectTask_FireCue::OnRemoved(FArcActiveEffect& OwningEffect, EArcEffectRemovalReason Reason)
{
    if (!bPersistent || !ActiveHandle.IsValid())
    {
        return;
    }

    UWorld* World = OwningEffect.OwnerWorld;
    if (!World)
    {
        return;
    }

    ArcCues::RemoveCue(World, ActiveHandle);
    ActiveHandle = FArcCueHandle::Invalid();
}

void FArcEffectTask_FireCue::OnExecute(FArcActiveEffect& OwningEffect)
{
    if (!CueTag.IsValid())
    {
        return;
    }

    UWorld* World = OwningEffect.OwnerWorld;
    if (!World)
    {
        return;
    }

    const FArcCueParameters Params = ArcAbilityKitFireCueDetail::BuildParams(OwningEffect, ExtraContextTags);
    ArcCues::ExecuteCue(World, CueTag, Params);
}
