// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcPersistenceClassRegistry.h"

#include "ArcPersistenceSettings.h"
#include "UObject/Class.h"

FArcPersistenceClassRegistry& FArcPersistenceClassRegistry::Get()
{
	static FArcPersistenceClassRegistry Instance;
	return Instance;
}

void FArcPersistenceClassRegistry::RegisterClass(const UClass* Class, bool bIncludeChildren)
{
	if (!Class)
	{
		return;
	}

	for (const FEntry& Existing : CodeEntries)
	{
		if (Existing.Class.Get() == Class)
		{
			return;
		}
	}

	CodeEntries.Add({ Class, bIncludeChildren });
}

void FArcPersistenceClassRegistry::UnregisterClass(const UClass* Class)
{
	CodeEntries.RemoveAll([Class](const FEntry& E) { return E.Class.Get() == Class; });
}

bool FArcPersistenceClassRegistry::IsClassAllowed(const UClass* Class) const
{
	if (!Class)
	{
		return false;
	}

	// Check code registrations
	for (const FEntry& Entry : CodeEntries)
	{
		const UClass* EntryClass = Entry.Class.Get();
		if (!EntryClass)
		{
			continue;
		}

		if (Entry.bIncludeChildren)
		{
			if (Class->IsChildOf(EntryClass))
			{
				return true;
			}
		}
		else
		{
			if (Class == EntryClass)
			{
				return true;
			}
		}
	}

	// Check settings entries
	if (const UArcPersistenceSettings* Settings = GetDefault<UArcPersistenceSettings>())
	{
		for (const FArcPersistenceClassEntry& Entry : Settings->AllowedClasses)
		{
			const UClass* EntryClass = Entry.Class.LoadSynchronous();
			if (!EntryClass)
			{
				continue;
			}

			if (Entry.bIncludeChildren)
			{
				if (Class->IsChildOf(EntryClass))
				{
					return true;
				}
			}
			else
			{
				if (Class == EntryClass)
				{
					return true;
				}
			}
		}
	}

	return false;
}

// ── Free functions ──────────────────────────────────────────────────────────

namespace ArcPersistence
{
	void RegisterPersistentClass(const UClass* Class, bool bIncludeChildren)
	{
		FArcPersistenceClassRegistry::Get().RegisterClass(Class, bIncludeChildren);
	}

	void UnregisterPersistentClass(const UClass* Class)
	{
		FArcPersistenceClassRegistry::Get().UnregisterClass(Class);
	}

	bool IsClassPersistent(const UClass* Class)
	{
		return FArcPersistenceClassRegistry::Get().IsClassAllowed(Class);
	}
}
