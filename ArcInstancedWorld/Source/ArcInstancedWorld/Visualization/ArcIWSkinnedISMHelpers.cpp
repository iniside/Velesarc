// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWSkinnedISMHelpers.h"

#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassInstancedSkinnedMeshRenderStateHelper.h"
#include "MassEntityManager.h"

namespace UE::ArcIW::SkinnedISM
{
	void FlushDirtyHolders(const TSet<FMassEntityHandle>& DirtyHolders, FMassEntityManager& EntityManager)
	{
		for (FMassEntityHandle HolderEntity : DirtyHolders)
		{
			if (!EntityManager.IsEntityValid(HolderEntity))
			{
				continue;
			}

			FArcMassRenderISMSkinnedFragment* ISMFrag =
				EntityManager.GetFragmentDataPtr<FArcMassRenderISMSkinnedFragment>(HolderEntity);
			if (!ISMFrag || !ISMFrag->RenderStateHelper)
			{
				continue;
			}

			ISMFrag->RenderStateHelper->DestroyRenderState(nullptr);
			if (ISMFrag->PerInstanceData.Num() > 0)
			{
				ISMFrag->RenderStateHelper->CreateRenderState(nullptr);
			}
			ISMFrag->bRenderStateDirty = false;
		}
	}
}
