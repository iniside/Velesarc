// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Extensions/ArcAbilityManifestExtension.h"
#include "Effects/ArcEffectDefinition.h"
#include "StateTree.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "MassEntityConfigAsset.h"
#include "HAL/IConsoleManager.h"
#include "Logging/LogMacros.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcAbilityManifest, Log, All);

namespace ArcAbilityManifestDebug
{
    void DumpManifest(const TArray<FString>& Args)
    {
        if (Args.Num() < 1)
        {
            UE_LOGF(LogArcAbilityManifest, Warning,"Usage: Arc.Ability.DumpManifest <StateTreeSoftObjectPath>");
            return;
        }

        const FSoftObjectPath TreePath(Args[0]);
        UStateTree* Tree = Cast<UStateTree>(TreePath.TryLoad());
        if (!Tree)
        {
            UE_LOGF(LogArcAbilityManifest, Warning,"Failed to load UStateTree at '%ls'.", *Args[0]);
            return;
        }

        const UArcAbilityManifestExtension* Manifest = Tree->GetExtension<UArcAbilityManifestExtension>();
        if (!Manifest)
        {
            UE_LOGF(LogArcAbilityManifest, Warning,"StateTree '%ls' has no UArcAbilityManifestExtension baked. "
                     "Add an 'Ability Manifest (Arc)' editor extension to the asset and recompile.",
                *Tree->GetPathName());
            return;
        }

        UE_LOGF(LogArcAbilityManifest, Display,"=== Ability Manifest for %ls ===", *Tree->GetPathName());

        UE_LOGF(LogArcAbilityManifest, Display,"InputActions (%d):", Manifest->InputActions.Num());
        for (const TObjectPtr<UInputAction>& Action : Manifest->InputActions)
        {
            UE_LOGF(LogArcAbilityManifest, Display,"  %ls", *GetPathNameSafe(Action));
        }

        UE_LOGF(LogArcAbilityManifest, Display,"InputMappingContexts (%d):", Manifest->InputMappingContexts.Num());
        for (const TObjectPtr<UInputMappingContext>& IMC : Manifest->InputMappingContexts)
        {
            UE_LOGF(LogArcAbilityManifest, Display,"  %ls", *GetPathNameSafe(IMC));
        }

        UE_LOGF(LogArcAbilityManifest, Display,"InputTags: %ls", *Manifest->InputTags.ToString());

        UE_LOGF(LogArcAbilityManifest, Display,"AppliedEffects (%d):", Manifest->AppliedEffects.Num());
        for (const TObjectPtr<UArcEffectDefinition>& Effect : Manifest->AppliedEffects)
        {
            UE_LOGF(LogArcAbilityManifest, Display,"  %ls", *GetPathNameSafe(Effect));
        }

        UE_LOGF(LogArcAbilityManifest, Display,"SpawnedActorClasses (%d):", Manifest->SpawnedActorClasses.Num());
        for (const TSubclassOf<AActor>& Cls : Manifest->SpawnedActorClasses)
        {
            UE_LOGF(LogArcAbilityManifest, Display,"  %ls", *GetPathNameSafe(Cls.Get()));
        }

        UE_LOGF(LogArcAbilityManifest, Display,"SpawnedEntityConfigs (%d):", Manifest->SpawnedEntityConfigs.Num());
        for (const TObjectPtr<UMassEntityConfigAsset>& Cfg : Manifest->SpawnedEntityConfigs)
        {
            UE_LOGF(LogArcAbilityManifest, Display,"  %ls", *GetPathNameSafe(Cfg));
        }

        UE_LOGF(LogArcAbilityManifest, Display,"ClaimedSparseFragments (%d):", Manifest->ClaimedSparseFragments.Num());
        for (const TObjectPtr<UScriptStruct>& Frag : Manifest->ClaimedSparseFragments)
        {
            UE_LOGF(LogArcAbilityManifest, Display,"  %ls", Frag ? *Frag->GetStructCPPName() : TEXT("<null>"));
        }
    }

    static FAutoConsoleCommand GDumpManifestCmd(
        TEXT("Arc.Ability.DumpManifest"),
        TEXT("Dump the baked UArcAbilityManifestExtension on a UStateTree. Usage: Arc.Ability.DumpManifest /Game/Path/To/AbilityTree.AbilityTree"),
        FConsoleCommandWithArgsDelegate::CreateStatic(&DumpManifest));
}
