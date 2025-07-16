/**
 * This file is part of ArcX.
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
#include "Engine/EngineBaseTypes.h"
#include "ArcExperienceBundleState.generated.h"

USTRUCT()
struct ARCCORE_API FArcExperienceBundleState
{
	GENERATED_BODY()
	
protected:
	// Always added, regardless of NetMode.
	UPROPERTY(EditAnywhere)
	TArray<FName> Bundles;

	// Added on clients, standalone and listen server.
	UPROPERTY(EditAnywhere)
	TArray<FName> ClientBundles;

	// Added on servers, standalone and listen server.
	UPROPERTY(EditAnywhere)
	TArray<FName> ServerBundles;

public:
	FArcExperienceBundleState()
	{
		Bundles.AddUnique("Game");
		ClientBundles.AddUnique("Client");
		ServerBundles.AddUnique("Server");
	};
	
	
	TArray<FName> GetBundles(ENetMode InNetMode) const
	{
		TArray<FName> OutBundles;
		if (InNetMode == ENetMode::NM_Standalone || InNetMode == ENetMode::NM_ListenServer)
		{
			OutBundles.Append(Bundles);
			OutBundles.Append(ClientBundles);
			OutBundles.Append(ServerBundles);
		}
		else if (InNetMode == ENetMode::NM_Client)
		{
			OutBundles.Append(Bundles);
			OutBundles.Append(ClientBundles);
		}
		else if (InNetMode == ENetMode::NM_DedicatedServer)
		{
			OutBundles.Append(Bundles);
			OutBundles.Append(ServerBundles);
		}

		return OutBundles;
	}
};
