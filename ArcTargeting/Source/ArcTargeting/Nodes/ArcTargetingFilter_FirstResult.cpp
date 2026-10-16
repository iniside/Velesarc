#include "Nodes/ArcTargetingFilter_FirstResult.h"

void FArcTargetingFilter_FirstResult::Execute(FArcTargetingContext&, TArray<FArcTargetCandidate>& Candidates) const
{
	if (Candidates.Num() > 1)
	{
		Candidates.SetNum(1, EAllowShrinking::No);
	}
}
