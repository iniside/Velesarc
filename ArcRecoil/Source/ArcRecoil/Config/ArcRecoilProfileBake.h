// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FArcRecoilProfileFragment;
struct FArcRecoilStats;
class UArcRecoilPreset;

namespace ArcRecoil
{
	ARCRECOIL_API void BakeProfile(const UArcRecoilPreset& Preset, const FArcRecoilStats& Stats, FArcRecoilProfileFragment& Out);
}
