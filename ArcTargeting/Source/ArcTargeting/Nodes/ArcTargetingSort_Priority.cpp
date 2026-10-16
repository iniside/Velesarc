#include "Nodes/ArcTargetingSort_Priority.h"

void FArcTargetingSort_Priority::Execute(FArcTargetingContext&, TArray<FArcTargetCandidate>& Candidates) const
{
	Candidates.StableSort([](const FArcTargetCandidate& A, const FArcTargetCandidate& B)
	{
		return A.Priority > B.Priority;
	});
}
