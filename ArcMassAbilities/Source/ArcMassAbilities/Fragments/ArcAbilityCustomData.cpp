// Copyright Lukasz Baran. All Rights Reserved.

#include "Fragments/ArcAbilityCustomData.h"

FArcAbilityCustomData::FArcAbilityCustomData() = default;
FArcAbilityCustomData::~FArcAbilityCustomData() = default;
FArcAbilityCustomData::FArcAbilityCustomData(const FArcAbilityCustomData&) = default;
FArcAbilityCustomData::FArcAbilityCustomData(FArcAbilityCustomData&&) noexcept = default;
FArcAbilityCustomData& FArcAbilityCustomData::operator=(const FArcAbilityCustomData&) = default;
FArcAbilityCustomData& FArcAbilityCustomData::operator=(FArcAbilityCustomData&&) noexcept = default;

void FArcAbilityCustomData::Add(FInstancedStruct&& Source)
{
	Sources.Emplace(MoveTemp(Source));
}

bool FArcAbilityCustomData::IsEmpty() const
{
	return Sources.IsEmpty();
}

const FInstancedStruct* FArcAbilityCustomData::FindByScriptStruct(const UScriptStruct* Type) const
{
	if (Type == nullptr)
	{
		return nullptr;
	}
	for (const FInstancedStruct& Entry : Sources)
	{
		const UScriptStruct* EntryStruct = Entry.GetScriptStruct();
		if (EntryStruct != nullptr && EntryStruct->IsChildOf(Type))
		{
			return &Entry;
		}
	}
	return nullptr;
}

const FInstancedStruct& FArcAbilityCustomData::GetPrimary() const
{
	static const FInstancedStruct Empty;
	return Sources.IsEmpty() ? Empty : Sources[0];
}

FInstancedStruct& FArcAbilityCustomData::GetPrimaryMutable()
{
	if (Sources.IsEmpty())
	{
		Sources.Emplace();
	}
	return Sources[0];
}
