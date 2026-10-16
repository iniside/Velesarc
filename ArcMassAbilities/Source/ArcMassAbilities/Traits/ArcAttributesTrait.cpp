// Copyright Lukasz Baran. All Rights Reserved.

#include "Traits/ArcAttributesTrait.h"
#include "Traits/ArcDefaultEffectsObserver.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcEffectDefinition.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "MassEntityTemplateRegistry.h"
#include "Mass/EntityUtils.h"
#include "MassWorldUtils.h"

namespace ArcAttributesTraitDetail
{
	static void SyncAuthoredAttributeCurrentValues(FInstancedStruct& InOutFragment)
	{
		const UScriptStruct* ScriptStruct = InOutFragment.GetScriptStruct();
		uint8* Memory = InOutFragment.GetMutableMemory();
		if (ScriptStruct == nullptr || Memory == nullptr)
		{
			return;
		}

		for (TFieldIterator<FStructProperty> It(ScriptStruct); It; ++It)
		{
			const FStructProperty* StructProp = *It;
			if (StructProp->Struct != FArcAttribute::StaticStruct())
			{
				continue;
			}

			FArcAttribute* Attr = StructProp->ContainerPtrToValuePtr<FArcAttribute>(Memory);
			Attr->InitValue(Attr->GetBaseValue());
		}
	}
}

void UArcAttributesTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FArcEffectStackFragment>();
	BuildContext.AddFragment<FArcAggregatorFragment>();

	if (DefaultEffects.Num() > 0)
	{
		FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
		FArcDefaultEffectsSharedFragment DefaultEffectsConfig;
		DefaultEffectsConfig.DefaultEffects = DefaultEffects;
		const FConstSharedStruct SharedFragment = EntityManager.GetOrCreateConstSharedFragment(DefaultEffectsConfig);
		BuildContext.AddConstSharedFragment(SharedFragment);
	}
}

void UArcAttributesTrait::BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const
{
	Out.AddSparseFragment<FArcPendingAttributeOpsFragment>();

	for (const FInstancedStruct& AttrFragment : AttributeFragments)
	{
		if (!AttrFragment.IsValid())
		{
			continue;
		}

		FInstancedStruct AttrFragmentCopy = AttrFragment;
		ArcAttributesTraitDetail::SyncAuthoredAttributeCurrentValues(AttrFragmentCopy);
		Out.AddSparseFragment(FConstStructView(AttrFragmentCopy));
	}
}
