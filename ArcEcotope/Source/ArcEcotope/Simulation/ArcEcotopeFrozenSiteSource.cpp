// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcEcotopeFrozenSiteSource.h"

#if WITH_EDITORONLY_DATA
int64 FArcEcotopeFrozenSiteSource::GetLoadedSourceBytes() const
{
	int64 Total = 0;
	for (const FArcEcotopeFrozenSitePayload& Payload : Payloads)
	{
		if (Payload.bLoadedAfterFreeze && Payload.ExpectedSize > 0 && Total <= MAX_int64 - Payload.ExpectedSize)
		{
			Total += Payload.ExpectedSize;
		}
	}
	return Total;
}

int64 FArcEcotopeFrozenSiteSource::GetMemoryOnlySourceBytes() const
{
	int64 Total = 0;
	for (const FArcEcotopeFrozenSitePayload& Payload : Payloads)
	{
		if (Payload.bMemoryOnly && Payload.ExpectedSize > 0 && Total <= MAX_int64 - Payload.ExpectedSize)
		{
			Total += Payload.ExpectedSize;
		}
	}
	return Total;
}
#endif
