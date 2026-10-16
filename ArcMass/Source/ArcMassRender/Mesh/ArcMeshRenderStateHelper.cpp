// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMeshRenderStateHelper.h"

#include "Mass/EntityManager.h"
#include "MeshComponentHelper.h"
#include "Mesh/ArcRenderMeshFragments.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"

namespace ArcMeshRenderStateHelper_Internal
{
#if WITH_EDITOR
	// Componentless proxies are invisible to the engine's material->primitive auto-rebuild
	// (FObjectCacheEventSink, fed only by real UPrimitiveComponents), and adding a primitive to the
	// scene does not request shader compilation. A material that loaded idle-incomplete (under ODSC,
	// UMaterialInstance::PostLoad runs InitStaticPermutation(None) and skips the compile) would therefore
	// stay substituted with the default grey forever, because OnMaterialCompilationFinished never fires.
	// Kick the compile on the game thread so completion fires the delegate and the existing rebuild
	// upgrades grey->real. Mirrors what a real component triggers via CheckMaterialUsage->SetMaterialUsage.
	void KickIncompleteMaterialCompile(UMaterialInterface* Material)
	{
		// CacheShaders / UpdateStaticPermutation assert IsInGameThread(); skip off-GT — the next
		// game-thread rebuild re-enters here and re-kicks. Nothing to do if already complete/in-flight.
		if (!IsInGameThread() || Material == nullptr || Material->IsComplete() || Material->IsCompiling())
		{
			return;
		}

		if (UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material))
		{
			// Primary: starts an async compile when the instance has no shader map yet.
			MaterialInstance->CacheShaders(EMaterialShaderPrecompileMode::Background);

			// Fallback: CacheShaders no-ops when the instance already holds an incomplete-but-present
			// shader map (FMaterial::CacheShaders only reaches BeginCompileShaderMap when an instance's
			// GameThreadShaderMap is null). Force a static-permutation update — it regenerates the shader
			// map id, missing FindId and queuing a compile regardless of the prior map state.
			if (!MaterialInstance->IsComplete() && !MaterialInstance->IsCompiling())
			{
				MaterialInstance->UpdateStaticPermutation();
			}
		}
		else if (UMaterial* BaseMaterial = Cast<UMaterial>(Material))
		{
			BaseMaterial->ForceRecompileForRendering();
		}
	}
#endif // WITH_EDITOR
}

FArcMeshRenderStateHelper::FArcMeshRenderStateHelper(FMassEntityHandle InEntityHandle, TNotNull<FMassEntityManager*> InEntityManager, const FArcRenderPrimitiveFragment& RenderPrimitiveFragment, const FArcOverrideMaterialsFragment* OverrideMaterialsFragment)
	: Super(InEntityHandle, InEntityManager, RenderPrimitiveFragment)
{
}

void FArcMeshRenderStateHelper::GetMaterialSlotsOverlayMaterial(TArray<TObjectPtr<UMaterialInterface>>& OutMaterialSlotOverlayMaterials) const
{
	FMeshComponentHelper::GetMaterialSlotsOverlayMaterial(*this, OutMaterialSlotOverlayMaterials);
}

FMaterialRelevance FArcMeshRenderStateHelper::GetMaterialRelevance(EShaderPlatform InShaderPlatform) const
{
	return FMeshComponentHelper::GetMaterialRelevance(*this, InShaderPlatform);
}

const FArcOverrideMaterialsFragment* FArcMeshRenderStateHelper::GetOverrideMaterialsFragment() const
{
	return GetEntityManager().GetConstSharedFragmentDataPtr<FArcOverrideMaterialsFragment>(EntityHandle);
}

const FArcVisualizationMeshFragment& FArcMeshRenderStateHelper::GetMeshFragment() const
{
	return GetEntityManager().GetConstSharedFragmentDataChecked<FArcVisualizationMeshFragment>(EntityHandle);
}

#if WITH_EDITOR
const FArcEditorVisualizationMeshFragment* FArcMeshRenderStateHelper::GetEditorMeshFragment() const
{
	return GetEntityManager().GetConstSharedFragmentDataPtr<FArcEditorVisualizationMeshFragment>(EntityHandle);
}
#endif // WITH_EDITOR

TConstArrayView<TObjectPtr<UMaterialInterface>> FArcMeshRenderStateHelper::GetOverrideMaterials() const
{
	if (const FArcOverrideMaterialsFragment* OverrideMaterialFragment = GetOverrideMaterialsFragment())
	{
		return OverrideMaterialFragment->OverrideMaterials;
	}
	return TConstArrayView<TObjectPtr<UMaterialInterface>>();
}

TConstArrayView<TObjectPtr<UMaterialInterface>> FArcMeshRenderStateHelper::BuildRenderSafeOverrideMaterials(int32 NumSlots, TFunctionRef<UMaterialInterface*(int32)> EffectiveMaterialGetter)
{
	TConstArrayView<TObjectPtr<UMaterialInterface>> Overrides = GetOverrideMaterials();
	const int32 Count = FMath::Max(NumSlots, Overrides.Num());
	RenderSafeOverrideMaterials.Reset();
	RenderSafeOverrideMaterials.SetNumZeroed(Count);
	bool bAnySubstituted = false;
	for (int32 SlotIndex = 0; SlotIndex < Count; ++SlotIndex)
	{
		UMaterialInterface* Override = Overrides.IsValidIndex(SlotIndex) ? Overrides[SlotIndex].Get() : nullptr;
		UMaterialInterface* Effective = EffectiveMaterialGetter(SlotIndex);
		if (Effective && !Effective->IsComplete())
		{
#if WITH_EDITOR
			// Bind the crash-safe default now, but kick the real material's compile so a later
			// OnMaterialCompilationFinished rebuild can upgrade this slot grey->real.
			ArcMeshRenderStateHelper_Internal::KickIncompleteMaterialCompile(Effective);
#endif
			RenderSafeOverrideMaterials[SlotIndex] = UMaterial::GetDefaultMaterial(MD_Surface);
			bAnySubstituted = true;
		}
		else
		{
			RenderSafeOverrideMaterials[SlotIndex] = Override; // preserve null = use asset slot material
		}
	}
	if (!bAnySubstituted && Overrides.Num() == 0)
	{
		RenderSafeOverrideMaterials.Reset();
		return TConstArrayView<TObjectPtr<UMaterialInterface>>();
	}
	return RenderSafeOverrideMaterials;
}
