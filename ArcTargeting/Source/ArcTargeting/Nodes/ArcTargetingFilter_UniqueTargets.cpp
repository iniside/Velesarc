#include "Nodes/ArcTargetingFilter_UniqueTargets.h"

#include "GameFramework/Actor.h"

void FArcTargetingFilter_UniqueTargets::Execute(FArcTargetingContext&, TArray<FArcTargetCandidate>& Candidates) const
{
	TSet<FMassEntityHandle> SeenEntities;
	TSet<const AActor*> SeenActors;
	TArray<FArcTargetCandidate> UniqueCandidates;
	UniqueCandidates.Reserve(Candidates.Num());

	for (FArcTargetCandidate& Candidate : Candidates)
	{
		if (Candidate.Entity.IsValid())
		{
			if (SeenEntities.Contains(Candidate.Entity))
			{
				continue;
			}

			SeenEntities.Add(Candidate.Entity);
		}
		else if (const AActor* Actor = Candidate.HitResult.GetActor())
		{
			if (SeenActors.Contains(Actor))
			{
				continue;
			}

			SeenActors.Add(Actor);
		}

		UniqueCandidates.Add(MoveTemp(Candidate));
	}

	Candidates = MoveTemp(UniqueCandidates);
}
