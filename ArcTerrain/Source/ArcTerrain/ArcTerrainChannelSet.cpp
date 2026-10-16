// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainChannelSet.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcTerrainChannelSet, Log, All);
#endif

int32 UArcTerrainChannelSet::FindChannelIndex(FName InChannel) const
{
	return Channels.IndexOfByKey(InChannel);
}

uint32 ArcTerrain::ChannelNameHash(FName Channel)
{
	return FCrc::StrCrc32(*Channel.ToString().ToLower());
}

#if WITH_EDITOR

namespace ArcTerrainChannelSetLocals_7f2a
{
	void CollectProblems(const UArcTerrainChannelSet& ChannelSet, TArray<FString>& OutProblems)
	{
		TMap<FName, int32> FirstIndexByName;
		TMap<uint32, int32> FirstIndexByHash;

		for (int32 Index = 0; Index < ChannelSet.Channels.Num(); ++Index)
		{
			const FName ChannelName = ChannelSet.Channels[Index];

			if (ChannelName.IsNone())
			{
				OutProblems.Add(FString::Printf(TEXT("Channels[%d] is NAME_None"), Index));
				continue;
			}

			if (const int32* FirstIndex = FirstIndexByName.Find(ChannelName))
			{
				OutProblems.Add(FString::Printf(TEXT("Channels[%d] ('%s') duplicates Channels[%d]"),
					Index, *ChannelName.ToString(), *FirstIndex));
				continue;
			}
			FirstIndexByName.Add(ChannelName, Index);

			const uint32 Hash = ArcTerrain::ChannelNameHash(ChannelName);
			if (const int32* FirstHashIndex = FirstIndexByHash.Find(Hash))
			{
				const FName OtherName = ChannelSet.Channels[*FirstHashIndex];
				OutProblems.Add(FString::Printf(
					TEXT("Channels[%d] ('%s') hash-collides with Channels[%d] ('%s')"),
					Index, *ChannelName.ToString(), *FirstHashIndex, *OtherName.ToString()));
				continue;
			}
			FirstIndexByHash.Add(Hash, Index);
		}
	}
}

EDataValidationResult UArcTerrainChannelSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	TArray<FString> Problems;
	ArcTerrainChannelSetLocals_7f2a::CollectProblems(*this, Problems);

	for (const FString& Problem : Problems)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(FText::FromString(Problem));
	}

	return Result;
}

void UArcTerrainChannelSet::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	TArray<FString> Problems;
	ArcTerrainChannelSetLocals_7f2a::CollectProblems(*this, Problems);

	for (const FString& Problem : Problems)
	{
		UE_LOGF(LogArcTerrainChannelSet, Warning,"UArcTerrainChannelSet: %ls", *Problem);
	}
}

#endif // WITH_EDITOR
