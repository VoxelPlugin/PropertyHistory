// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "PropertyHistoryProcessor.h"
#include "PropertyHistoryUtilities.h"
#include "Editor/MaterialEditor/Private/MaterialEditor.h"
#include "Editor/UnrealEd/Private/Toolkits/SStandaloneAssetEditorToolkitHost.h"
#include "MaterialEditor/DEditorDoubleVectorParameterValue.h"
#include "MaterialEditor/DEditorFontParameterValue.h"
#include "MaterialEditor/DEditorParameterValue.h"
#include "MaterialEditor/DEditorRuntimeVirtualTextureParameterValue.h"
#include "MaterialEditor/DEditorScalarParameterValue.h"
#include "MaterialEditor/DEditorSparseVolumeTextureParameterValue.h"
#include "MaterialEditor/DEditorStaticComponentMaskParameterValue.h"
#include "MaterialEditor/DEditorStaticSwitchParameterValue.h"
#include "MaterialEditor/DEditorTextureCollectionParameterValue.h"
#include "MaterialEditor/DEditorTextureParameterValue.h"
#include "MaterialEditor/DEditorVectorParameterValue.h"
#include "MaterialEditor/MaterialEditorInstanceConstant.h"
#include "MaterialEditor/PreviewMaterial.h"
#include "Materials/MaterialInstanceConstant.h"
#include "StructUtils/InstancedStruct.h"

FPropertyHistoryProcessor::FPropertyHistoryProcessor(
	UObject* Object,
	const TArray<FPropertyData>& Properties,
	const FGuid Guid)
	: Object(Object)
	, Properties(Properties)
	, Guid(Guid)
{
}

bool FPropertyHistoryProcessor::Process(void*& Container)
{
	if (!PreProcess())
	{
		return false;
	}

	Container = Object;

	for (int32 Index = Properties.Num() - 1; Index >= 1; Index--)
	{
		const FPropertyData& Data = Properties[Index];
		FPropertyData& ChildData = Properties[Index - 1];

		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Data.Property))
		{
			if (!ProcessStruct(Container, Data, ChildData))
			{
				return false;
			}

			continue;
		}

		if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Data.Property))
		{
			if (ObjectProperty->HasAllPropertyFlags(CPF_TObjectPtr))
			{
				const TObjectPtr<UObject> ContainerObject = *Data.Property->ContainerPtrToValuePtr<TObjectPtr<UObject>>(Container);
				Container = ContainerObject;
				continue;
			}
		}

		Container = Data.Property->ContainerPtrToValuePtr<void>(Container);

		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Data.Property))
		{
			if (!ProcessArray(Container, ArrayProperty, ChildData))
			{
				return false;
			}
		}
		else if (const FSetProperty* SetProperty = CastField<FSetProperty>(Data.Property))
		{
			FScriptSetHelper SetHelper(SetProperty, Container);
			const int32 SetSize = SetHelper.Num();
			const int32 SetIndex = ChildData.Index;
			if (SetIndex < 0 ||
				SetIndex >= SetSize)
			{
				return false;
			}

			Container = SetHelper.GetElementPtr(SetIndex);
		}
		else if (const FMapProperty* MapProperty = CastField<FMapProperty>(Data.Property))
		{
			FScriptMapHelper MapHelper(MapProperty, Container);
			const int32 MapSize = MapHelper.Num();
			const int32 MapIndex = ChildData.Index;
			if (MapIndex < 0 ||
				MapIndex >= MapSize)
			{
				return false;
			}

			Container = MapHelper.GetPairPtr(MapIndex);
		}
	}

	return PostProcess(Container);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DEFINE_PRIVATE_ACCESS(SStandaloneAssetEditorToolkitHost, HostedAssetEditorToolkit)

bool FPropertyHistoryProcessor::PreProcess()
{
	if (const UMaterialEditorInstanceConstant* MaterialEditorInstance = Cast<UMaterialEditorInstanceConstant>(Object))
	{
		return PreProcessMaterialInstance(MaterialEditorInstance);
	}
	if (const UPreviewMaterial* PreviewMaterial = Cast<UPreviewMaterial>(Object))
	{
		return PreProcessMaterial(PreviewMaterial);
	}
	if (const UMaterialExpression* MaterialExpression = Cast<UMaterialExpression>(Object))
	{
		return PreProcessMaterialExpression(MaterialExpression);
	}

	return true;
}

