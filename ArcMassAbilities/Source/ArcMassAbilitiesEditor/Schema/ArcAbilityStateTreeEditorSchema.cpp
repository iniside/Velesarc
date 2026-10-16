// Copyright Lukasz Baran. All Rights Reserved.

#include "Schema/ArcAbilityStateTreeEditorSchema.h"
#include "Extensions/ArcAbilityManifestEditorExtension.h"
#include "Extensions/ArcAbilityPresetEditorExtension.h"
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAbilityStateTreeEditorSchema)

void UArcAbilityStateTreeEditorSchema::Validate(TNotNull<UStateTree*> StateTree)
{
	Super::Validate(StateTree);

	UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
	if (!EditorData)
	{
		return;
	}

	// GetOrCreateExtension is idempotent: returns existing if present, NewObjects + appends otherwise.
	UArcAbilityManifestEditorExtension* ManifestExtension = EditorData->GetOrCreateExtension<UArcAbilityManifestEditorExtension>();
	ManifestExtension->SetFlags(RF_Transactional);

	UArcAbilityPresetEditorExtension* PresetExtension = EditorData->GetOrCreateExtension<UArcAbilityPresetEditorExtension>();
	// GetOrCreateExtension creates the object without RF_Transactional, so Modify() would be a no-op without this.
	PresetExtension->SetFlags(RF_Transactional);
}
