// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Singleton registry of classes allowed to participate in actor persistence.
 * Merges entries from UArcPersistenceSettings (ini) and static code registration.
 */
class ARCPERSISTENCE_API FArcPersistenceClassRegistry
{
public:
	static FArcPersistenceClassRegistry& Get();

	/** Register a class for persistence (code-driven). */
	void RegisterClass(const UClass* Class, bool bIncludeChildren = true);

	/** Unregister a class. */
	void UnregisterClass(const UClass* Class);

	/**
	 * Check if a class is allowed for persistence.
	 * Checks code registrations first, then settings entries.
	 * For entries with bIncludeChildren, walks the class hierarchy.
	 */
	bool IsClassAllowed(const UClass* Class) const;

private:
	struct FEntry
	{
		TWeakObjectPtr<const UClass> Class;
		bool bIncludeChildren = true;
	};

	TArray<FEntry> CodeEntries;
};

// ── Free functions for convenience ──────────────────────────────────────────

namespace ArcPersistence
{
	ARCPERSISTENCE_API void RegisterPersistentClass(const UClass* Class, bool bIncludeChildren = true);
	ARCPERSISTENCE_API void UnregisterPersistentClass(const UClass* Class);
	ARCPERSISTENCE_API bool IsClassPersistent(const UClass* Class);
}
