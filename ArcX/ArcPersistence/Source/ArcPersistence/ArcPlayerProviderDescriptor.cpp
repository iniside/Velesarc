/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2026 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "ArcPlayerProviderDescriptor.h"

// ─────────────────────────────────────────────────────────────────────────────
// Registry singleton
// ─────────────────────────────────────────────────────────────────────────────

FArcPlayerProviderRegistry& FArcPlayerProviderRegistry::Get()
{
	static FArcPlayerProviderRegistry Instance;
	return Instance;
}

void FArcPlayerProviderRegistry::Register(const FArcPlayerProviderDescriptor& Descriptor)
{
	for (FArcPlayerProviderDescriptor& Existing : Descriptors)
	{
		if (Existing.TargetClass == Descriptor.TargetClass)
		{
			Existing = Descriptor;
			return;
		}
	}
	Descriptors.Add(Descriptor);
}

void FArcPlayerProviderRegistry::Unregister(UClass* TargetClass)
{
	Descriptors.RemoveAll([&](const FArcPlayerProviderDescriptor& D)
	{
		return D.TargetClass == TargetClass;
	});
}

const FArcPlayerProviderDescriptor* FArcPlayerProviderRegistry::FindByClass(UClass* TargetClass) const
{
	for (const FArcPlayerProviderDescriptor& D : Descriptors)
	{
		if (D.TargetClass == TargetClass)
		{
			return &D;
		}
	}
	return nullptr;
}

const FArcPlayerProviderDescriptor* FArcPlayerProviderRegistry::FindByDomainSegment(const FString& Segment) const
{
	for (const FArcPlayerProviderDescriptor& D : Descriptors)
	{
		if (D.DomainSegment == Segment)
		{
			return &D;
		}
	}
	return nullptr;
}

TArray<const FArcPlayerProviderDescriptor*> FArcPlayerProviderRegistry::GetSerializableDescriptors() const
{
	TArray<const FArcPlayerProviderDescriptor*> Result;
	for (const FArcPlayerProviderDescriptor& D : Descriptors)
	{
		if (D.bSerializable)
		{
			Result.Add(&D);
		}
	}
	return Result;
}

TArray<const FArcPlayerProviderDescriptor*> FArcPlayerProviderRegistry::BuildChain(
	const FArcPlayerProviderDescriptor& Leaf) const
{
	TArray<const FArcPlayerProviderDescriptor*> Chain;
	Chain.Add(&Leaf);

	const FArcPlayerProviderDescriptor* Current = &Leaf;
	while (Current->ParentClass != nullptr)
	{
		const FArcPlayerProviderDescriptor* Parent = FindByClass(Current->ParentClass);
		if (!Parent)
		{
			break;
		}
		Chain.Insert(Parent, 0);
		Current = Parent;
	}

	return Chain;
}

FString FArcPlayerProviderRegistry::BuildDomainPath(
	const FArcPlayerProviderDescriptor& Leaf) const
{
	TArray<const FArcPlayerProviderDescriptor*> Chain = BuildChain(Leaf);

	FString Path;
	for (const FArcPlayerProviderDescriptor* Desc : Chain)
	{
		if (!Path.IsEmpty())
		{
			Path += TEXT("/");
		}
		Path += Desc->DomainSegment;
	}
	return Path;
}

// ─────────────────────────────────────────────────────────────────────────────
// Auto-registration delegates
// ─────────────────────────────────────────────────────────────────────────────

FArcPlayerProviderRegistryDelegates::FArcPlayerProviderRegistryDelegates() = default;
FArcPlayerProviderRegistryDelegates::~FArcPlayerProviderRegistryDelegates() = default;
