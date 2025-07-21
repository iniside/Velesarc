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
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Iris/ReplicationState/Private/IrisFastArraySerializerInternal.h"
#include "Iris/Serialization/NetSerializer.h"
#include "Iris/Serialization/NetSerializerConfig.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/Serialization/NetSerializers.h"
#include "Iris/Serialization/PolymorphicNetSerializer.h"
#include "Iris/Serialization/PolymorphicNetSerializerImpl.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemData.h"

#include "ArcItemIdItemNetSerializer.generated.h"

USTRUCT()
struct FArcItemIdItemNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()
};

namespace Arcx::Net
{
	// UE_NET_DECLARE_SERIALIZER(FArcItemIdItemNetSerializer, ARCCORE_API);
}
