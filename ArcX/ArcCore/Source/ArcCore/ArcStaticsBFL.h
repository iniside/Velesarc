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


#include "Templates/SubclassOf.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArcStaticsBFL.generated.h"

class UArcUserFacingExperienceDefinition;
class UObject;
class UScriptStruct;

enum class EAnimCardinalDirection : uint8
{
	North
	, East
	, South
	, West
};

/**
 *
 */
UCLASS()
class ARCCORE_API UArcStaticsBFL : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static class UArcItemsComponent* GetItemsComponent(class AActor* InActor)
	{
		return nullptr;
	};

	UFUNCTION(BlueprintCallable
		, meta = (WorldContext = "WorldContextObject", DisplayName ="Open Level (by Primary Asset Id)")
		, Category = "Arc Core|Game")
	static void OpenLevelByPrimaryAssetId(const UObject* WorldContextObject
										  , const FPrimaryAssetId Level
										  , bool bAbsolute = true
										  , FString Options = FString(TEXT("")));

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Arc Core|Game")
	static void OpenLevelFromUserFacingExperience(const UObject* WorldContextObject, UArcUserFacingExperienceDefinition* InDefinition);
	
	static EAnimCardinalDirection GetCardinalDirectionFromAngle(EAnimCardinalDirection PreviousCardinalDirection
																, float AngleInDegrees
																, float DeadZoneAngle)
	{
		// Use deadzone offset to favor backpedal on S & W, and favor frontpedal on N & E
		const float AbsoluteAngle = FMath::Abs(AngleInDegrees);
		if (PreviousCardinalDirection == EAnimCardinalDirection::North)
		{
			if (AbsoluteAngle <= (45.f + DeadZoneAngle + DeadZoneAngle))
			{
				return EAnimCardinalDirection::North;
			}
			else if (AbsoluteAngle >= 135.f - DeadZoneAngle)
			{
				return EAnimCardinalDirection::South;
			}
			else if (AngleInDegrees > 0.f)
			{
				return EAnimCardinalDirection::East;
			}
			return EAnimCardinalDirection::West;
		}
		else if (PreviousCardinalDirection == EAnimCardinalDirection::South)
		{
			if (AbsoluteAngle <= 45.f + DeadZoneAngle)
			{
				return EAnimCardinalDirection::North;
			}
			else if (AbsoluteAngle >= (135.f - DeadZoneAngle - DeadZoneAngle))
			{
				return EAnimCardinalDirection::South;
			}
			else if (AngleInDegrees > 0.f)
			{
				return EAnimCardinalDirection::East;
			}
			return EAnimCardinalDirection::West;
		}

		// East and West
		if (AbsoluteAngle <= (45.f + DeadZoneAngle))
		{
			return EAnimCardinalDirection::North;
		}
		else if (AbsoluteAngle >= (135.f - DeadZoneAngle))
		{
			return EAnimCardinalDirection::South;
		}
		else if (AngleInDegrees > 0.f)
		{
			return EAnimCardinalDirection::East;
		}
		return EAnimCardinalDirection::West;
	}


	UFUNCTION(BlueprintCallable)
	static void SpawnSingleEntity(UObject* ContextObject, UMassEntityConfigAsset* InConfig, FVector InLocation);

	UFUNCTION(BlueprintCallable, CustomThunk, BlueprintInternalUseOnly, Category = "Arc Core|Ability", meta = (CustomStructureParam = "InCommand"))
	static void SendCommandToServer(UObject* ContextObject,
		UPARAM(meta = (MetaStruct = "/Script/ArcCore.ArcReplicatedCommand")) UScriptStruct* InCommandType
		, const int32& InCommand);
	
	DECLARE_FUNCTION(execSendCommandToServer);

	UFUNCTION(BlueprintCallable, Category = "Arc Core", Meta = (ExpandBoolAsExecs = "bSuccess", DeterminesOutputType = "InClass"))
	static UObject* IsObjectChildOf(UObject* InObject, TSubclassOf<UObject> InClass, bool& bSuccess);
};