bool FPropertyHistoryProcessor::PostProcess(void* Container)
{
	if (!bFetchMaterialParameterName)
	{
		return true;
	}

	Object = Cast<UMaterialEditorInstanceConstant>(Object)->SourceInstance;

	Properties = {};

	const UDEditorParameterValue* MaterialParameterValue = static_cast<UDEditorParameterValue*>(Container);
	Guid = MaterialParameterValue->ExpressionId;

	if (MaterialParameterValue->IsA<UDEditorScalarParameterValue>())
	{
		Properties.Add({ &FindFPropertyChecked(FScalarParameterValue, ParameterValue) });
		Properties.Add({ &FindFPropertyChecked(UMaterialInstance, ScalarParameterValues) });
	}
	else if (MaterialParameterValue->IsA<UDEditorVectorParameterValue>())
	{
		Properties.Add({ &FindFPropertyChecked(FVectorParameterValue, ParameterValue) });
		Properties.Add({ &FindFPropertyChecked(UMaterialInstance, VectorParameterValues) });
	}
	else if (MaterialParameterValue->IsA<UDEditorDoubleVectorParameterValue>())
	{
		Properties.Add({ &FindFPropertyChecked(FDoubleVectorParameterValue, ParameterValue) });
		Properties.Add({ &FindFPropertyChecked(UMaterialInstance, DoubleVectorParameterValues) });
	}
	else if (MaterialParameterValue->IsA<UDEditorTextureParameterValue>())
	{
		Properties.Add({ &FindFPropertyChecked(FTextureParameterValue, ParameterValue) });
		Properties.Add({ &FindFPropertyChecked(UMaterialInstance, TextureParameterValues) });
	}
	else if (MaterialParameterValue->IsA<UDEditorTextureCollectionParameterValue>())
	{
		Properties.Add({ &FindFPropertyChecked(FTextureCollectionParameterValue, ParameterValue) });
		Properties.Add({ &FindFPropertyChecked(UMaterialInstance, TextureCollectionParameterValues) });
	}
	else if (MaterialParameterValue->IsA<UDEditorFontParameterValue>())
	{
		Properties.Add({ &FindFPropertyChecked(FFontParameterValue, FontValue) });
		Properties.Add({ &FindFPropertyChecked(UMaterialInstance, FontParameterValues) });
	}
	else if (MaterialParameterValue->IsA<UDEditorRuntimeVirtualTextureParameterValue>())
	{
		Properties.Add({ &FindFPropertyChecked(FRuntimeVirtualTextureParameterValue, ParameterValue) });
		Properties.Add({ &FindFPropertyChecked(UMaterialInstance, RuntimeVirtualTextureParameterValues) });
	}
	else if (MaterialParameterValue->IsA<UDEditorSparseVolumeTextureParameterValue>())
	{
		Properties.Add({ &FindFPropertyChecked(FSparseVolumeTextureParameterValue, ParameterValue) });
		Properties.Add({ &FindFPropertyChecked(UMaterialInstance, SparseVolumeTextureParameterValues) });
	}
	else if (MaterialParameterValue->IsA<UDEditorStaticSwitchParameterValue>())
	{
		Properties.Add({ &FindFPropertyChecked(FStaticSwitchParameter, Value) });
		Properties.Add({ &FindFPropertyChecked(FStaticParameterSetRuntimeData, StaticSwitchParameters) });
		Properties.Add({ &FindFPropertyChecked_ByName(UMaterialInstance, "StaticParametersRuntime") });
	}
	else if (MaterialParameterValue->IsA<UDEditorStaticComponentMaskParameterValue>())
	{
		Properties.Add({ &FindFPropertyChecked(FStaticParameterSetEditorOnlyData, StaticComponentMaskParameters) });
		Properties.Add({ &FindFPropertyChecked(UMaterialInstanceEditorOnlyData, StaticParameters) });
		Properties.Add({ &FindFPropertyChecked_ByName(UMaterialInstance, "EditorOnlyData") });
	}
	else
	{
		ensure(false);
		return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FPropertyHistoryProcessor::ProcessStruct(
	void*& Container,
	const FPropertyData& Data,
	FPropertyData& ChildData)
{
	static UScriptStruct* StampRefStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/Voxel.VoxelStampRef"));
	static UScriptStruct* StampStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/Voxel.VoxelStamp"));
	static UScriptStruct* ParameterOverridesStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/VoxelGraph.VoxelParameterOverrides"));

	const FStructProperty* Property = CastField<FStructProperty>(Data.Property);

	if (Property->Struct == FInstancedStruct::StaticStruct())
	{
		FInstancedStruct* InstancedStruct = Property->ContainerPtrToValuePtr<FInstancedStruct>(Container);
		if (!InstancedStruct->IsValid())
		{
			return false;
		}

		if (const FStructProperty* ChildStructProperty = CastField<FStructProperty>(ChildData.Property))
		{
			if (ChildStructProperty->Struct != FInstancedStruct::StaticStruct() &&
				ChildStructProperty->Struct != InstancedStruct->GetScriptStruct())
			{
				return false;
			}
		}

		Container = InstancedStruct->GetMutableMemory();
		if (!Container)
		{
			return false;
		}

		return true;
	}

	if (Property->Struct == StampRefStruct)
	{
		struct FVoxelVirtualStructAccessor
		{
			uint8 Padding[16];
			UScriptStruct* PrivateStruct;
		};
		struct FVoxelStampRefInnerAccessor : public TSharedFromThis<FVoxelStampRefInnerAccessor>
		{
			TSharedPtr<FVoxelVirtualStructAccessor> Stamp;
		};

		const TSharedRef<FVoxelStampRefInnerAccessor> StampRefAccessor = *Property->ContainerPtrToValuePtr<TSharedRef<FVoxelStampRefInnerAccessor>>(Container);
		Container = StampRefAccessor->Stamp.Get();

		// Necessary to get the struct type, if during changes struct type has changed.
		// StampStruct should have alignment of 16
		if (ensure(StampStruct->GetMinAlignment() == 16))
		{
			TargetStampStruct = StampRefAccessor->Stamp->PrivateStruct;
		}
		return true;
	}

	if (Property->Struct == ParameterOverridesStruct)
	{
		if (TargetStampStruct != Property->GetOwnerStruct())
		{
			return false;
		}

		if (!Guid.IsValid())
		{
			// Not supported
			return false;
		}

		Container = Property->ContainerPtrToValuePtr<void>(Container);

		UScriptStruct* ParameterValueOverrideStruct = nullptr;
		for (const FMapProperty* MapProperty : TFieldRange<FMapProperty>(ParameterOverridesStruct))
		{
			const FStructProperty* StructKeyProperty = CastField<FStructProperty>(MapProperty->KeyProp);
			const FStructProperty* StructValueProperty = CastField<FStructProperty>(MapProperty->ValueProp);
			if (!StructKeyProperty ||
				!StructValueProperty ||
				StructKeyProperty->Struct != TBaseStructure<FGuid>::Get())
			{
				continue;
			}

			Container = MapProperty->ContainerPtrToValuePtr<void>(Container);

			FScriptMapHelper MapHelper(MapProperty, Container);
			const int32 PairIndex = MapHelper.FindMapIndexWithKey(&Guid);
			if (PairIndex == -1)
			{
				return false;
			}

			Container = MapHelper.GetValuePtr(PairIndex);
			ParameterValueOverrideStruct = StructValueProperty->Struct;
			break;
		}

		// Reassign FVoxelPinValue, because it was from FStructOnScope, which had no proper offset
		if (const FStructProperty* ChildStructProperty = CastField<FStructProperty>(ChildData.Property))
		{
			for (const FStructProperty* StructProperty : TFieldRange<FStructProperty>(ParameterValueOverrideStruct))
			{
				if (StructProperty->Struct == ChildStructProperty->Struct)
				{
					ChildData.Property = StructProperty;
					break;
				}
			}
		}

		return true;
	}

	/*if (Property->Struct->IsChildOf(StampStruct))
	{
		if (TargetStampStruct != Property->Struct)
		{
			return false;
		}
	}*/

	Container = Property->ContainerPtrToValuePtr<void>(Container);
	return true;
}

bool FPropertyHistoryProcessor::ProcessArray(
	void*& Container,
	const FArrayProperty* Property,
	const FPropertyData& ChildData) const
{
	FScriptArrayHelper ArrayHelper(Property, Container);
	const int32 ArraySize = ArrayHelper.Num();

	if (const FProperty* ComparisonProperty = GetMaterialParameterComparisonProperty(Property))
	{
		for (int32 Index = 0; Index < ArraySize; Index++)
		{
			uint8* ElementPtr = ArrayHelper.GetElementPtr(Index);
			if (ComparisonProperty->Identical(ComparisonProperty->ContainerPtrToValuePtr<void>(ElementPtr), &Guid))
			{
				Container = ElementPtr;
				return true;
			}
		}

		return false;
	}

	const int32 ArrayIndex = ChildData.Index;
	if (ArrayIndex < 0 ||
		ArrayIndex >= ArraySize)
	{
		return false;
	}

	Container = ArrayHelper.GetElementPtr(ArrayIndex);
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FPropertyHistoryProcessor::PreProcessMaterialInstance(const UMaterialEditorInstanceConstant* MaterialEditorInstance)
{
	if (!MaterialEditorInstance ||
		!MaterialEditorInstance->SourceInstance)
	{
		return false;
	}

	FPropertyData& RootProperty = Properties.Last();

	if (RootProperty.Property == &FindFPropertyChecked(UMaterialEditorInstanceConstant, ParameterGroups))
	{
		if (Properties.Num() != 5 ||
			Properties[0].Property->GetFName() != "ParameterValue")
		{
			return false;
		}

		bFetchMaterialParameterName = true;

		Properties[0].Property = &FindFPropertyChecked(UDEditorParameterValue, ParameterInfo);
		return true;
	}

	if (Properties.Num() > 1)
	{
		if (RootProperty.Property != &FindFPropertyChecked(UMaterialEditorInstanceConstant, BasePropertyOverrides) &&
			RootProperty.Property != &FindFPropertyChecked(UMaterialEditorInstanceConstant, LightmassSettings))
		{
			return false;
		}
	}

	Object = MaterialEditorInstance->SourceInstance;

	const FProperty* Property = FindFProperty<FProperty>(Object->GetClass(), RootProperty.Property->GetFName());
	if (!Property)
	{
		return false;
	}

	if (RootProperty.Property->SameType(Property))
	{
		RootProperty.Property = Property;
		return true;
	}

	if (RootProperty.Property != &FindFPropertyChecked(UMaterialEditorInstanceConstant, LightmassSettings) ||
		Properties.Num() != 3)
	{
		return false;
	}

	if (Properties.Num() != 3)
	{
		return false;
	}

	RootProperty.Property = Property;

	// First property is ParameterValue, we need the name so it's second property
	FPropertyData& LeafProperty = Properties[1];
	const FProperty* LightmassProperty = FindFProperty<FProperty>(FLightmassMaterialInterfaceSettings::StaticStruct(), LeafProperty.Property->GetFName());
	if (!LightmassProperty)
	{
		// Handle bool properties
		LightmassProperty = FindFProperty<FProperty>(FLightmassMaterialInterfaceSettings::StaticStruct(), FName("b" + LeafProperty.Property->GetName()));
		if (!LightmassProperty)
		{
			return false;
		}
	}

	LeafProperty.Property = LightmassProperty;

	Properties.RemoveAt(0);
	return true;
}

bool FPropertyHistoryProcessor::PreProcessMaterial(const UPreviewMaterial* PreviewMaterial)
{
	if (!DetailsView)
	{
		return false;
	}

	TSharedPtr<SWidget> ParentWidget = DetailsView->GetParentWidget();
	while (ParentWidget)
	{
		if (ParentWidget->GetTypeAsString() == "SStandaloneAssetEditorToolkitHost")
		{
			const TSharedPtr<FAssetEditorToolkit> Toolkit = PrivateAccess::HostedAssetEditorToolkit(*StaticCastSharedPtr<SStandaloneAssetEditorToolkitHost>(ParentWidget));
			if (!Toolkit ||
				Toolkit->GetToolkitFName() != "MaterialEditor")
			{
				return false;
			}

			const TSharedPtr<FMaterialEditor> MaterialEditor = StaticCastSharedPtr<FMaterialEditor>(Toolkit);
			if (!MaterialEditor ||
				MaterialEditor->Material != PreviewMaterial)
			{
				return false;
			}

			Object = MaterialEditor->OriginalMaterial;
			return true;
		}

		ParentWidget = ParentWidget->GetParentWidget();
	}

	return false;
}

bool FPropertyHistoryProcessor::PreProcessMaterialExpression(const UMaterialExpression* MaterialExpression)
{
	if (!MaterialExpression->GetOutermost()->HasAllFlags(RF_Transient))
	{
		return true;
	}

	if (!DetailsView)
	{
		return false;
	}

	TSharedPtr<SWidget> ParentWidget = DetailsView->GetParentWidget();
	while (ParentWidget)
	{
		if (ParentWidget->GetTypeAsString() == "SStandaloneAssetEditorToolkitHost")
		{
			const TSharedPtr<FAssetEditorToolkit> Toolkit = PrivateAccess::HostedAssetEditorToolkit(*StaticCastSharedPtr<SStandaloneAssetEditorToolkitHost>(ParentWidget));
			if (!Toolkit ||
				Toolkit->GetToolkitFName() != "MaterialEditor")
			{
				return false;
			}

			const TSharedPtr<FMaterialEditor> MaterialEditor = StaticCastSharedPtr<FMaterialEditor>(Toolkit);
			if (!MaterialEditor ||
				MaterialEditor->Material != MaterialExpression->GetTypedOuter<UMaterial>())
			{
				return false;
			}

			const UMaterial* Material = MaterialEditor->OriginalMaterial;
			for (UMaterialExpression* Expression : Material->GetExpressions())
			{
				if (Expression &&
					Expression->MaterialExpressionGuid == MaterialExpression->MaterialExpressionGuid)
				{
					Object = Expression;
					return true;
				}
			}

			return false;
		}

		ParentWidget = ParentWidget->GetParentWidget();
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FProperty* FPropertyHistoryProcessor::GetMaterialParameterComparisonProperty(const FArrayProperty* Property)
{
	if (Property == &FindFPropertyChecked(UMaterialInstance, ScalarParameterValues))
	{
		return &FindFPropertyChecked(FScalarParameterValue, ExpressionGUID);
	}
	if (Property == &FindFPropertyChecked(UMaterialInstance, DoubleVectorParameterValues))
	{
		return &FindFPropertyChecked(FDoubleVectorParameterValue, ExpressionGUID);
	}
	if (Property == &FindFPropertyChecked(UMaterialInstance, TextureParameterValues))
	{
		return &FindFPropertyChecked(FTextureParameterValue, ExpressionGUID);
	}
	if (Property == &FindFPropertyChecked(UMaterialInstance, TextureCollectionParameterValues))
	{
		return &FindFPropertyChecked(FTextureCollectionParameterValue, ExpressionGUID);
	}
	if (Property == &FindFPropertyChecked(UMaterialInstance, FontParameterValues))
	{
		return &FindFPropertyChecked(FFontParameterValue, ExpressionGUID);
	}
	if (Property == &FindFPropertyChecked(UMaterialInstance, RuntimeVirtualTextureParameterValues))
	{
		return &FindFPropertyChecked(FRuntimeVirtualTextureParameterValue, ExpressionGUID);
	}
	if (Property == &FindFPropertyChecked(UMaterialInstance, SparseVolumeTextureParameterValues))
	{
		return &FindFPropertyChecked(FSparseVolumeTextureParameterValue, ExpressionGUID);
	}
	if (Property == &FindFPropertyChecked(FStaticParameterSetRuntimeData, StaticSwitchParameters))
	{
		return &FindFPropertyChecked(FStaticSwitchParameter, ExpressionGUID);
	}
	if (Property == &FindFPropertyChecked(FStaticParameterSetEditorOnlyData, StaticComponentMaskParameters))
	{
		return &FindFPropertyChecked(FStaticComponentMaskParameter, ExpressionGUID);
	}

	return nullptr;
}