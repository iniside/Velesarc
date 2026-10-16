// Copyright Lukasz Baran. All Rights Reserved.

#include "Serialization/ArcEcotopeCustomVersion.h"

#include "Serialization/CustomVersion.h"

const FGuid FArcEcotopeCustomVersion::GUID(0xC7AF45B2, 0xF16F4D4D, 0xA152F38C, 0x7398A6E1);

static FCustomVersionRegistration GRegisterArcEcotopeCustomVersion(
	FArcEcotopeCustomVersion::GUID,
	FArcEcotopeCustomVersion::LatestVersion,
	TEXT("ArcEcotopeVer"));
