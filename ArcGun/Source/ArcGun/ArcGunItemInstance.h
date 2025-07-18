/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
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
#include "Items/ArcItemInstance.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "ArcGunItemInstance.generated.h"

class UArcItemsStoreComponent;
struct FArcItemData;

USTRUCT()
struct ARCGUN_API FArcItemInstance_GunMagazineAmmo : public FArcItemInstance
{
	GENERATED_BODY()
	
protected:
	UPROPERTY()
	int32 CurrentAmmo = 30;

public:
	void SetAmmo(int32 NewAmmo);

	void RemoveAmmo(int32 Removed);

	int32 GetAmmo() const
	{
		return CurrentAmmo;
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcItemInstance_GunMagazineAmmo::StaticStruct();
	}

	virtual bool Equals(const FArcItemInstance& Other) const override
	{
		const FArcItemInstance_GunMagazineAmmo& OtherInstance = static_cast<const FArcItemInstance_GunMagazineAmmo&>(Other);
		return CurrentAmmo == OtherInstance.CurrentAmmo;
	}

	virtual bool ShouldPersist() const override { return true; }
};

USTRUCT(BlueprintType)
struct ARCGUN_API FArcItemFragment_GunMagazineAmmo : public FArcItemFragment_ItemInstanceBase
{
	GENERATED_BODY()

public:
	virtual ~FArcItemFragment_GunMagazineAmmo() override = default;

	virtual UScriptStruct* GetItemInstanceType() const override
	{
		return FArcItemInstance_GunMagazineAmmo::StaticStruct();
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcItemFragment_GunMagazineAmmo::StaticStruct();
	}
};
