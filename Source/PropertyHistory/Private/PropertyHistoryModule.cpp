// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "CoreMinimal.h"
#include "ToolMenus.h"
#include "SPropertyHistory.h"
#include "DetailRowMenuContext.h"
#include "PropertyHistoryHandler.h"
#include "PropertyHistoryProcessor.h"
#include "PropertyHistoryUtilities.h"
#include "RevisionControlStyle/RevisionControlStyle.h"
#include "Editor/PropertyEditor/Private/PropertyHandleImpl.h"
#include "Editor/PropertyEditor/Private/SDetailSingleItemRow.h"
#include "Editor/PropertyEditor/Private/DetailRowMenuContextPrivate.h"

DEFINE_PRIVATE_ACCESS(FPropertyNode, InstanceMetaData)
DEFINE_PRIVATE_ACCESS(SDetailsViewBase, DetailLayouts)
DEFINE_PRIVATE_ACCESS(SDetailTableRowBase, OwnerTreeNode)
DEFINE_PRIVATE_ACCESS_FUNCTION(SDetailSingleItemRow, GetPropertyNode);

UClass* UDetailRowMenuContextPrivate::GetPrivateStaticClass()
{
	static UClass* Class = FindObjectChecked<UClass>(nullptr, TEXT("/Script/PropertyEditor.DetailRowMenuContextPrivate"));
	check(Class);
	return Class;
}

class FPropertyHistoryModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		{
			const TSharedRef<FGlobalTabmanager> TabManager = FGlobalTabmanager::Get();

