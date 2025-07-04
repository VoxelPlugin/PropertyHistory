// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PropertyHistoryUtilities.h"

class UPreviewMaterial;
class UMaterialEditorInstanceConstant;

struct FPropertyData
{
	const FProperty* Property;
	int32 Index = -1;
};

struct FPropertyHistoryProcessor
{
public:
	FPropertyHistoryProcessor(
		UObject* Object,
		const TArray<FPropertyData>& Properties,
		FGuid Guid = {});

	bool Process(void*& Container);

private:
	bool PreProcess();
	bool PostProcess(void* Container);

private:
	bool ProcessStruct(
		void*& Container,
		const FPropertyData& Data,
		FPropertyData& ChildData);
	bool ProcessArray(
		void*& Container,
		const FArrayProperty* Property,
		const FPropertyData& ChildData) const;

private:
	bool PreProcessMaterialInstance(const UMaterialEditorInstanceConstant* MaterialEditorInstance);
	bool PreProcessMaterial(const UPreviewMaterial* PreviewMaterial);
	bool PreProcessMaterialExpression(const UMaterialExpression* MaterialExpression);

private:
	static FProperty* GetMaterialParameterComparisonProperty(const FArrayProperty* Property);

public:
	UObject* Object;
	TArray<FPropertyData> Properties;

	FGuid Guid;

#if PROPERTY_HISTORY_ENGINE_VERSION >= 506
	TSharedPtr<IDetailsView> DetailsView;
#else
	IDetailsView* DetailsView = nullptr;
#endif

private:
	UScriptStruct* TargetStampStruct = nullptr;
	bool bFetchMaterialParameterName = false;
};