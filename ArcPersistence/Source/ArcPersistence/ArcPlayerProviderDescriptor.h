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

#pragma once

#include "CoreMinimal.h"

/**
 * Hierarchical provider descriptor. Providers form a tree rooted at
 * APlayerController (obtained from PostLogin). Each provider resolves
 * from its parent via ResolveFunc.
 *
 * Interior nodes (e.g., PlayerState) resolve actors.
 * Leaf nodes (e.g., UArcItemsStoreComponent) resolve serializable objects.
 * Domain path is built by concatenating DomainSegment along the chain.
 */
struct ARCPERSISTENCE_API FArcPlayerProviderDescriptor
{
	/** The UClass this provider resolves to. */
	UClass* TargetClass = nullptr;

	/** Path segment for this level (e.g., "PlayerState", "UArcItemsStoreComponent"). */
	FString DomainSegment;

	/** Parent's TargetClass. nullptr = root (resolves directly from PlayerController). */
	UClass* ParentClass = nullptr;

	/** Resolves an instance of TargetClass from the parent object. */
	TFunction<UObject*(UObject* Parent)> ResolveFunc;

	/** If true, this is a leaf node whose object gets serialized/deserialized. */
	bool bSerializable = false;
};

/**
 * Singleton registry of player provider descriptors.
 */
class ARCPERSISTENCE_API FArcPlayerProviderRegistry
{
public:
	static FArcPlayerProviderRegistry& Get();

	void Register(const FArcPlayerProviderDescriptor& Descriptor);
	void Unregister(UClass* TargetClass);

	const TArray<FArcPlayerProviderDescriptor>& GetDescriptors() const { return Descriptors; }

	/** Find descriptor by target class. */
	const FArcPlayerProviderDescriptor* FindByClass(UClass* TargetClass) const;

	/** Find descriptor by domain segment. */
	const FArcPlayerProviderDescriptor* FindByDomainSegment(const FString& Segment) const;

	/** Get all serializable (leaf) descriptors. */
	TArray<const FArcPlayerProviderDescriptor*> GetSerializableDescriptors() const;

	/**
	 * Build the parent chain from a leaf descriptor to root.
	 * Returns descriptors ordered root-first: [PlayerState, ItemsStoreComponent].
	 */
	TArray<const FArcPlayerProviderDescriptor*> BuildChain(
		const FArcPlayerProviderDescriptor& Leaf) const;

	/**
	 * Build the full domain path for a descriptor by walking its parent chain.
	 * Example: "PlayerState/UArcItemsStoreComponent"
	 */
	FString BuildDomainPath(const FArcPlayerProviderDescriptor& Leaf) const;

private:
	TArray<FArcPlayerProviderDescriptor> Descriptors;
};

// ─────────────────────────────────────────────────────────────────────────────
// Auto-registration base class
// ─────────────────────────────────────────────────────────────────────────────

class ARCPERSISTENCE_API FArcPlayerProviderRegistryDelegates
{
public:
	FArcPlayerProviderRegistryDelegates();
	virtual ~FArcPlayerProviderRegistryDelegates();

	virtual void OnRegisterProviders() {}
	virtual void OnUnregisterProviders() {}
};

// ─────────────────────────────────────────────────────────────────────────────
// Registration macros
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Header macro — declares the auto-registration delegate for a provider.
 *
 * @param TargetType  The UClass this provider resolves (e.g., UArcItemsStoreComponent)
 * @param Api         Module API macro (e.g., ARCCORE_API)
 */
#define UE_ARC_DECLARE_PLAYER_PROVIDER(TargetType, Api) \
	struct Api TargetType##_ProviderDelegates final : public FArcPlayerProviderRegistryDelegates \
	{ \
		TargetType##_ProviderDelegates(); \
		~TargetType##_ProviderDelegates(); \
		virtual void OnRegisterProviders() override; \
		virtual void OnUnregisterProviders() override; \
	};

/**
 * CPP macro — implements the delegate and creates the static auto-registration instance.
 *
 * @param TargetType      The UClass this provider resolves
 * @param DomainSegment   Domain path segment (unquoted identifier, e.g. PlayerState)
 * @param ParentClassExpr Expression evaluating to UClass* of parent (nullptr for root)
 * @param ResolveLambda   Lambda: UObject*(UObject* Parent)
 * @param bIsSerializable true if this is a serializable leaf node
 */
#define UE_ARC_IMPLEMENT_PLAYER_PROVIDER(TargetType, InDomainSegment, ParentClassExpr, ResolveLambda, bIsSerializable) \
	TargetType##_ProviderDelegates::TargetType##_ProviderDelegates() \
	{ \
		OnRegisterProviders(); \
	} \
	TargetType##_ProviderDelegates::~TargetType##_ProviderDelegates() \
	{ \
		OnUnregisterProviders(); \
	} \
	static TargetType##_ProviderDelegates G##TargetType##_ProviderDelegatesInstance; \
	void TargetType##_ProviderDelegates::OnRegisterProviders() \
	{ \
		FArcPlayerProviderDescriptor Desc; \
		Desc.TargetClass = TargetType::StaticClass(); \
		Desc.DomainSegment = TEXT(#InDomainSegment); \
		Desc.ParentClass = ParentClassExpr; \
		Desc.ResolveFunc = ResolveLambda; \
		Desc.bSerializable = bIsSerializable; \
		FArcPlayerProviderRegistry::Get().Register(Desc); \
	} \
	void TargetType##_ProviderDelegates::OnUnregisterProviders() \
	{ \
		FArcPlayerProviderRegistry::Get().Unregister(TargetType::StaticClass()); \
	}
