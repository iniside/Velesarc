// Copyright Lukasz Baran. All Rights Reserved.

#include "Traits/ArcRecoilTrait.h"

#include "ArcRecoilModule.h"
#include "Config/ArcRecoilPreset.h"
#include "Config/ArcRecoilProfileBake.h"
#include "MassEntityTemplateRegistry.h"

namespace ArcRecoilTraitPrivate
{
	constexpr int32 TemplateStreamSeed = 0x41524352;
}

void UArcRecoilTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	if (Preset == nullptr)
	{
		UE_LOGF(LogArcRecoil, Error,"UArcRecoilTrait '%ls' has no Preset set - no recoil fragments added.", *GetPathName());
		return;
	}

	FArcRecoilStateFragment& StateFragment = BuildContext.AddFragment_GetRef<FArcRecoilStateFragment>();
	StateFragment.Stream.Initialize(ArcRecoilTraitPrivate::TemplateStreamSeed);

	FArcRecoilProfileFragment& ProfileFragment = BuildContext.AddFragment_GetRef<FArcRecoilProfileFragment>();
	ArcRecoil::BakeProfile(*Preset, DefaultStats, ProfileFragment);
}
