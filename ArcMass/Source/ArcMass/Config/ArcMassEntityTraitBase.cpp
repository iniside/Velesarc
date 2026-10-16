// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Config/ArcMassEntityTraitBase.h"

#include "Engine/World.h"
#include "MassEntityConfigAsset.h"
#include "MassWorldUtils.h"

namespace ArcMass
{
	void CollectCombinedTraits(const FMassEntityConfig& InConfig, const UWorld* World,
		TArray<UMassEntityTraitBase*>& OutTraits)
	{
		OutTraits.Reset();

		TArray<const FMassEntityConfig*, TInlineAllocator<8>> Chain;
		TArray<const UMassEntityConfigAsset*, TInlineAllocator<8>> Visited;
		const FMassEntityConfig* Current = &InConfig;
		while (Current != nullptr)
		{
			Chain.Add(Current);

			const UMassEntityConfigAsset* Parent = Current->GetParent();
			if (Parent == nullptr || Visited.Contains(Parent))
			{
				break;
			}
			Visited.Add(Parent);
			Current = &Parent->GetConfig();
		}

		for (int32 ChainIndex = Chain.Num() - 1; ChainIndex >= 0; --ChainIndex)
		{
			for (UMassEntityTraitBase* Trait : Chain[ChainIndex]->GetTraits())
			{
				if (Trait == nullptr)
				{
					continue;
				}

				const int32 Index = OutTraits.IndexOfByPredicate(
					[Trait](const UMassEntityTraitBase* Existing) -> bool
					{
						return Trait->GetClass() == Existing->GetClass()
							&& static_cast<bool>(Trait->ValidTargetConfig & Existing->ValidTargetConfig);
					});

				if (Index != INDEX_NONE)
				{
					OutTraits[Index] = Trait;
				}
				else
				{
					OutTraits.Add(Trait);
				}
			}
		}

		if (World == nullptr)
		{
			return;
		}

		const EProcessorExecutionFlags WorldProcessingFlags = World->IsGameWorld()
			? UE::Mass::Utils::DetermineProcessorExecutionFlags(World)
			: EProcessorExecutionFlags::All;

		OutTraits.RemoveAll(
			[WorldProcessingFlags](const UMassEntityTraitBase* Trait) -> bool
			{
				return !static_cast<bool>(Trait->ValidTargetConfig & WorldProcessingFlags);
			});
	}
}
