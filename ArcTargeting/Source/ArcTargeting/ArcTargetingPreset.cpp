#include "ArcTargetingPreset.h"

#if WITH_EDITOR
void UArcTargetingPreset::PostLoad()
{
	Super::PostLoad();

	if (Nodes.Num() == 0)
	{
		UE_LOGF(LogTemp, Warning,"UArcTargetingPreset '%ls' has no nodes; pipeline will produce no candidates.",
			*GetPathName());
	}
}
#endif
