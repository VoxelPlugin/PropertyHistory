// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "CoreMinimal.h"
#include "DetailGroup.h"
#include "DetailPropertyRow.h"
#include "PropertyEditorHelpers.h"
#include "DetailCategoryBuilderImpl.h"
#include "Editor/PropertyEditor/Private/PropertyHandleImpl.h"
#include "Editor/PropertyEditor/Private/SDetailSingleItemRow.h"

TSharedPtr<FPropertyNode> FDetailGroup::GetHeaderPropertyNode() const
{
	return HeaderCustomization.IsValid() ? HeaderCustomization->GetPropertyNode() : nullptr;
}

TSharedPtr<IPropertyHandle> FDetailCustomBuilderRow::GetPropertyHandle() const
{
	return CustomNodeBuilder->GetPropertyHandle();
}

TSharedPtr<FPropertyNode> FDetailLayoutCustomization::GetPropertyNode() const
{
	return PropertyRow.IsValid() ? PropertyRow->GetPropertyNode() : nullptr;
}

TSharedPtr<FPropertyNode> SDetailSingleItemRow::GetPropertyNode() const
{
	TSharedPtr<FPropertyNode> PropertyNode = Customization->GetPropertyNode();
	if (!PropertyNode.IsValid() && Customization->DetailGroup.IsValid())
	{
		PropertyNode = Customization->DetailGroup->GetHeaderPropertyNode();
	}

	// See if a custom builder has an associated node
	if (!PropertyNode.IsValid() && Customization->HasCustomBuilder())
	{
		const TSharedPtr<IPropertyHandle> PropertyHandle = Customization->CustomBuilderRow->GetPropertyHandle();
		if (PropertyHandle.IsValid())
		{
			PropertyNode = StaticCastSharedPtr<FPropertyHandleBase>(PropertyHandle)->GetPropertyNode();
		}
	}

	return PropertyNode;
}

bool FPropertyNode::GetReadAddress(bool InRequiresSingleSelection,
								   FReadAddressList& OutAddresses,
								   bool bComparePropertyContents,
								   bool bObjectForceCompare,
								   bool bArrayPropertiesCanDifferInSize) const

{
	if (!ParentNodeWeakPtr.IsValid())
	{
		return false;
	}

	// @todo PropertyEditor Nodes which require validation cannot be cached
	if( CachedReadAddresses.Num() && !CachedReadAddresses.bRequiresCache && !HasNodeFlags(EPropertyNodeFlags::RequiresValidation) )
	{
		OutAddresses.ReadAddressListData = &CachedReadAddresses;
		return CachedReadAddresses.bAllValuesTheSame;
	}

	CachedReadAddresses.Reset();

	bool bAllValuesTheSame = GetReadAddressUncached( *this, InRequiresSingleSelection, &CachedReadAddresses, bComparePropertyContents, bObjectForceCompare, bArrayPropertiesCanDifferInSize );
	OutAddresses.ReadAddressListData = &CachedReadAddresses;
	CachedReadAddresses.bAllValuesTheSame = bAllValuesTheSame;
	CachedReadAddresses.bRequiresCache = false;

	return bAllValuesTheSame;
}

FPropertyAccess::Result FPropertyNode::GetSingleObject(UObject*& OutObject) const
{
	OutObject = nullptr;
	FReadAddressList ReadAddresses;
	bool bAllValuesTheSame = GetReadAddress(HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), ReadAddresses, false, true);

	if ((ReadAddresses.Num() > 0 && bAllValuesTheSame) || ReadAddresses.Num() == 1)
	{
		OutObject = (UObject*)ReadAddresses.GetObject(0);

		return FPropertyAccess::Success;
	}

	return ReadAddresses.Num() > 1 ? FPropertyAccess::MultipleValues : FPropertyAccess::Fail;
}