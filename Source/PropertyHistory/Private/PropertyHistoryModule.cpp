// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "CoreMinimal.h"
#include "ToolMenus.h"
#include "SPropertyHistory.h"
#include "DetailRowMenuContext.h"
#include "PropertyHistoryHandler.h"
#include "PropertyHistoryProcessor.h"
#include "PropertyHistoryUtilities.h"
#include "RevisionControlStyle/RevisionControlStyle.h"
#include "Editor/PropertyEditor/Private/SDetailSingleItemRow.h"
#include "Editor/PropertyEditor/Private/DetailRowMenuContextPrivate.h"

DEFINE_PRIVATE_ACCESS(FPropertyNode, InstanceMetaData)
DEFINE_PRIVATE_ACCESS(SDetailsViewBase, DetailLayouts)
DEFINE_PRIVATE_ACCESS_FUNCTION(SDetailSingleItemRow, GetPropertyNode);

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
			UClass* Class = FindObject<UClass>(nullptr, TEXT("/Script/PropertyEditor.DetailRowMenuContextPrivate"));
			if (!ensure(Class))
			{
				return;
			}

			const UDetailRowMenuContext* DetailsContext = ToolMenu->FindContext<UDetailRowMenuContext>();
			UObject* Context = ToolMenu->Context.FindByClass(Class);
			if (!DetailsContext ||
				!Context ||
				!Context->GetClass()->IsChildOf(Class))
			{
				return;
			}

			const UDetailRowMenuContextPrivate* ContextObject = static_cast<UDetailRowMenuContextPrivate*>(Context);
			const TSharedPtr<SDetailSingleItemRow> Row = StaticCastSharedPtr<SDetailSingleItemRow>(ContextObject->Row.Pin());
			if (!Row)
			{
				return;
			}

			TSharedPtr<FPropertyNode> Node = PrivateAccess::GetPropertyNode(*Row)();
			if (!Node)
			{
				return;
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
					SDetailsViewBase* DetailsViewBase = reinterpret_cast<SDetailsViewBase*>(DetailsContext->DetailsView);
					const TSharedPtr<FPropertyNode> RootNode = INLINE_LAMBDA -> TSharedPtr<FPropertyNode>
					{
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

			UObject* Object = nullptr;
			if (Node->GetSingleObject(Object) != FPropertyAccess::Success)
			{
				return;
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
			Processor.DetailsView = DetailsContext->DetailsView;
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