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
#include "IDetailCustomization.h"
#include "IDetailCustomNodeBuilder.h"
#include "IDetailGroup.h"
#include "IPropertyTypeCustomization.h"
#include "InstancedStructDetails.h"

class FInstancedStructDetails;


class FArcInstancedStructDataDetails : public FInstancedStructDataDetails
{
	TSharedPtr<IPropertyHandle> MyStructProperty;
	TMap<FName, IDetailGroup*> AddedGroups;
public:
	FArcInstancedStructDataDetails(TSharedPtr<IPropertyHandle> InStructProperty)
		 : FInstancedStructDataDetails(InStructProperty)
	{
	//	bAllowChildPropertyCustomization = true;
		MyStructProperty = InStructProperty;
	}
	//virtual void OnChildPropertyFound(IDetailChildrenBuilder& ChildrenBuilder, TSharedPtr<IPropertyHandle> ChildHandle) override;
};

class FArcInstancedStructDetails : public IPropertyTypeCustomization
{
private:
	TSharedPtr<FInstancedStructDetails> InstancedStructDetails;
	TSharedPtr<IPropertyHandle> ArcStructHandle;
	TSharedPtr<IPropertyHandle> InstanceStructHandle;
	
public:
	FArcInstancedStructDetails();
	
	/** Makes a new instance of this detail layout class for a specific detail view
	 * requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle
								 , class FDetailWidgetRow& HeaderRow
								 , IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle
								   , class IDetailChildrenBuilder& StructBuilder
								   , IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
};

class FArcItemFragment_MakeLocationInfoDetailCustomization : public IDetailCustomization
{
	
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/* IDetailsCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
