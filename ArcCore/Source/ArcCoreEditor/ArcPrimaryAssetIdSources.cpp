// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcPrimaryAssetIdSources.h"

namespace ArcPrimaryAssetIdSourcesPrivate
{
	TMap<FPrimaryAssetType, FArcPrimaryAssetIdSource>& GetSources()
	{
		static TMap<FPrimaryAssetType, FArcPrimaryAssetIdSource> Sources;
		return Sources;
	}
}

void ArcPrimaryAssetIdSources::Register(const FPrimaryAssetType& Type, const FArcPrimaryAssetIdSource& Source)
{
	check(IsInGameThread());
	check(Type.IsValid());
	check(Source.GetEntries.IsBound());
	ArcPrimaryAssetIdSourcesPrivate::GetSources().Add(Type, Source);
}

void ArcPrimaryAssetIdSources::Unregister(const FPrimaryAssetType& Type)
{
	check(IsInGameThread());
	ArcPrimaryAssetIdSourcesPrivate::GetSources().Remove(Type);
}

const FArcPrimaryAssetIdSource* ArcPrimaryAssetIdSources::Find(const FPrimaryAssetType& Type)
{
	check(IsInGameThread());
	return ArcPrimaryAssetIdSourcesPrivate::GetSources().Find(Type);
}
