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

#include "Engine/DataAsset.h"
#include "StructUtils/InstancedStruct.h"
#include "Pawn/ArcMassPawnDataFragments.h"

#include "ArcMassPawnData.generated.h"

class APawn;
class UCameraRigAsset;
class UArcMassInputActionConfig;

/**
 * UArcMassPawnData
 *
 * PrimaryDataAsset describing actor-side initialization for a Mass-driven pawn:
 * which pawn class to spawn, which components to add, init fragments to run, and
 * the default camera rig to activate. Entity-side fragments are configured
 * independently by Mass traits in consumer plugins.
 */
UCLASS(BlueprintType, Const, Meta = (DisplayName = "Arc Mass Pawn Data", ShortTooltip = "Configures actor-side initialization for a Mass-driven pawn."))
class ARCMASSGAMEPLAYFRAMEWORK_API UArcMassPawnData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UArcMassPawnData(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Class to spawn for this pawn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ArcMass|Pawn")
	TSoftClassPtr<APawn> PawnClass;

	/** Components to add to the pawn during DataInitialized. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ArcMass|Pawn", meta = (BaseStruct = "/Script/ArcMassGameplayFramework.ArcMassAddPawnComponent"))
	TArray<FInstancedStruct> Components;

	/** Actor-side init steps run during DataInitialized. Receives optional entity handle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ArcMass|Pawn", meta = (BaseStruct = "/Script/ArcMassGameplayFramework.ArcMassPawnInitFragment"))
	TArray<FInstancedStruct> InitFragments;

	/** Default camera rig activated on the pawn's UGameplayCameraComponent during DataInitialized. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ArcMass|Camera")
	TSoftObjectPtr<UCameraRigAsset> DefaultCameraRig;

	/** Input action configs bound by UArcMassPawnExtensionComponent during DataInitialized. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ArcMass|Input")
	TArray<TObjectPtr<UArcMassInputActionConfig>> InputConfig;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITORONLY_DATA
	virtual void UpdateAssetBundleData() override;
#endif
};
