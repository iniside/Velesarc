// Copyright Lukasz Baran. All Rights Reserved.

#include "QuickBar/ArcMassQuickBarTrait.h"
#include "QuickBar/ArcMassQuickBarConfig.h"
#include "QuickBar/ArcMassQuickBarOperations.h"
#include "QuickBar/ArcMassQuickBarSharedFragment.h"
#include "QuickBar/ArcMassQuickBarStateFragment.h"
#include "QuickBar/ArcMassQuickBarTypes.h"
#include "MassEntityTemplateRegistry.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcMassQuickBarTrait, Log, All);

namespace ArcMassQuickBarTrait_Private
{
	struct FBarNode
	{
		FGameplayTag BarId;
		FGameplayTag ParentBarId;
		EArcMassQuickSlotsMode QuickSlotsMode = EArcMassQuickSlotsMode::Cyclable;
	};
}

void UArcMassQuickBarTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FArcMassQuickBarSharedFragment SharedFragment;
	SharedFragment.Configs = Configs;
	BuildContext.AddConstSharedFragment(FConstSharedStruct::Make(SharedFragment));
}

bool UArcMassQuickBarTrait::ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World, FAdditionalTraitRequirements& OutTraitRequirements) const
{
	bool bValid = true;

	// Ops, commands and FArcMassQuickBarStateFragment address bars and groups by tag alone, so ids must be unique across all configs.
	TSet<FGameplayTag> SeenBarIds;
	TSet<FGameplayTag> SeenGroupIds;
	TArray<ArcMassQuickBarTrait_Private::FBarNode> BarNodes;

	for (int32 ConfigIndex = 0; ConfigIndex < Configs.Num(); ++ConfigIndex)
	{
		const UArcMassQuickBarConfig* Config = Configs[ConfigIndex];
		if (!Config)
		{
			UE_LOGF(LogArcMassQuickBarTrait, Error, "UArcMassQuickBarTrait on '%ls': Configs[%d] is null.", *GetPathName(), ConfigIndex);
			bValid = false;
			continue;
		}

		if (!Config->StoreType)
		{
			UE_LOGF(LogArcMassQuickBarTrait, Error, "UArcMassQuickBarTrait on '%ls': config '%ls' has no StoreType.", *GetPathName(), *Config->GetPathName());
			bValid = false;
		}

		for (const FArcMassQuickBarGroup& Group : Config->Groups)
		{
			bool bAlreadySeen = false;
			SeenGroupIds.Add(Group.GroupId, &bAlreadySeen);
			if (bAlreadySeen)
			{
				UE_LOGF(LogArcMassQuickBarTrait, Error, "UArcMassQuickBarTrait on '%ls': GroupId %ls appears more than once (config '%ls').",
					*GetPathName(), *Group.GroupId.ToString(), *Config->GetPathName());
				bValid = false;
			}

			int32 ActiveOnStartCount = 0;
			for (const FArcMassQuickBarEntry& Bar : Group.Bars)
			{
				if (Bar.bActiveOnStart)
				{
					++ActiveOnStartCount;
				}
			}
			if (ActiveOnStartCount > 1)
			{
				UE_LOGF(LogArcMassQuickBarTrait, Error, "UArcMassQuickBarTrait on '%ls': group %ls (config '%ls') has %d bars with bActiveOnStart; at most one is allowed.",
					*GetPathName(), *Group.GroupId.ToString(), *Config->GetPathName(), ActiveOnStartCount);
				bValid = false;
			}
		}

		ArcMassItems::QuickBar::ForEachBarConfig(*Config,
			[this, Config, &SeenBarIds, &BarNodes, &bValid](const FArcMassQuickBarEntry& Bar, const FArcMassQuickBarGroup* Group) -> bool
			{
				ArcMassQuickBarTrait_Private::FBarNode& Node = BarNodes.AddDefaulted_GetRef();
				Node.BarId = Bar.BarId;
				Node.ParentBarId = Bar.ParentBarId;
				Node.QuickSlotsMode = Bar.QuickSlotsMode;

				bool bAlreadySeen = false;
				SeenBarIds.Add(Bar.BarId, &bAlreadySeen);
				if (bAlreadySeen)
				{
					UE_LOGF(LogArcMassQuickBarTrait, Error, "UArcMassQuickBarTrait on '%ls': BarId %ls appears more than once (config '%ls').",
						*GetPathName(), *Bar.BarId.ToString(), *Config->GetPathName());
					bValid = false;
				}

				if (Group && !Bar.ParentBarId.IsValid())
				{
					if (!Bar.bDeferItemActivation)
					{
						UE_LOGF(LogArcMassQuickBarTrait, Error, "UArcMassQuickBarTrait on '%ls': grouped bar %ls (group %ls, config '%ls') must set bDeferItemActivation.",
							*GetPathName(), *Bar.BarId.ToString(), *Group->GroupId.ToString(), *Config->GetPathName());
						bValid = false;
					}

					for (const FArcMassQuickBarSlot& Slot : Bar.Slots)
					{
						if (!Slot.ItemSlotId.IsValid())
						{
							UE_LOGF(LogArcMassQuickBarTrait, Error, "UArcMassQuickBarTrait on '%ls': grouped bar %ls (group %ls, config '%ls') slot %ls has no ItemSlotId.",
								*GetPathName(), *Bar.BarId.ToString(), *Group->GroupId.ToString(), *Config->GetPathName(), *Slot.QuickBarSlotId.ToString());
							bValid = false;
						}
					}
				}

				if (!Bar.ParentBarId.IsValid())
				{
					return true;
				}

				if (Bar.bActiveOnStart)
				{
					UE_LOGF(LogArcMassQuickBarTrait, Error, "UArcMassQuickBarTrait on '%ls': child bar %ls (config '%ls') must not set bActiveOnStart.",
						*GetPathName(), *Bar.BarId.ToString(), *Config->GetPathName());
					bValid = false;
				}

				if (Bar.ParentBarId == Bar.BarId)
				{
					UE_LOGF(LogArcMassQuickBarTrait, Error, "UArcMassQuickBarTrait on '%ls': child bar %ls (config '%ls') names itself as ParentBarId.",
						*GetPathName(), *Bar.BarId.ToString(), *Config->GetPathName());
					bValid = false;
				}

				if (!Bar.bDeferItemActivation)
				{
					UE_LOGF(LogArcMassQuickBarTrait, Error, "UArcMassQuickBarTrait on '%ls': child bar %ls (config '%ls') must set bDeferItemActivation.",
						*GetPathName(), *Bar.BarId.ToString(), *Config->GetPathName());
					bValid = false;
				}

				if (Group)
				{
					UE_LOGF(LogArcMassQuickBarTrait, Error, "UArcMassQuickBarTrait on '%ls': child bar %ls (config '%ls') must be in QuickBars, not in group %ls.",
						*GetPathName(), *Bar.BarId.ToString(), *Config->GetPathName(), *Group->GroupId.ToString());
					bValid = false;
				}

				if (Bar.QuickSlotsMode == EArcMassQuickSlotsMode::Cyclable)
				{
					UE_LOGF(LogArcMassQuickBarTrait, Error, "UArcMassQuickBarTrait on '%ls': child bar %ls (config '%ls') must not be Cyclable.",
						*GetPathName(), *Bar.BarId.ToString(), *Config->GetPathName());
					bValid = false;
				}

				for (const FArcMassQuickBarSlot& Slot : Bar.Slots)
				{
					if (!Slot.ItemSlotId.IsValid())
					{
						UE_LOGF(LogArcMassQuickBarTrait, Error, "UArcMassQuickBarTrait on '%ls': child bar %ls (config '%ls') slot %ls has no ItemSlotId.",
							*GetPathName(), *Bar.BarId.ToString(), *Config->GetPathName(), *Slot.QuickBarSlotId.ToString());
						bValid = false;
					}
				}

				return true;
			});
	}

	TSet<FGameplayTag> ParentsWithChild;
	for (const ArcMassQuickBarTrait_Private::FBarNode& Node : BarNodes)
	{
		if (!Node.ParentBarId.IsValid() || Node.ParentBarId == Node.BarId)
		{
			continue;
		}

		bool bParentHasChild = false;
		ParentsWithChild.Add(Node.ParentBarId, &bParentHasChild);
		if (bParentHasChild)
		{
			UE_LOGF(LogArcMassQuickBarTrait, Error, "UArcMassQuickBarTrait on '%ls': parent bar %ls has more than one child bar (%ls is another).",
				*GetPathName(), *Node.ParentBarId.ToString(), *Node.BarId.ToString());
			bValid = false;
		}

		const ArcMassQuickBarTrait_Private::FBarNode* Parent = BarNodes.FindByPredicate(
			[&Node](const ArcMassQuickBarTrait_Private::FBarNode& Candidate) { return Candidate.BarId == Node.ParentBarId; });
		if (!Parent)
		{
			UE_LOGF(LogArcMassQuickBarTrait, Error, "UArcMassQuickBarTrait on '%ls': child bar %ls names ParentBarId %ls, which is not a bar of this trait.",
				*GetPathName(), *Node.BarId.ToString(), *Node.ParentBarId.ToString());
			bValid = false;
			continue;
		}

		if (Parent->ParentBarId.IsValid())
		{
			UE_LOGF(LogArcMassQuickBarTrait, Error, "UArcMassQuickBarTrait on '%ls': child bar %ls names ParentBarId %ls, which is itself a child bar; child bars do not nest.",
				*GetPathName(), *Node.BarId.ToString(), *Node.ParentBarId.ToString());
			bValid = false;
		}

		if (Parent->QuickSlotsMode == EArcMassQuickSlotsMode::AutoActivateOnly)
		{
			UE_LOGF(LogArcMassQuickBarTrait, Error, "UArcMassQuickBarTrait on '%ls': parent bar %ls of child bar %ls must not be AutoActivateOnly.",
				*GetPathName(), *Node.ParentBarId.ToString(), *Node.BarId.ToString());
			bValid = false;
		}
	}

	return bValid;
}

void UArcMassQuickBarTrait::BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const
{
	FArcMassQuickBarStateFragment State;
	ArcMassItems::QuickBar::SeedBarStates(Configs, State);
	Out.AddSparseFragment(State);
}
