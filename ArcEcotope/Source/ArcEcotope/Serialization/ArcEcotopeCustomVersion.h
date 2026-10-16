// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct ARCECOTOPE_API FArcEcotopeCustomVersion
{
	static const FGuid GUID;

	enum Type : int32
	{
		Initial = 1,
		SiteFieldsAndGridLevels = 2,
		StageMeshVariants = 3,
		VisualHeightBands = 4,
		StageRenderer = 5,
		SpeciesStageRenderer = 6,
		ChunkedSiteBulkData = 7,
		AuthoritativeChunkedSite = 8,
		RegionalRecruitmentLineage = 9,
		SingleProductionBackends = 10,
		ChunkedSiteOnly = 11,
		LatestVersion = ChunkedSiteOnly,
	};
};