			TabManager->RegisterNomadTabSpawner("PropertyHistoryTab", MakeLambdaDelegate([=](const FSpawnTabArgs& SpawnTabArgs)
			{
				return
					SNew(SDockTab)
					.TabRole(NomadTab)
					.Label(INVTEXT("Property History"))
					.ToolTipText(INVTEXT("Shows history of Property, using Source Control"))
					[
						SNew(SPropertyHistory)
					];
			}))
			.SetDisplayName(INVTEXT("Property History"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "RevisionControl.Actions.History"));
		}

		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(UE::PropertyEditor::RowContextMenuName);

		Menu->AddDynamicSection(NAME_None, MakeLambdaDelegate([](UToolMenu* ToolMenu)
		{
			const UDetailRowMenuContext* Context = ToolMenu->FindContext<UDetailRowMenuContext>();
			const UDetailRowMenuContextPrivate* ContextPrivate = ToolMenu->FindContext<UDetailRowMenuContextPrivate>();
			if (!Context ||
				!ContextPrivate)
			{
				return;
			}

			const TSharedPtr<SDetailTableRowBase> RowBase = ContextPrivate->Row.Pin();
			if (!RowBase)
			{
				return;
			}

			const TSharedPtr<FDetailTreeNode> TreeNode = PrivateAccess::OwnerTreeNode(*RowBase).Pin();
			if (!TreeNode ||
				TreeNode->GetNodeType() != EDetailNodeType::Item)
			{
				return;
			}

			const TSharedPtr<SDetailSingleItemRow> Row = StaticCastSharedPtr<SDetailSingleItemRow>(RowBase);
			if (!Row)
			{
				return;
			}

			TSharedPtr<FPropertyNode> Node = PrivateAccess::GetPropertyNode(*Row)();
			if (!Node)
			{
				if (Context->PropertyHandles.Num() == 0)
				{
					return;
				}

				const TSharedPtr<FPropertyHandleBase> PropertyHandle = StaticCastSharedPtr<FPropertyHandleBase>(Context->PropertyHandles[0]);
				if (!PropertyHandle)
				{
					return;
				}

				Node = PropertyHandle->GetPropertyNode();
				if (!Node)
				{
					return;
				}
			}

			TArray<FPropertyData> Properties;
			FString PropertyChainString;
			FGuid PropertyGuid;
			{
				TSharedPtr<FPropertyNode> LocalNode = Node;
				while (LocalNode)
				{
					const FProperty* Property = LocalNode->GetProperty();
					if (!Property)
					{
						LocalNode = LocalNode->GetParentNodeSharedPtr();
						continue;
					}

					Node = LocalNode;
					Properties.Add({ Property, LocalNode->GetArrayIndex() });
					if (const FString* PropertyGuidPtr = PrivateAccess::InstanceMetaData(*LocalNode).Find("PropertyGuid"))
					{
						FGuid::Parse(*PropertyGuidPtr, PropertyGuid);
					}
					if (const FString* PropertyChainPtr = PrivateAccess::InstanceMetaData(*LocalNode).Find("VoxelPropertyChain"))
					{
						PropertyChainString = *PropertyChainPtr;
					}
					LocalNode = LocalNode->GetParentNodeSharedPtr();
				}
			}

			if (!PropertyChainString.IsEmpty())
			{
				TArray<FString> ParsedChainNodes;
				PropertyChainString.ParseIntoArray(ParsedChainNodes, TEXT(";;"));

				int32 NumAddedProperties = 0;
				for (const FString& NodeData : ParsedChainNodes)
				{
					TArray<FString> Parts;
					NodeData.ParseIntoArray(Parts, TEXT("|"));
					if (!ensure(Parts.Num() == 3))
					{
						continue;
					}

					const UStruct* OwnerProperty = FindObject<UStruct>(nullptr, *Parts[0]);
					if (!OwnerProperty)
					{
						NumAddedProperties = 0;
						break;
					}

					const FProperty* Property = FindFProperty<FProperty>(OwnerProperty, *Parts[1]);
					if (!Property)
					{
						NumAddedProperties = 0;
						break;
					}

					int32 ArrayIndex = -1;
					LexFromString(ArrayIndex, Parts[2]);

					Properties.Add({ Property, ArrayIndex });
					NumAddedProperties++;
				}

				if (NumAddedProperties > 0)
				{
					const FPropertyData& RootProperty = Properties.Last();
#if PROPERTY_HISTORY_ENGINE_VERSION >= 506
					const TSharedPtr<SDetailsViewBase> DetailsViewBase = StaticCastSharedPtr<SDetailsViewBase>(Context->DetailsView.Pin());
#else
					SDetailsViewBase* DetailsViewBase = reinterpret_cast<SDetailsViewBase*>(Context->DetailsView);
#endif
					const TSharedPtr<FPropertyNode> RootNode = INLINE_LAMBDA -> TSharedPtr<FPropertyNode>
					{
						if (!DetailsViewBase)
						{
							return nullptr;
						}

						for (const FDetailLayoutData& DetailLayout : PrivateAccess::DetailLayouts(*DetailsViewBase))
						{
							const TMap<FName, FPropertyNodeMap>* PropertyMapPtr = DetailLayout.ClassToPropertyMap.Find(RootProperty.Property->GetOwner<UStruct>()->GetFName());
							if (!PropertyMapPtr)
							{
								continue;
							}

							for (const auto& It : *PropertyMapPtr)
							{
								const TSharedPtr<FPropertyNode> PropertyNode = It.Value.PropertyNameToNode.FindRef(RootProperty.Property->GetFName());
								if (!PropertyNode)
								{
									continue;
								}

								return PropertyNode;
							}
						}

						return nullptr;
					};

					if (RootNode)
					{
						Node = RootNode;
					}
				}
			}

			if (Properties.Num() == 0)
			{
				return;
			}

			UObject* Object;
			{
				FReadAddressList ReadAddresses;
				const bool bAllValuesTheSame = Node->GetReadAddress(false, ReadAddresses, false, false);
				if (ReadAddresses.Num() == 1 ||
					(ReadAddresses.Num() > 0 && bAllValuesTheSame))
				{
					Object = const_cast<UObject*>(ReadAddresses.GetObject(0));

					// Ensure that all objects are the same
					for (int32 Index = 1; Index < ReadAddresses.Num(); Index++)
					{
						const UObject* TargetObject = ReadAddresses.GetObject(Index);
						if (Object != TargetObject)
						{
							return;
						}
					}
				}
				else
				{
					return;
				}
			}

			if (!Object)
			{
				return;
			}

			UClass* OwnerClass = Cast<UClass>(Properties.Last().Property->GetOwnerUObject());
			if (!OwnerClass ||
				!Object->IsA(OwnerClass))
			{
				return;
			}

			FPropertyHistoryProcessor Processor(Object, Properties, PropertyGuid);
#if PROPERTY_HISTORY_ENGINE_VERSION >= 506
			Processor.DetailsView = Context->DetailsView.Pin();
#else
			Processor.DetailsView = Context->DetailsView;
#endif
			void* Container = nullptr;
			if (!Processor.Process(Container))
			{
				return;
			}

			const TSharedRef<FPropertyHistoryHandler> Handler = MakeShared<FPropertyHistoryHandler>(Processor);
			if (!Handler->Initialize(*Processor.Object))
			{
				return;
			}

			FToolMenuSection& Section = ToolMenu->FindOrAddSection("History", INVTEXT("History"));

			Section.AddMenuEntry(
				"SeeHistory",
				INVTEXT("See history"),
				INVTEXT("See this property history"),
				FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Actions.History"),
				FUIAction(
					MakeWeakObjectPtrDelegate(Object, [Handler]
					{
						Handler->ShowHistory();
					})));
		}));
	}
};

IMPLEMENT_MODULE(FPropertyHistoryModule, PropertyHistory);