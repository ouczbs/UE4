// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/RigVMController.h"
#include "RigVMModel/RigVMControllerActions.h"
#include "RigVMModel/Nodes/RigVMFunctionEntryNode.h"
#include "RigVMModel/Nodes/RigVMFunctionReturnNode.h"
#include "RigVMModel/Nodes/RigVMFunctionReferenceNode.h"
#include "RigVMCore/RigVMRegistry.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "RigVMCompiler/RigVMCompiler.h"
#include "RigVMDeveloperModule.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/Package.h"
#include "Misc/CoreMisc.h"

#if WITH_EDITOR
#include "Exporters/Exporter.h"
#include "UnrealExporter.h"
#include "Factories.h"
#include "UObject/CoreRedirects.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "EditorStyleSet.h"
#endif

TMap<URigVMController::FControlRigStructPinRedirectorKey, FString> URigVMController::PinPathCoreRedirectors;

URigVMController::URigVMController()
	: bValidatePinDefaults(true)
	, bSuspendNotifications(false)
	, bReportWarningsAndErrors(true)
	, bIgnoreRerouteCompactnessChanges(false)
{
	SetExecuteContextStruct(FRigVMExecuteContext::StaticStruct());
}

URigVMController::URigVMController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bValidatePinDefaults(true)
	, bReportWarningsAndErrors(true)
{
	ActionStack = CreateDefaultSubobject<URigVMActionStack>(TEXT("ActionStack"));
	SetExecuteContextStruct(FRigVMExecuteContext::StaticStruct());

	ActionStack->OnModified().AddLambda([&](ERigVMGraphNotifType NotifType, URigVMGraph* InGraph, UObject* InSubject) -> void {
		Notify(NotifType, InSubject);
	});
}

URigVMController::~URigVMController()
{
}

URigVMGraph* URigVMController::GetGraph() const
{
	if (Graphs.Num() == 0)
	{
		return nullptr;
	}
	return Graphs.Last();
}

void URigVMController::SetGraph(URigVMGraph* InGraph)
{
	ensure(Graphs.Num() < 2);

	URigVMGraph* PreviousGraph = GetGraph();
	if (PreviousGraph)
	{
		PreviousGraph->OnModified().RemoveAll(this);
	}

	Graphs.Reset();
	if (InGraph != nullptr)
	{
		PushGraph(InGraph, false);
	}

	URigVMGraph* CurrentGraph = GetGraph();
	if (CurrentGraph)
	{
		CurrentGraph->OnModified().AddUObject(this, &URigVMController::HandleModifiedEvent);
	}

	HandleModifiedEvent(ERigVMGraphNotifType::GraphChanged, CurrentGraph, nullptr);
}

void URigVMController::PushGraph(URigVMGraph* InGraph, bool bSetupUndoRedo)
{
	check(InGraph);
	Graphs.Push(InGraph);

	if (bSetupUndoRedo)
	{
		ActionStack->AddAction(FRigVMPushGraphAction(InGraph));
	}
}

URigVMGraph* URigVMController::PopGraph(bool bSetupUndoRedo)
{
	ensure(Graphs.Num() > 1);
	URigVMGraph* LastGraph = GetGraph();
	Graphs.Pop();

	if (bSetupUndoRedo)
	{
		ActionStack->AddAction(FRigVMPopGraphAction(LastGraph));
	}

	return LastGraph;
}

URigVMGraph* URigVMController::GetTopLevelGraph() const
{
	URigVMGraph* Graph = GetGraph();
	UObject* Outer = Graph->GetOuter();
	while (Outer)
	{
		if (URigVMGraph* OuterGraph = Cast<URigVMGraph>(Outer))
		{
			Graph = OuterGraph;
			Outer = Outer->GetOuter();
		}
		else if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Outer))
		{
			Outer = Outer->GetOuter();
		}
		else
		{
			break;
		}
	}

	return Graph;
}

FRigVMGraphModifiedEvent& URigVMController::OnModified()
{
	return ModifiedEventStatic;
}

void URigVMController::Notify(ERigVMGraphNotifType InNotifType, UObject* InSubject)
{
	if (bSuspendNotifications)
	{
		return;
	}
	if (URigVMGraph* Graph = GetGraph())
	{
		Graph->Notify(InNotifType, InSubject);
	}
}

void URigVMController::ResendAllNotifications()
{
	if (URigVMGraph* Graph = GetGraph())
	{
		for (URigVMLink* Link : Graph->Links)
		{
			Notify(ERigVMGraphNotifType::LinkRemoved, Link);
		}

		for (URigVMNode* Node : Graph->Nodes)
		{
			Notify(ERigVMGraphNotifType::NodeRemoved, Node);
		}

		for (URigVMNode* Node : Graph->Nodes)
		{
			Notify(ERigVMGraphNotifType::NodeAdded, Node);
		}

		for (URigVMLink* Link : Graph->Links)
		{
			Notify(ERigVMGraphNotifType::LinkAdded, Link);
		}
	}
}

void URigVMController::HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
{
	switch (InNotifType)
	{
		case ERigVMGraphNotifType::GraphChanged:
		case ERigVMGraphNotifType::NodeAdded:
		case ERigVMGraphNotifType::NodeRemoved:
		case ERigVMGraphNotifType::LinkAdded:
		case ERigVMGraphNotifType::LinkRemoved:
		case ERigVMGraphNotifType::PinArraySizeChanged:
		case ERigVMGraphNotifType::VariableAdded:
		case ERigVMGraphNotifType::VariableRemoved:
		case ERigVMGraphNotifType::ParameterAdded:
		case ERigVMGraphNotifType::ParameterRemoved:
		{
			if (InGraph)
			{
				InGraph->ClearAST();
			}
			break;
		}
		case ERigVMGraphNotifType::PinDefaultValueChanged:
		{
			if (InGraph->RuntimeAST.IsValid())
			{
				URigVMPin* RootPin = CastChecked<URigVMPin>(InSubject)->GetRootPin();
				FRigVMASTProxy RootPinProxy = FRigVMASTProxy::MakeFromUObject(RootPin);
				const FRigVMExprAST* Expression = InGraph->GetRuntimeAST()->GetExprForSubject(RootPinProxy);
				if (Expression == nullptr)
				{
					InGraph->ClearAST();
					break;
				}
				else if(Expression->NumParents() > 1)
				{
					InGraph->ClearAST();
					break;
				}
			}
			break;
		}
	}

	ModifiedEventStatic.Broadcast(InNotifType, InGraph, InSubject);
	if (ModifiedEventDynamic.IsBound())
	{
		ModifiedEventDynamic.Broadcast(InNotifType, InGraph, InSubject);
	}
}

#if WITH_EDITOR

URigVMUnitNode* URigVMController::AddUnitNode(UScriptStruct* InScriptStruct, const FName& InMethodName, const FVector2D& InPosition, const FString& InNodeName, bool bSetupUndoRedo)
{
	if(!IsValidGraph())
	{
		return nullptr;
	}

	if (GetGraph()->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Cannot add unit nodes to function library graphs."));
		return nullptr;
	}

	if (InScriptStruct == nullptr)
	{
		ReportError(TEXT("InScriptStruct is null."));
		return nullptr;
	}
	if (InMethodName == NAME_None)
	{
		ReportError(TEXT("InMethodName is None."));
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	FString FunctionName = FString::Printf(TEXT("F%s::%s"), *InScriptStruct->GetName(), *InMethodName.ToString());
	FRigVMFunctionPtr Function = FRigVMRegistry::Get().FindFunction(*FunctionName);
	if (Function == nullptr)
	{
		ReportErrorf(TEXT("RIGVM_METHOD '%s' cannot be found."), *FunctionName);
		return nullptr;
	}

	FString StructureError;
	if (!FRigVMStruct::ValidateStruct(InScriptStruct, &StructureError))
	{
		ReportErrorf(TEXT("Failed to validate struct '%s': %s"), *InScriptStruct->GetName(), *StructureError);
		return nullptr;
	}

	// don't allow event nodes in anything but top level graphs
	if (bSetupUndoRedo)
	{
		if (!Graph->IsTopLevelGraph())
		{
			FStructOnScope StructOnScope(InScriptStruct);
			FRigVMStruct* StructMemory = (FRigVMStruct*)StructOnScope.GetStructMemory();
			InScriptStruct->InitializeDefaultValue((uint8*)StructMemory);

			if (!StructMemory->GetEventName().IsNone())
			{
				ReportAndNotifyError(TEXT("Event nodes can only be added to top level graphs."));
				return nullptr;
			}
		}
	}

	FString Name = GetValidNodeName(InNodeName.IsEmpty() ? InScriptStruct->GetName() : InNodeName);
	URigVMUnitNode* Node = NewObject<URigVMUnitNode>(Graph, *Name);
	Node->ScriptStruct = InScriptStruct;
	Node->MethodName = InMethodName;
	Node->Position = InPosition;
	Node->NodeTitle = InScriptStruct->GetMetaData(TEXT("DisplayName"));
	
	FString NodeColorMetadata;
	InScriptStruct->GetStringMetaDataHierarchical(*URigVMNode::NodeColorName, &NodeColorMetadata);
	if (!NodeColorMetadata.IsEmpty())
	{
		Node->NodeColor = GetColorFromMetadata(NodeColorMetadata);
	}

	FString ExportedDefaultValue;
	CreateDefaultValueForStructIfRequired(InScriptStruct, ExportedDefaultValue);
	AddPinsForStruct(InScriptStruct, Node, nullptr, ERigVMPinDirection::Invalid, ExportedDefaultValue, true);

	Graph->Nodes.Add(Node);
	if (!bSuspendNotifications)
	{
		Graph->MarkPackageDirty();
	}

	FRigVMAddUnitNodeAction Action;
	if (bSetupUndoRedo)
	{
		Action = FRigVMAddUnitNodeAction(Node);
		Action.Title = FString::Printf(TEXT("Add %s Node"), *Node->GetNodeTitle());
		ActionStack->BeginAction(Action);
	}

	Notify(ERigVMGraphNotifType::NodeAdded, Node);

	if (UnitNodeCreatedContext.IsValid())
	{
		if (TSharedPtr<FStructOnScope> StructScope = Node->ConstructStructInstance())
		{
			TGuardValue<FName> NodeNameScope(UnitNodeCreatedContext.NodeName, Node->GetFName());
			FRigVMStruct* StructInstance = (FRigVMStruct*)StructScope->GetStructMemory();
			StructInstance->OnUnitNodeCreated(UnitNodeCreatedContext);
		}
	}

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action);
	}

	return Node;
}

URigVMUnitNode* URigVMController::AddUnitNodeFromStructPath(const FString& InScriptStructPath, const FName& InMethodName, const FVector2D& InPosition, const FString& InNodeName, bool bSetupUndoRedo)
{
	if(!IsValidGraph())
	{
		return nullptr;
	}

	UScriptStruct* ScriptStruct = URigVMPin::FindObjectFromCPPTypeObjectPath<UScriptStruct>(InScriptStructPath);
	if (ScriptStruct == nullptr)
	{
		ReportErrorf(TEXT("Cannot find struct for path '%s'."), *InScriptStructPath);
		return nullptr;
	}

	return AddUnitNode(ScriptStruct, InMethodName, InPosition, InNodeName, bSetupUndoRedo);
}

URigVMVariableNode* URigVMController::AddVariableNode(const FName& InVariableName, const FString& InCPPType, UObject* InCPPTypeObject, bool bIsGetter, const FString& InDefaultValue, const FVector2D& InPosition, const FString& InNodeName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (Graph->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Cannot add variables nodes to function library graphs."));
		return nullptr;
	}

	if (InCPPTypeObject == nullptr)
	{
		InCPPTypeObject = URigVMCompiler::GetScriptStructForCPPType(InCPPType);
	}
	if (InCPPTypeObject == nullptr)
	{
		InCPPTypeObject = URigVMPin::FindObjectFromCPPTypeObjectPath<UObject>(InCPPType);
	}

	FString CPPType = InCPPType;
	if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(InCPPTypeObject))
	{
		CPPType = ScriptStruct->GetStructCPPName();
	}

	FString Name = GetValidNodeName(InNodeName.IsEmpty() ? FString(TEXT("VariableNode")) : InNodeName);
	URigVMVariableNode* Node = NewObject<URigVMVariableNode>(Graph, *Name);
	Node->Position = InPosition;

	if (!bIsGetter)
	{
		URigVMPin* ExecutePin = NewObject<URigVMPin>(Node, FRigVMStruct::ExecuteContextName);
		ExecutePin->CPPType = FString::Printf(TEXT("F%s"), *ExecuteContextStruct->GetName());
		ExecutePin->CPPTypeObject = ExecuteContextStruct;
		ExecutePin->CPPTypeObjectPath = *ExecutePin->CPPTypeObject->GetPathName();
		ExecutePin->Direction = ERigVMPinDirection::IO;
		Node->Pins.Add(ExecutePin);
	}

	URigVMPin* VariablePin = NewObject<URigVMPin>(Node, *URigVMVariableNode::VariableName);
	VariablePin->CPPType = TEXT("FName");
	VariablePin->Direction = ERigVMPinDirection::Hidden;
	VariablePin->DefaultValue = InVariableName.ToString();
	VariablePin->CustomWidgetName = TEXT("VariableName");
	Node->Pins.Add(VariablePin);

	URigVMPin* ValuePin = NewObject<URigVMPin>(Node, *URigVMVariableNode::ValueName);
	ValuePin->CPPType = CPPType;

	if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(InCPPTypeObject))
	{
		ValuePin->CPPTypeObject = ScriptStruct;
		ValuePin->CPPTypeObjectPath = *ValuePin->CPPTypeObject->GetPathName();
	}
	else if (UEnum* Enum = Cast<UEnum>(InCPPTypeObject))
	{
		ValuePin->CPPTypeObject = Enum;
		ValuePin->CPPTypeObjectPath = *ValuePin->CPPTypeObject->GetPathName();
	}

	ValuePin->Direction = bIsGetter ? ERigVMPinDirection::Output : ERigVMPinDirection::Input;
	Node->Pins.Add(ValuePin);

	Graph->Nodes.Add(Node);

	if (ValuePin->IsStruct())
	{
		FString DefaultValue = InDefaultValue;
		CreateDefaultValueForStructIfRequired(ValuePin->GetScriptStruct(), DefaultValue);
		AddPinsForStruct(ValuePin->GetScriptStruct(), Node, ValuePin, ValuePin->Direction, DefaultValue, false);
	}
	else if (!InDefaultValue.IsEmpty() && InDefaultValue != TEXT("()"))
	{
		SetPinDefaultValue(ValuePin, InDefaultValue, true, false, false);
	}

	ForEveryPinRecursively(Node, [](URigVMPin* Pin) {
		Pin->bIsExpanded = false;
	});

	if (!bSuspendNotifications)
	{
		Graph->MarkPackageDirty();
	}

	FRigVMAddVariableNodeAction Action;
	if (bSetupUndoRedo)
	{
		Action = FRigVMAddVariableNodeAction(Node);
		Action.Title = FString::Printf(TEXT("Add %s Variable"), *InVariableName.ToString());
		ActionStack->BeginAction(Action);
	}

	Notify(ERigVMGraphNotifType::NodeAdded, Node);
	Notify(ERigVMGraphNotifType::VariableAdded, Node);

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action);
	}

	return Node;
}

URigVMVariableNode* URigVMController::AddVariableNodeFromObjectPath(const FName& InVariableName, const FString& InCPPType, const FString& InCPPTypeObjectPath, bool bIsGetter, const FString& InDefaultValue, const FVector2D& InPosition, const FString& InNodeName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return nullptr;
	}

	UObject* CPPTypeObject = nullptr;
	if (!InCPPTypeObjectPath.IsEmpty())
	{
		CPPTypeObject = URigVMPin::FindObjectFromCPPTypeObjectPath<UObject>(InCPPTypeObjectPath);
		if (CPPTypeObject == nullptr)
		{
			ReportErrorf(TEXT("Cannot find cpp type object for path '%s'."), *InCPPTypeObjectPath);
			return nullptr;
		}
	}

	return AddVariableNode(InVariableName, InCPPType, CPPTypeObject, bIsGetter, InDefaultValue, InPosition, InNodeName, bSetupUndoRedo);
}

void URigVMController::RefreshVariableNode(const FName& InNodeName, const FName& InVariableName, const FString& InCPPType, UObject* InCPPTypeObject, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(Graph->FindNodeByName(InNodeName)))
	{
		if (URigVMPin* VariablePin = VariableNode->FindPin(URigVMVariableNode::VariableName))
		{
			if (VariablePin->Direction == ERigVMPinDirection::Visible)
			{
				if (bSetupUndoRedo)
				{
					VariablePin->Modify();
				}
				VariablePin->Direction = ERigVMPinDirection::Hidden;
				Notify(ERigVMGraphNotifType::PinDirectionChanged, VariablePin);
			}

			if (InVariableName.IsValid() && VariablePin->DefaultValue != InVariableName.ToString())
			{
				if (bSetupUndoRedo)
				{
					VariablePin->Modify();
				}
				VariablePin->DefaultValue = InVariableName.ToString();
				Notify(ERigVMGraphNotifType::PinDefaultValueChanged, VariablePin);
				Notify(ERigVMGraphNotifType::VariableRenamed, VariableNode);
			}

			if (!InCPPType.IsEmpty())
			{
				if (URigVMPin* ValuePin = VariableNode->FindPin(URigVMVariableNode::ValueName))
				{
					if (ValuePin->CPPType != InCPPType)
					{
						if (bSetupUndoRedo)
						{
							ValuePin->Modify();
						}

						BreakAllLinks(ValuePin, ValuePin->GetDirection() == ERigVMPinDirection::Input, bSetupUndoRedo);
						BreakAllLinksRecursive(ValuePin, ValuePin->GetDirection() == ERigVMPinDirection::Input, false, bSetupUndoRedo);

						// if this is an unsupported datatype...
						if (InCPPType == FName(NAME_None).ToString())
						{
							RemoveNode(VariableNode, bSetupUndoRedo);
							return;
						}

						ValuePin->CPPType = InCPPType;
						ValuePin->CPPTypeObject = InCPPTypeObject;
						ValuePin->CPPTypeObjectPath = *InCPPTypeObject->GetPathName();

						TArray<URigVMPin*> SubPins = ValuePin->GetSubPins();
						for(URigVMPin * SubPin : SubPins)
						{
							ValuePin->SubPins.Remove(SubPin);
						}

						if (ValuePin->IsStruct())
						{
							FString DefaultValue = ValuePin->DefaultValue;
							CreateDefaultValueForStructIfRequired(ValuePin->GetScriptStruct(), DefaultValue);
							AddPinsForStruct(ValuePin->GetScriptStruct(), ValuePin->GetNode(), ValuePin, ValuePin->Direction, DefaultValue, false);
						}

						Notify(ERigVMGraphNotifType::PinTypeChanged, ValuePin);
					}
				}
			}
		}
	}
}

void URigVMController::OnExternalVariableRemoved(const FName& InVarName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return;
	}

	if (!InVarName.IsValid())
	{
		return;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (Graph->IsA<URigVMFunctionLibrary>())
	{
		return;
	}

	FString VarNameStr = InVarName.ToString();

	if (bSetupUndoRedo)
	{
		OpenUndoBracket(TEXT("Remove Variable Nodes"));
	}

	TArray<URigVMNode*> Nodes = Graph->GetNodes();
	for (URigVMNode* Node : Nodes)
	{
		if (URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(Node))
		{
			if (URigVMPin* VariablePin = VariableNode->FindPin(URigVMVariableNode::VariableName))
			{
				if (VariablePin->GetDefaultValue() == VarNameStr)
				{
					RemoveNode(Node, bSetupUndoRedo, true);
					continue;
				}
			}
		}

		TArray<URigVMPin*> AllPins = Node->GetAllPinsRecursively();
		for (URigVMPin* Pin : AllPins)
		{
			if (Pin->GetBoundVariableName() == InVarName.ToString())
			{
				BindPinToVariable(Pin, FString(), true);
			}
		}
	}

	if (bSetupUndoRedo)
	{
		CloseUndoBracket();
	}
}

void URigVMController::OnExternalVariableRenamed(const FName& InOldVarName, const FName& InNewVarName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return;
	}

	if (!InOldVarName.IsValid() || !InNewVarName.IsValid())
	{
		return;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (Graph->IsA<URigVMFunctionLibrary>())
	{
		return;
	}

	FString VarNameStr = InOldVarName.ToString();

	if (bSetupUndoRedo)
	{
		OpenUndoBracket(TEXT("Rename Variable Nodes"));
	}

	TArray<URigVMNode*> Nodes = Graph->GetNodes();
	for (URigVMNode* Node : Nodes)
	{
		if (URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(Node))
		{
			if (URigVMPin* VariablePin = VariableNode->FindPin(URigVMVariableNode::VariableName))
			{
				if (VariablePin->GetDefaultValue() == VarNameStr)
				{
					RefreshVariableNode(Node->GetFName(), InNewVarName, FString(), nullptr, bSetupUndoRedo);
					continue;
				}
			}
		}

		TArray<URigVMPin*> AllPins = Node->GetAllPinsRecursively();
		for (URigVMPin* Pin : AllPins)
		{
			if (Pin->GetBoundVariableName() == InOldVarName.ToString())
			{
				FString OldVariablePath = Pin->GetBoundVariablePath();
				FString NewVariablePath = OldVariablePath.Replace(*InOldVarName.ToString(), *InNewVarName.ToString());
				BindPinToVariable(Pin, NewVariablePath, true);
			}
		}
	}

	if (bSetupUndoRedo)
	{
		CloseUndoBracket();
	}
}

void URigVMController::OnExternalVariableTypeChanged(const FName& InVarName, const FString& InCPPType, UObject* InCPPTypeObject, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return;
	}

	if (!InVarName.IsValid())
	{
		return;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (Graph->IsA<URigVMFunctionLibrary>())
	{
		return;
	}

	FString VarNameStr = InVarName.ToString();

	if (bSetupUndoRedo)
	{
		OpenUndoBracket(TEXT("Change Variable Nodes Type"));
	}

	TArray<URigVMNode*> Nodes = Graph->GetNodes();
	for (URigVMNode* Node : Nodes)
	{
		if (URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(Node))
		{
			if (URigVMPin* VariablePin = VariableNode->FindPin(URigVMVariableNode::VariableName))
			{
				if (VariablePin->GetDefaultValue() == VarNameStr)
				{
					RefreshVariableNode(Node->GetFName(), InVarName, InCPPType, InCPPTypeObject, bSetupUndoRedo);
					continue;
				}
			}
		}

		TArray<URigVMPin*> AllPins = Node->GetAllPinsRecursively();
		for (URigVMPin* Pin : AllPins)
		{
			if (Pin->GetBoundVariableName() == InVarName.ToString())
			{
				FString BoundVariablePath = Pin->GetBoundVariablePath();
				BindPinToVariable(Pin, FString(), true);
				// try to bind it again - maybe it can be bound (due to cast rules etc)
				BindPinToVariable(Pin, BoundVariablePath, true);
			}
		}
	}

	if (bSetupUndoRedo)
	{
		CloseUndoBracket();
	}
}

URigVMVariableNode* URigVMController::ReplaceParameterNodeWithVariable(const FName& InNodeName, const FName& InVariableName, const FString& InCPPType, UObject* InCPPTypeObject, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (URigVMParameterNode* ParameterNode = Cast<URigVMParameterNode>(Graph->FindNodeByName(InNodeName)))
	{
		URigVMPin* ParameterValuePin = ParameterNode->FindPin(URigVMParameterNode::ValueName);
		check(ParameterValuePin);

		FRigVMGraphParameterDescription Description = ParameterNode->GetParameterDescription();
		
		URigVMVariableNode* VariableNode = AddVariableNode(
			InVariableName,
			InCPPType,
			InCPPTypeObject,
			ParameterValuePin->GetDirection() == ERigVMPinDirection::Output,
			ParameterValuePin->GetDefaultValue(),
			ParameterNode->GetPosition(),
			FString(),
			bSetupUndoRedo);

		if (VariableNode)
		{
			URigVMPin* VariableValuePin = VariableNode->FindPin(URigVMVariableNode::ValueName);

			RewireLinks(
				ParameterValuePin,
				VariableValuePin,
				ParameterValuePin->GetDirection() == ERigVMPinDirection::Input,
				bSetupUndoRedo
			);

			RemoveNode(ParameterNode, bSetupUndoRedo, true);

			return VariableNode;
		}
	}

	return nullptr;
}

URigVMParameterNode* URigVMController::AddParameterNode(const FName& InParameterName, const FString& InCPPType, UObject* InCPPTypeObject, bool bIsInput, const FString& InDefaultValue, const FVector2D& InPosition, const FString& InNodeName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (Graph->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Cannot add parameter nodes to function library graphs."));
		return nullptr;
	}

	if (InCPPTypeObject == nullptr)
	{
		InCPPTypeObject = URigVMCompiler::GetScriptStructForCPPType(InCPPType);
	}
	if (InCPPTypeObject == nullptr)
	{
		InCPPTypeObject = URigVMPin::FindObjectFromCPPTypeObjectPath<UObject>(InCPPType);
	}

	TArray<FRigVMGraphParameterDescription> ExistingParameters = Graph->GetParameterDescriptions();
	for (const FRigVMGraphParameterDescription& ExistingParameter : ExistingParameters)
	{
		if (ExistingParameter.Name == InParameterName)
		{
			if (ExistingParameter.CPPType != InCPPType ||
				ExistingParameter.CPPTypeObject != InCPPTypeObject ||
				ExistingParameter.bIsInput != bIsInput)
			{
				ReportErrorf(TEXT("Cannot add parameter '%s' - parameter already exists."), *InParameterName.ToString());
				return nullptr;
			}
		}
	}

	FString Name = GetValidNodeName(InNodeName.IsEmpty() ? FString(TEXT("ParameterNode")) : InNodeName);
	URigVMParameterNode* Node = NewObject<URigVMParameterNode>(Graph, *Name);
	Node->Position = InPosition;

	if (!bIsInput)
	{
		URigVMPin* ExecutePin = NewObject<URigVMPin>(Node, FRigVMStruct::ExecuteContextName);
		ExecutePin->CPPType = FString::Printf(TEXT("F%s"), *ExecuteContextStruct->GetName());
		ExecutePin->CPPTypeObject = ExecuteContextStruct;
		ExecutePin->CPPTypeObjectPath = *ExecutePin->CPPTypeObject->GetPathName();
		ExecutePin->Direction = ERigVMPinDirection::IO;
		Node->Pins.Add(ExecutePin);
	}

	URigVMPin* ParameterPin = NewObject<URigVMPin>(Node, *URigVMParameterNode::ParameterName);
	ParameterPin->CPPType = TEXT("FName");
	ParameterPin->Direction = ERigVMPinDirection::Visible;
	ParameterPin->DefaultValue = InParameterName.ToString();
	ParameterPin->CustomWidgetName = TEXT("ParameterName");

	Node->Pins.Add(ParameterPin);

	URigVMPin* DefaultValuePin = nullptr;
	if (bIsInput)
	{
		DefaultValuePin = NewObject<URigVMPin>(Node, *URigVMParameterNode::DefaultName);
	}
	URigVMPin* ValuePin = NewObject<URigVMPin>(Node, *URigVMParameterNode::ValueName);

	if (DefaultValuePin)
	{
		DefaultValuePin->CPPType = InCPPType;
	}
	ValuePin->CPPType = InCPPType;

	if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(InCPPTypeObject))
	{
		if (DefaultValuePin)
		{
			DefaultValuePin->CPPTypeObject = ScriptStruct;
			DefaultValuePin->CPPTypeObjectPath = *DefaultValuePin->CPPTypeObject->GetPathName();
		}
		ValuePin->CPPTypeObject = ScriptStruct;
		ValuePin->CPPTypeObjectPath = *ValuePin->CPPTypeObject->GetPathName();
	}
	else if (UEnum* Enum = Cast<UEnum>(InCPPTypeObject))
	{
		if (DefaultValuePin)
		{
			DefaultValuePin->CPPTypeObject = Enum;
			DefaultValuePin->CPPTypeObjectPath = *DefaultValuePin->CPPTypeObject->GetPathName();
		}
		ValuePin->CPPTypeObject = Enum;
		ValuePin->CPPTypeObjectPath = *ValuePin->CPPTypeObject->GetPathName();
	}

	if (DefaultValuePin)
	{
		DefaultValuePin->Direction = ERigVMPinDirection::Visible;
	}
	ValuePin->Direction = bIsInput ? ERigVMPinDirection::Output : ERigVMPinDirection::Input;

	if (bIsInput)
	{
		if (ValuePin->CPPType == TEXT("FName"))
		{
			ValuePin->bIsConstant = true;
		}
	}

	if (DefaultValuePin)
	{
		Node->Pins.Add(DefaultValuePin);
	}
	Node->Pins.Add(ValuePin);

	Graph->Nodes.Add(Node);

	if (ValuePin->IsStruct())
	{
		FString DefaultValue = InDefaultValue;
		CreateDefaultValueForStructIfRequired(ValuePin->GetScriptStruct(), DefaultValue);
		if (DefaultValuePin)
		{
			AddPinsForStruct(DefaultValuePin->GetScriptStruct(), Node, DefaultValuePin, DefaultValuePin->Direction, DefaultValue, false);
		}
		AddPinsForStruct(ValuePin->GetScriptStruct(), Node, ValuePin, ValuePin->Direction, DefaultValue, false);
	}
	else if (!InDefaultValue.IsEmpty() && InDefaultValue != TEXT("()"))
	{
		if (DefaultValuePin)
		{
			SetPinDefaultValue(DefaultValuePin, InDefaultValue, true, false, false);
		}
		SetPinDefaultValue(ValuePin, InDefaultValue, true, false, false);
	}

	ForEveryPinRecursively(Node, [](URigVMPin* Pin) {
		Pin->bIsExpanded = false;
	});

	if (!bSuspendNotifications)
	{
		Graph->MarkPackageDirty();
	}

	FRigVMAddParameterNodeAction Action;
	if (bSetupUndoRedo)
	{
		Action = FRigVMAddParameterNodeAction(Node);
		Action.Title = FString::Printf(TEXT("Add %s Parameter"), *InParameterName.ToString());
		ActionStack->BeginAction(Action);
	}

	Notify(ERigVMGraphNotifType::NodeAdded, Node);
	Notify(ERigVMGraphNotifType::ParameterAdded, Node);

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action);
	}

	return Node;
}

URigVMParameterNode* URigVMController::AddParameterNodeFromObjectPath(const FName& InParameterName, const FString& InCPPType, const FString& InCPPTypeObjectPath, bool bIsInput, const FString& InDefaultValue, const FVector2D& InPosition, const FString& InNodeName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return nullptr;
	}

	UObject* CPPTypeObject = nullptr;
	if (!InCPPTypeObjectPath.IsEmpty())
	{
		CPPTypeObject = URigVMPin::FindObjectFromCPPTypeObjectPath<UObject>(InCPPTypeObjectPath);
		if (CPPTypeObject == nullptr)
		{
			ReportErrorf(TEXT("Cannot find cpp type object for path '%s'."), *InCPPTypeObjectPath);
			return nullptr;
		}
	}

	return AddParameterNode(InParameterName, InCPPType, CPPTypeObject, bIsInput, InDefaultValue, InPosition, InNodeName, bSetupUndoRedo);
}

URigVMCommentNode* URigVMController::AddCommentNode(const FString& InCommentText, const FVector2D& InPosition, const FVector2D& InSize, const FLinearColor& InColor, const FString& InNodeName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (Graph->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Cannot add comment nodes to function library graphs."));
		return nullptr;
	}

	FString Name = GetValidNodeName(InNodeName.IsEmpty() ? FString(TEXT("CommentNode")) : InNodeName);
	URigVMCommentNode* Node = NewObject<URigVMCommentNode>(Graph, *Name);
	Node->Position = InPosition;
	Node->Size = InSize;
	Node->NodeColor = InColor;
	Node->CommentText = InCommentText;

	Graph->Nodes.Add(Node);
	if (!bSuspendNotifications)
	{
		Graph->MarkPackageDirty();
	}

	FRigVMAddCommentNodeAction Action;
	if (bSetupUndoRedo)
	{
		Action = FRigVMAddCommentNodeAction(Node);
		Action.Title = FString::Printf(TEXT("Add Comment"));
		ActionStack->BeginAction(Action);
	}

	Notify(ERigVMGraphNotifType::NodeAdded, Node);

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action);
	}

	return Node;
}

URigVMRerouteNode* URigVMController::AddRerouteNodeOnLink(URigVMLink* InLink, bool bShowAsFullNode, const FVector2D& InPosition, const FString& InNodeName, bool bSetupUndoRedo)
{
	if(!IsValidLinkForGraph(InLink))
	{
		return nullptr;
	}

	if (GetGraph()->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Cannot add reroutes to function library graphs."));
		return nullptr;
	}

	URigVMPin* SourcePin = InLink->GetSourcePin();
	URigVMPin* TargetPin = InLink->GetTargetPin();

	TGuardValue<bool> GuardCompactness(bIgnoreRerouteCompactnessChanges, true);

	FRigVMBaseAction Action;
	if (bSetupUndoRedo)
	{
		Action.Title = FString::Printf(TEXT("Add Reroute"));
		ActionStack->BeginAction(Action);
	}

	URigVMRerouteNode* Node = AddRerouteNodeOnPin(TargetPin->GetPinPath(), true, bShowAsFullNode, InPosition, InNodeName, bSetupUndoRedo);
	if (Node == nullptr)
	{
		if (bSetupUndoRedo)
		{
			ActionStack->CancelAction(Action);
		}
		return nullptr;
	}

	URigVMPin* ValuePin = Node->Pins[0];
	AddLink(SourcePin, ValuePin, bSetupUndoRedo);

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action);
	}

	return Node;
}

URigVMRerouteNode* URigVMController::AddRerouteNodeOnLinkPath(const FString& InLinkPinPathRepresentation, bool bShowAsFullNode, const FVector2D& InPosition, const FString& InNodeName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMLink* Link = Graph->FindLink(InLinkPinPathRepresentation);
	return AddRerouteNodeOnLink(Link, bShowAsFullNode, InPosition, InNodeName, bSetupUndoRedo);
}

URigVMRerouteNode* URigVMController::AddRerouteNodeOnPin(const FString& InPinPath, bool bAsInput, bool bShowAsFullNode, const FVector2D& InPosition, const FString& InNodeName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (Graph->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Cannot add reroutes to function library graphs."));
		return nullptr;
	}

	URigVMPin* Pin = Graph->FindPin(InPinPath);
	if(Pin == nullptr)
	{
		return nullptr;
	}

	TGuardValue<bool> GuardCompactness(bIgnoreRerouteCompactnessChanges, true);

	FRigVMBaseAction Action;
	if (bSetupUndoRedo)
	{
		Action.Title = FString::Printf(TEXT("Add Reroute"));
		ActionStack->BeginAction(Action);
	}

	//in case an injected node is present, use its pins for any new links
	URigVMPin *PinForLink = Pin->GetPinForLink(); 
	BreakAllLinks(PinForLink, bAsInput, bSetupUndoRedo);

	FString Name = GetValidNodeName(InNodeName.IsEmpty() ? FString(TEXT("RerouteNode")) : InNodeName);
	URigVMRerouteNode* Node = NewObject<URigVMRerouteNode>(Graph, *Name);
	Node->Position = InPosition;
	Node->bShowAsFullNode = bShowAsFullNode;

	URigVMPin* ValuePin = NewObject<URigVMPin>(Node, *URigVMRerouteNode::ValueName);
	ConfigurePinFromPin(ValuePin, Pin);
	ValuePin->Direction = ERigVMPinDirection::IO;
	Node->Pins.Add(ValuePin);

	if (ValuePin->IsStruct())
	{
		AddPinsForStruct(ValuePin->GetScriptStruct(), Node, ValuePin, ValuePin->Direction, FString(), false);
	}

	FString DefaultValue = Pin->GetDefaultValue();
	if (!DefaultValue.IsEmpty())
	{
		SetPinDefaultValue(ValuePin, Pin->GetDefaultValue(), true, false, false);
	}

	ForEveryPinRecursively(ValuePin, [](URigVMPin* Pin) {
		Pin->bIsExpanded = true;
	});

	Graph->Nodes.Add(Node);
	if (!bSuspendNotifications)
	{
		Graph->MarkPackageDirty();
	}

	if (bSetupUndoRedo)
	{
		ActionStack->AddAction(FRigVMAddRerouteNodeAction(Node));
	}

	Notify(ERigVMGraphNotifType::NodeAdded, Node);

	if (bAsInput)
	{
		AddLink(ValuePin, PinForLink, bSetupUndoRedo);
	}
	else
	{
		AddLink(PinForLink, ValuePin, bSetupUndoRedo);
	}

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action);
	}

	return Node;
}

URigVMInjectionInfo* URigVMController::AddInjectedNode(const FString& InPinPath, bool bAsInput, UScriptStruct* InScriptStruct, const FName& InMethodName, const FName& InInputPinName, const FName& InOutputPinName, const FString& InNodeName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (Graph->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Cannot add injected nodes to function library graphs."));
		return nullptr;
	}

	URigVMPin* Pin = Graph->FindPin(InPinPath);
	if (Pin == nullptr)
	{
		return nullptr;
	}

	if (Pin->IsArray())
	{
		return nullptr;
	}

	if (bAsInput && !(Pin->GetDirection() == ERigVMPinDirection::Input || Pin->GetDirection() == ERigVMPinDirection::IO))
	{
		ReportError(TEXT("Pin is not an input / cannot add injected input node."));
		return nullptr;
	}
	if (!bAsInput && !(Pin->GetDirection() == ERigVMPinDirection::Output))
	{
		ReportError(TEXT("Pin is not an output / cannot add injected output node."));
		return nullptr;
	}

	if (InScriptStruct == nullptr)
	{
		ReportError(TEXT("InScriptStruct is null."));
		return nullptr;
	}

	if (InMethodName == NAME_None)
	{
		ReportError(TEXT("InMethodName is None."));
		return nullptr;
	}

	// find the input and output pins to use
	FProperty* InputProperty = InScriptStruct->FindPropertyByName(InInputPinName);
	if (InputProperty == nullptr)
	{
		ReportErrorf(TEXT("Cannot find property '%s' on struct type '%s'."), *InInputPinName.ToString(), *InScriptStruct->GetName());
		return nullptr;
	}
	if (!InputProperty->HasMetaData(FRigVMStruct::InputMetaName))
	{
		ReportErrorf(TEXT("Property '%s' on struct type '%s' is not marked as an input."), *InInputPinName.ToString(), *InScriptStruct->GetName());
		return nullptr;
	}
	FProperty* OutputProperty = InScriptStruct->FindPropertyByName(InOutputPinName);
	if (OutputProperty == nullptr)
	{
		ReportErrorf(TEXT("Cannot find property '%s' on struct type '%s'."), *InOutputPinName.ToString(), *InScriptStruct->GetName());
		return nullptr;
	}
	if (!OutputProperty->HasMetaData(FRigVMStruct::OutputMetaName))
	{
		ReportErrorf(TEXT("Property '%s' on struct type '%s' is not marked as an output."), *InOutputPinName.ToString(), *InScriptStruct->GetName());
		return nullptr;
	}

	FRigVMBaseAction Action;
	if (bSetupUndoRedo)
	{
		Action.Title = FString::Printf(TEXT("Add Injected Node"));
		ActionStack->BeginAction(Action);
	}

	URigVMUnitNode* UnitNode = nullptr;
	{
		TGuardValue<bool> GuardNotifications(bSuspendNotifications, true);
		UnitNode = AddUnitNode(InScriptStruct, InMethodName, FVector2D::ZeroVector, InNodeName, false);
	}
	if (UnitNode == nullptr)
	{
		if (bSetupUndoRedo)
		{
			ActionStack->CancelAction(Action);
		}
		return nullptr;
	}
	else if (UnitNode->IsMutable())
	{
		ReportErrorf(TEXT("Injected node %s is mutable."), *InScriptStruct->GetName());
		RemoveNode(UnitNode, false);
		if (bSetupUndoRedo)
		{
			ActionStack->CancelAction(Action);
		}
		return nullptr;
	}

	URigVMPin* InputPin = UnitNode->FindPin(InInputPinName.ToString());
	check(InputPin);
	URigVMPin* OutputPin = UnitNode->FindPin(InOutputPinName.ToString());
	check(OutputPin);

	if (InputPin->GetCPPType() != OutputPin->GetCPPType() ||
		InputPin->IsArray() != OutputPin->IsArray())
	{
		ReportErrorf(TEXT("Injected node %s is using incompatible input and output pins."), *InScriptStruct->GetName());
		RemoveNode(UnitNode, false);
		if (bSetupUndoRedo)
		{
			ActionStack->CancelAction(Action);
		}
		return nullptr;
	}

	if (InputPin->GetCPPType() != Pin->GetCPPType() ||
		InputPin->IsArray() != Pin->IsArray())
	{
		ReportErrorf(TEXT("Injected node %s is using incompatible pin."), *InScriptStruct->GetName());
		RemoveNode(UnitNode, false);
		if (bSetupUndoRedo)
		{
			ActionStack->CancelAction(Action);
		}
		return nullptr;
	}

	URigVMInjectionInfo* InjectionInfo = NewObject<URigVMInjectionInfo>(Pin);

	// re-parent the unit node to be under the injection info
	UnitNode->Rename(nullptr, InjectionInfo);

	InjectionInfo->UnitNode = UnitNode;
	InjectionInfo->bInjectedAsInput = bAsInput;
	InjectionInfo->InputPin = InputPin;
	InjectionInfo->OutputPin = OutputPin;

	if (bSetupUndoRedo)
	{
		ActionStack->AddAction(FRigVMAddInjectedNodeAction(InjectionInfo));
	}

	URigVMPin* PreviousInputPin = Pin;
	URigVMPin* PreviousOutputPin = Pin;
	if (Pin->InjectionInfos.Num() > 0)
	{
		PreviousInputPin = Pin->InjectionInfos.Last()->InputPin;
		PreviousOutputPin = Pin->InjectionInfos.Last()->OutputPin;
	}
	Pin->InjectionInfos.Add(InjectionInfo);

	Notify(ERigVMGraphNotifType::NodeAdded, UnitNode);

	// now update all of the links
	if (bAsInput)
	{
		FString PinDefaultValue = PreviousInputPin->GetDefaultValue();
		if (!PinDefaultValue.IsEmpty())
		{
			SetPinDefaultValue(InjectionInfo->InputPin, PinDefaultValue, true, false, false);
		}
		TArray<URigVMLink*> Links = PreviousInputPin->GetSourceLinks(true /* recursive */);
		BreakAllLinks(PreviousInputPin, true, false);
		AddLink(InjectionInfo->OutputPin, PreviousInputPin, false);
		if (Links.Num() > 0)
		{
			RewireLinks(PreviousInputPin, InjectionInfo->InputPin, true, false, Links);
		}
	}
	else
	{
		TArray<URigVMLink*> Links = PreviousOutputPin->GetTargetLinks(true /* recursive */);
		BreakAllLinks(PreviousOutputPin, false, false);
		AddLink(PreviousOutputPin, InjectionInfo->InputPin, false);
		if (Links.Num() > 0)
		{
			RewireLinks(PreviousOutputPin, InjectionInfo->OutputPin, false, false, Links);
		}
	}

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action);
	}

	return InjectionInfo;

}

URigVMInjectionInfo* URigVMController::AddInjectedNodeFromStructPath(const FString& InPinPath, bool bAsInput, const FString& InScriptStructPath, const FName& InMethodName, const FName& InInputPinName, const FName& InOutputPinName, const FString& InNodeName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return nullptr;
	}

	UScriptStruct* ScriptStruct = URigVMPin::FindObjectFromCPPTypeObjectPath<UScriptStruct>(InScriptStructPath);
	if (ScriptStruct == nullptr)
	{
		ReportErrorf(TEXT("Cannot find struct for path '%s'."), *InScriptStructPath);
		return nullptr;
	}

	return AddInjectedNode(InPinPath, bAsInput, ScriptStruct, InMethodName, InInputPinName, InOutputPinName, InNodeName, bSetupUndoRedo);
}

URigVMNode* URigVMController::EjectNodeFromPin(const FString& InPinPath, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (Graph->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Cannot eject nodes in function library graphs."));
		return nullptr;
	}

	URigVMPin* Pin = Graph->FindPin(InPinPath);
	if (Pin == nullptr)
	{
		ReportErrorf(TEXT("Cannot find pin '%s'."), *InPinPath);
		return nullptr;
	}

	if (!Pin->HasInjectedNodes())
	{
		ReportErrorf(TEXT("Pin '%s' has no injected nodes."), *InPinPath);
		return nullptr;
	}

	URigVMInjectionInfo* Injection = Pin->InjectionInfos.Last();

	UScriptStruct* ScriptStruct = Injection->UnitNode->GetScriptStruct();
	FName UnitNodeName = Injection->UnitNode->GetFName();
	FName MethodName = Injection->UnitNode->GetMethodName();
	FName InputPinName = Injection->InputPin->GetFName();
	FName OutputPinName = Injection->OutputPin->GetFName();

	TMap<FName, FString> DefaultValues;
	for (URigVMPin* PinOnNode : Injection->UnitNode->GetPins())
	{
		if (PinOnNode->GetDirection() == ERigVMPinDirection::Input ||
			PinOnNode->GetDirection() == ERigVMPinDirection::Visible ||
			PinOnNode->GetDirection() == ERigVMPinDirection::IO)
		{
			FString DefaultValue = PinOnNode->GetDefaultValue();
			PostProcessDefaultValue(PinOnNode, DefaultValue);
			DefaultValues.Add(PinOnNode->GetFName(), DefaultValue);
		}
	}

	FRigVMBaseAction Action;
	if (bSetupUndoRedo)
	{
		Action.Title = FString::Printf(TEXT("Eject Node"));
		ActionStack->BeginAction(Action);
	}

	FVector2D Position = Pin->GetNode()->GetPosition() + FVector2D(0.f, 12.f) * float(Pin->GetPinIndex());
	if (Pin->GetDirection() == ERigVMPinDirection::Output)
	{
		Position += FVector2D(250.f, 0.f);
	}
	else
	{
		Position -= FVector2D(250.f, 0.f);
	}

	URigVMNode* EjectedNode = AddUnitNode(ScriptStruct, MethodName, Position, FString(), bSetupUndoRedo);

	for (const TPair<FName, FString>& Pair : DefaultValues)
	{
		if (Pair.Value.IsEmpty())
		{
			continue;
		}
		if (URigVMPin* PinOnNode = EjectedNode->FindPin(Pair.Key.ToString()))
		{
			SetPinDefaultValue(PinOnNode, Pair.Value, true, bSetupUndoRedo, false);
		}
	}

	TArray<URigVMLink*> PreviousLinks = Injection->InputPin->GetSourceLinks(true);
	PreviousLinks.Append(Injection->OutputPin->GetTargetLinks(true));
	for (URigVMLink* PreviousLink : PreviousLinks)
	{
		PreviousLink->PrepareForCopy();
		PreviousLink->SourcePin = PreviousLink->TargetPin = nullptr;
	}

	RemoveNode(Injection->UnitNode, bSetupUndoRedo);

	FString OldNodeNamePrefix = UnitNodeName.ToString() + TEXT(".");
	FString NewNodeNamePrefix = EjectedNode->GetName() + TEXT(".");

	for (URigVMLink* PreviousLink : PreviousLinks)
	{
		FString SourcePinPath = PreviousLink->SourcePinPath;
		if (SourcePinPath.StartsWith(OldNodeNamePrefix))
		{
			SourcePinPath = NewNodeNamePrefix + SourcePinPath.RightChop(OldNodeNamePrefix.Len());
		}
		FString TargetPinPath = PreviousLink->TargetPinPath;
		if (TargetPinPath.StartsWith(OldNodeNamePrefix))
		{
			TargetPinPath = NewNodeNamePrefix + TargetPinPath.RightChop(OldNodeNamePrefix.Len());
		}

		URigVMPin* SourcePin = Graph->FindPin(SourcePinPath);
		URigVMPin* TargetPin = Graph->FindPin(TargetPinPath);
		AddLink(SourcePin, TargetPin, bSetupUndoRedo);
	}

	TArray<FName> NodeNamesToSelect;
	NodeNamesToSelect.Add(EjectedNode->GetFName());
	SetNodeSelection(NodeNamesToSelect, bSetupUndoRedo);

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action);
	}

	return EjectedNode;
}


bool URigVMController::Undo()
{
	if (!IsValidGraph())
	{
		return false;
	}

	TGuardValue<bool> GuardCompactness(bIgnoreRerouteCompactnessChanges, true);
	return ActionStack->Undo(this);
}

bool URigVMController::Redo()
{
	if (!IsValidGraph())
	{
		return false;
	}

	TGuardValue<bool> GuardCompactness(bIgnoreRerouteCompactnessChanges, true);
	return ActionStack->Redo(this);
}

bool URigVMController::OpenUndoBracket(const FString& InTitle)
{
	if (!IsValidGraph())
	{
		return false;
	}
	return ActionStack->OpenUndoBracket(InTitle);
}

bool URigVMController::CloseUndoBracket()
{
	if (!IsValidGraph())
	{
		return false;
	}
	return ActionStack->CloseUndoBracket();
}

bool URigVMController::CancelUndoBracket()
{
	if (!IsValidGraph())
	{
		return false;
	}
	return ActionStack->CancelUndoBracket();
}

FString URigVMController::ExportNodesToText(const TArray<FName>& InNodeNames)
{
	if (!IsValidGraph())
	{
		return FString();
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	UnMarkAllObjects(EObjectMark(OBJECTMARK_TagExp | OBJECTMARK_TagImp));

	FStringOutputDevice Archive;
	const FExportObjectInnerContext Context;

	TArray<FName> AllNodeNames = InNodeNames;
	for (const FName& NodeName : InNodeNames)
	{
		if (URigVMNode* Node = Graph->FindNodeByName(NodeName))
		{
			for (URigVMPin* Pin : Node->GetPins())
			{
				for (URigVMInjectionInfo* Injection : Pin->GetInjectedNodes())
				{
					AllNodeNames.AddUnique(Injection->UnitNode->GetFName());
				}
			}
		}
	}

	// Export each of the selected nodes
	for (const FName& NodeName : InNodeNames)
	{
		if (URigVMNode* Node = Graph->FindNodeByName(NodeName))
		{
			UExporter::ExportToOutputDevice(&Context, Node, NULL, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false, Node->GetOuter());
		}
	}

	for (URigVMLink* Link : Graph->Links)
	{
		URigVMPin* SourcePin = Link->GetSourcePin();
		URigVMPin* TargetPin = Link->GetTargetPin();
		if (SourcePin && TargetPin)
		{
			if (!AllNodeNames.Contains(SourcePin->GetNode()->GetFName()))
			{
				continue;
			}
			if (!AllNodeNames.Contains(TargetPin->GetNode()->GetFName()))
			{
				continue;
			}
			Link->PrepareForCopy();
			UExporter::ExportToOutputDevice(&Context, Link, NULL, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false, Link->GetOuter());
		}
	}

	return MoveTemp(Archive);
}

FString URigVMController::ExportSelectedNodesToText()
{
	if (!IsValidGraph())
	{
		return FString();
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	return ExportNodesToText(Graph->GetSelectNodes());
}

struct FRigVMControllerObjectFactory : public FCustomizableTextObjectFactory
{
public:
	URigVMController* Controller;
	TArray<URigVMNode*> CreatedNodes;
	TMap<FName, FName> NodeNameMap;
	TArray<URigVMLink*> CreatedLinks;
public:
	FRigVMControllerObjectFactory(URigVMController* InController)
		: FCustomizableTextObjectFactory(GWarn)
		, Controller(InController)
	{
	}

protected:
	virtual bool CanCreateClass(UClass* ObjectClass, bool& bOmitSubObjs) const override
	{
		if (URigVMNode* DefaultNode = Cast<URigVMNode>(ObjectClass->GetDefaultObject()))
		{
			// bOmitSubObjs = true;
			return true;
		}
		if (URigVMLink* DefaultLink = Cast<URigVMLink>(ObjectClass->GetDefaultObject()))
		{
			return true;
		}

		return false;
	}

	virtual void UpdateObjectName(UClass* ObjectClass, FName& InOutObjName) override
	{
		if (URigVMNode* DefaultNode = Cast<URigVMNode>(ObjectClass->GetDefaultObject()))
		{
			FName ValidName = *Controller->GetValidNodeName(InOutObjName.ToString());
			NodeNameMap.Add(InOutObjName, ValidName);
			InOutObjName = ValidName;
		}
	}

	virtual void ProcessConstructedObject(UObject* CreatedObject) override
	{
		if (URigVMNode* CreatedNode = Cast<URigVMNode>(CreatedObject))
		{
			CreatedNodes.AddUnique(CreatedNode);

			for (URigVMPin* Pin : CreatedNode->GetPins())
			{
				for (URigVMInjectionInfo* Injection : Pin->GetInjectedNodes())
				{
					ProcessConstructedObject(Injection->UnitNode);

					FName NewName = Injection->UnitNode->GetFName();
					UpdateObjectName(URigVMNode::StaticClass(), NewName);
					Injection->UnitNode->Rename(*NewName.ToString(), nullptr);
					Injection->InputPin = Injection->UnitNode->FindPin(Injection->InputPin->GetName());
					Injection->OutputPin = Injection->UnitNode->FindPin(Injection->OutputPin->GetName());
				}
			}
		}
		else if (URigVMLink* CreatedLink = Cast<URigVMLink>(CreatedObject))
		{
			CreatedLinks.Add(CreatedLink);
		}
	}
};

bool URigVMController::CanImportNodesFromText(const FString& InText)
{
	if (!IsValidGraph())
	{
		return false;
	}

	if (GetGraph()->IsA<URigVMFunctionLibrary>())
	{
		return false;
	}

	FRigVMControllerObjectFactory Factory(nullptr);
	return Factory.CanCreateObjectsFromText(InText);
}

TArray<FName> URigVMController::ImportNodesFromText(const FString& InText, bool bSetupUndoRedo)
{
	TArray<FName> NodeNames;
	if (!IsValidGraph())
	{
		return NodeNames;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	FRigVMControllerObjectFactory Factory(this);
	Factory.ProcessBuffer(Graph, RF_Transactional, InText);

	if (Factory.CreatedNodes.Num() == 0)
	{
		return NodeNames;
	}

	if (bSetupUndoRedo)
	{
		OpenUndoBracket(TEXT("Importing Nodes from Text"));
	}

	FRigVMInverseAction AddNodesAction;
	if (bSetupUndoRedo)
	{
		ActionStack->BeginAction(AddNodesAction);
	}

	FRigVMUnitNodeCreatedContext::FScope UnitNodeCreatedScope(UnitNodeCreatedContext, ERigVMNodeCreatedReason::Paste);
	for (URigVMNode* CreatedNode : Factory.CreatedNodes)
	{
		if(!CanAddNode(CreatedNode, true))
		{
			continue;
		}

		Graph->Nodes.Add(CreatedNode);

		if (bSetupUndoRedo)
		{
			ActionStack->AddAction(FRigVMRemoveNodeAction(CreatedNode, this));
		}

		if (URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(CreatedNode))
		{
			if (UnitNodeCreatedContext.IsValid())
			{
				if (TSharedPtr<FStructOnScope> StructScope = UnitNode->ConstructStructInstance())
				{
					TGuardValue<FName> NodeNameScope(UnitNodeCreatedContext.NodeName, UnitNode->GetFName());
					FRigVMStruct* StructInstance = (FRigVMStruct*)StructScope->GetStructMemory();
					StructInstance->OnUnitNodeCreated(UnitNodeCreatedContext);
				}
			}
		}

		if (URigVMFunctionReferenceNode* FunctionRefNode = Cast<URigVMFunctionReferenceNode>(CreatedNode))
		{
			if (URigVMFunctionLibrary* FunctionLibrary = FunctionRefNode->GetLibrary())
			{
				if (URigVMLibraryNode* FunctionDefinition = FunctionRefNode->GetReferencedNode())
				{
					FunctionLibrary->FunctionReferences.FindOrAdd(FunctionDefinition).FunctionReferences.Add(FunctionRefNode);
				}
			}
		}

		Notify(ERigVMGraphNotifType::NodeAdded, CreatedNode);

		NodeNames.Add(CreatedNode->GetFName());
	}

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(AddNodesAction);
	}

	if (Factory.CreatedLinks.Num() > 0)
	{
		FRigVMBaseAction AddLinksAction;
		if (bSetupUndoRedo)
		{
			ActionStack->BeginAction(AddLinksAction);
		}

		for (URigVMLink* CreatedLink : Factory.CreatedLinks)
		{
			FString SourceLeft, SourceRight, TargetLeft, TargetRight;
			if (URigVMPin::SplitPinPathAtStart(CreatedLink->SourcePinPath, SourceLeft, SourceRight) &&
				URigVMPin::SplitPinPathAtStart(CreatedLink->TargetPinPath, TargetLeft, TargetRight))
			{
				const FName* NewSourceNodeName = Factory.NodeNameMap.Find(*SourceLeft);
				const FName* NewTargetNodeName = Factory.NodeNameMap.Find(*TargetLeft);
				if (NewSourceNodeName && NewTargetNodeName)
				{
					CreatedLink->SourcePinPath = URigVMPin::JoinPinPath(NewSourceNodeName->ToString(), SourceRight);
					CreatedLink->TargetPinPath = URigVMPin::JoinPinPath(NewTargetNodeName->ToString(), TargetRight);
					URigVMPin* SourcePin = CreatedLink->GetSourcePin();
					URigVMPin* TargetPin = CreatedLink->GetTargetPin();
					if (SourcePin && TargetPin)
					{
						Graph->Links.Add(CreatedLink);
						SourcePin->Links.Add(CreatedLink);
						TargetPin->Links.Add(CreatedLink);

						if (bSetupUndoRedo)
						{
							ActionStack->AddAction(FRigVMAddLinkAction(SourcePin, TargetPin));
						}
						Notify(ERigVMGraphNotifType::LinkAdded, CreatedLink);
						continue;
					}
				}
			}

			ReportErrorf(TEXT("Cannot import link '%s -> %s'."), *CreatedLink->SourcePinPath, *CreatedLink->TargetPinPath);
			DestroyObject(CreatedLink);
		}

		if (bSetupUndoRedo)
		{
			ActionStack->EndAction(AddLinksAction);
		}
	}

	if (bSetupUndoRedo)
	{
		CloseUndoBracket();
	}

	return NodeNames;
}

FName URigVMController::GetUniqueName(const FName& InName, TFunction<bool(const FName&)> IsNameAvailableFunction)
{
	FString NamePrefix = InName.ToString();
	int32 NameSuffix = 0;
	FString Name = NamePrefix;
	while (!IsNameAvailableFunction(*Name))
	{
		NameSuffix++;
		Name = FString::Printf(TEXT("%s_%d"), *NamePrefix, NameSuffix);
	}
	return *Name;
}

URigVMCollapseNode* URigVMController::CollapseNodes(const TArray<FName>& InNodeNames, const FString& InCollapseNodeName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	TArray<URigVMNode*> Nodes;
	for (const FName& NodeName : InNodeNames)
	{
		URigVMNode* Node = Graph->FindNodeByName(NodeName);
		if (Node == nullptr)
		{
			ReportErrorf(TEXT("Cannot find node '%s'."), *NodeName.ToString());
			return nullptr;
		}
		Nodes.AddUnique(Node);
	}

	return CollapseNodes(Nodes, InCollapseNodeName, bSetupUndoRedo);
}

TArray<URigVMNode*> URigVMController::ExpandLibraryNode(const FName& InNodeName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return TArray<URigVMNode*>();
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMNode* Node = Graph->FindNodeByName(InNodeName);
	if (Node == nullptr)
	{
		ReportErrorf(TEXT("Cannot find collapse node '%s'."), *InNodeName.ToString());
		return TArray<URigVMNode*>();
	}

	URigVMLibraryNode* LibNode = Cast<URigVMLibraryNode>(Node);
	if (LibNode == nullptr)
	{
		ReportErrorf(TEXT("Node '%s' is not a library node (not collapse nor function)."), *InNodeName.ToString());
		return TArray<URigVMNode*>();
	}

	return ExpandLibraryNode(LibNode, bSetupUndoRedo);
}

#endif

URigVMCollapseNode* URigVMController::CollapseNodes(const TArray<URigVMNode*>& InNodes, const FString& InCollapseNodeName, bool bSetupUndoRedo)
{
	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (Graph->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Cannot collapse nodes in function library graphs."));
		return nullptr;
	}

	TArray<URigVMNode*> Nodes;
	for (URigVMNode* Node : InNodes)
	{
		if (!IsValidNodeForGraph(Node))
		{
			return nullptr;
		}

		// filter out certain nodes
		if (Node->IsEvent())
		{
			continue;
		}

		if (Node->IsA<URigVMFunctionEntryNode>() ||
			Node->IsA<URigVMFunctionReturnNode>())
		{
			continue;
		}

		Nodes.Add(Node);
	}

	if (Nodes.Num() == 0)
	{
		return nullptr;
	}

	FBox2D Bounds = FBox2D(EForceInit::ForceInit);
	TArray<FName> NodeNames;
	for (URigVMNode* Node : Nodes)
	{
		NodeNames.Add(Node->GetFName());
		Bounds += Node->GetPosition();
	}

  	FVector2D Diagonal = Bounds.Max - Bounds.Min;
	FVector2D Center = (Bounds.Min + Bounds.Max) * 0.5f;

	bool bContainsOutputs = false;

	TArray<URigVMPin*> PinsToCollapse;
	TMap<URigVMPin*, URigVMPin*> CollapsedPins;
	TArray<URigVMLink*> LinksToRewire;
	TArray<URigVMLink*> AllLinks = Graph->GetLinks();

	// find all pins to collapse. we need this to find out if
	// we might have a parent pin of a given linked pin already 
	// collapsed.
	for (URigVMLink* Link : AllLinks)
	{
		URigVMPin* SourcePin = Link->GetSourcePin();
		URigVMPin* TargetPin = Link->GetTargetPin();
		bool bSourceToBeCollapsed = Nodes.Contains(SourcePin->GetNode());
		bool bTargetToBeCollapsed = Nodes.Contains(TargetPin->GetNode());
		if (bSourceToBeCollapsed == bTargetToBeCollapsed)
		{
			continue;
		}

		URigVMPin* PinToCollapse = SourcePin;
		PinsToCollapse.AddUnique(PinToCollapse);
		LinksToRewire.Add(Link);
	}

	// make sure that for execute pins we are on one branch only
	TArray<URigVMPin*> InputExecutePins;
	TArray<URigVMPin*> IntermediateExecutePins;
	TArray<URigVMPin*> OutputExecutePins;

	// first collect the output execute pins
	for (URigVMLink* Link : LinksToRewire)
	{
		URigVMPin* ExecutePin = Link->GetSourcePin();
		if (!ExecutePin->IsExecuteContext())
		{
			continue;
		}
		if (!Nodes.Contains(ExecutePin->GetNode()))
		{
			continue;
		}
		if (!OutputExecutePins.IsEmpty())
		{
			if (bSetupUndoRedo)
			{
				ReportAndNotifyErrorf(
					TEXT("Only one set of execute branches can be collapsed, pin %s and %s are on separate branches"),
					*OutputExecutePins[0]->GetPinPath(),
					*ExecutePin->GetPinPath()
				);
			}
			return nullptr;
		}
		OutputExecutePins.Add(ExecutePin);

		while (ExecutePin)
		{
			if (IntermediateExecutePins.Contains(ExecutePin))
			{
				if (bSetupUndoRedo)
				{
					ReportAndNotifyErrorf(TEXT("Only one set of execute branches can be collapsed."));
				}
				return nullptr;
			}
			IntermediateExecutePins.Add(ExecutePin);

			// walk backwards and find all "known execute pins"
			URigVMNode* ExecuteNode = ExecutePin->GetNode();
			for (URigVMPin* Pin : ExecuteNode->GetPins())
			{
				if (Pin->GetDirection() != ERigVMPinDirection::Input &&
					Pin->GetDirection() != ERigVMPinDirection::IO)
				{
					continue;
				}
				if (!Pin->IsExecuteContext())
				{
					continue;
				}
				TArray<URigVMLink*> SourceLinks = Pin->GetSourceLinks();
				ExecutePin = nullptr;
				if (SourceLinks.Num() > 0)
				{
					URigVMPin* PreviousExecutePin = SourceLinks[0]->GetSourcePin();
					if (Nodes.Contains(PreviousExecutePin->GetNode()))
					{
						if (Pin != IntermediateExecutePins.Last())
						{
							IntermediateExecutePins.Add(Pin);
						}
						ExecutePin = PreviousExecutePin;
						break;
					}
				}
			}
		}
	}
	for (URigVMLink* Link : LinksToRewire)
	{
		URigVMPin* ExecutePin = Link->GetTargetPin();
		if (!ExecutePin->IsExecuteContext())
		{
			continue;
		}
		if (!Nodes.Contains(ExecutePin->GetNode()))
		{
			continue;
		}
		if (!IntermediateExecutePins.Contains(ExecutePin) && !IntermediateExecutePins.IsEmpty())
		{
			if (bSetupUndoRedo)
			{
				ReportAndNotifyErrorf(TEXT("Only one set of execute branches can be collapsed"));
			}
			return nullptr;
		}

		if (!InputExecutePins.IsEmpty())
		{
			if (bSetupUndoRedo)
			{
				ReportAndNotifyErrorf(
					TEXT("Only one set of execute branches can be collapsed, pin %s and %s are on separate branches"),
					*InputExecutePins[0]->GetPinPath(),
					*ExecutePin->GetPinPath()
				);
			}
			return nullptr;
		}
		InputExecutePins.Add(ExecutePin);
	}

	FRigVMCollapseNodesAction CollapseAction;
	CollapseAction.Title = TEXT("Collapse Nodes");

	if (bSetupUndoRedo)
	{
		ActionStack->BeginAction(CollapseAction);
	}

	FString CollapseNodeName = GetValidNodeName(InCollapseNodeName.IsEmpty() ? FString(TEXT("CollapseNode")) : InCollapseNodeName);
	URigVMCollapseNode* CollapseNode = NewObject<URigVMCollapseNode>(Graph, *CollapseNodeName);
	CollapseNode->ContainedGraph = NewObject<URigVMGraph>(CollapseNode, TEXT("ContainedGraph"));
	CollapseNode->Position = Center;
	Graph->Nodes.Add(CollapseNode);

	// now looper over the links to be rewired
	for (URigVMLink* Link : LinksToRewire)
	{
		bool bSourceToBeCollapsed = Nodes.Contains(Link->GetSourcePin()->GetNode());
		bool bTargetToBeCollapsed = Nodes.Contains(Link->GetTargetPin()->GetNode());

		URigVMPin* PinToCollapse = bSourceToBeCollapsed ? Link->GetSourcePin() : Link->GetTargetPin();
		if (CollapsedPins.Contains(PinToCollapse))
		{
			continue;
		}

		if (PinToCollapse->IsExecuteContext())
		{
			bool bFoundExistingPin = false;
			for (URigVMPin* ExistingPin : CollapseNode->Pins)
			{
				if (ExistingPin->IsExecuteContext())
				{
					CollapsedPins.Add(PinToCollapse, ExistingPin);
					bFoundExistingPin = true;
					break;
				}
			}

			if (bFoundExistingPin)
			{
				continue;
			}
		}

		// for links that connect to the right side of the collapse
		// node, we need to skip sub pins of already exposed pins
		if (bSourceToBeCollapsed)
		{
			bool bParentPinCollapsed = false;
			URigVMPin* ParentPin = PinToCollapse->GetParentPin();
			while (ParentPin != nullptr)
			{
				if (PinsToCollapse.Contains(ParentPin))
				{
					bParentPinCollapsed = true;
					break;
				}
				ParentPin = ParentPin->GetParentPin();
			}

			if (bParentPinCollapsed)
			{
				continue;
			}
		}

		FName PinName = GetUniqueName(PinToCollapse->GetFName(), [CollapseNode](const FName& InName) {
			return CollapseNode->FindPin(InName.ToString()) == nullptr;
		});

		URigVMPin* CollapsedPin = NewObject<URigVMPin>(CollapseNode, PinName);
		ConfigurePinFromPin(CollapsedPin, PinToCollapse);

		if (CollapsedPin->IsExecuteContext())
		{
			CollapsedPin->Direction = ERigVMPinDirection::IO;
			bContainsOutputs = true;
		}
		else if (CollapsedPin->GetDirection() == ERigVMPinDirection::IO)
		{
			CollapsedPin->Direction = ERigVMPinDirection::Input;
		}

		if (CollapsedPin->IsStruct())
		{
			AddPinsForStruct(CollapsedPin->GetScriptStruct(), CollapseNode, CollapsedPin, CollapsedPin->GetDirection(), FString(), false);
		}

		bContainsOutputs = bContainsOutputs || bSourceToBeCollapsed;

		CollapseNode->Pins.Add(CollapsedPin);

		FPinState PinState = GetPinState(PinToCollapse);
		ApplyPinState(CollapsedPin, PinState);

		CollapsedPins.Add(PinToCollapse, CollapsedPin);
	}

	Notify(ERigVMGraphNotifType::NodeAdded, CollapseNode);

	URigVMFunctionEntryNode* EntryNode = nullptr;
	URigVMFunctionReturnNode* ReturnNode = nullptr;
	{
		FRigVMControllerGraphGuard GraphGuard(this, CollapseNode->GetContainedGraph(), false);

		EntryNode = NewObject<URigVMFunctionEntryNode>(CollapseNode->ContainedGraph, TEXT("Entry"));
		CollapseNode->ContainedGraph->Nodes.Add(EntryNode);
		EntryNode->Position = -Diagonal * 0.5f - FVector2D(250.f, 0.f);
		RefreshFunctionPins(EntryNode, false);

		Notify(ERigVMGraphNotifType::NodeAdded, EntryNode);

		if (bContainsOutputs)
		{
			ReturnNode = NewObject<URigVMFunctionReturnNode>(CollapseNode->ContainedGraph, TEXT("Return"));
			CollapseNode->ContainedGraph->Nodes.Add(ReturnNode);
			ReturnNode->Position = FVector2D(Diagonal.X, -Diagonal.Y) * 0.5f + FVector2D(300.f, 0.f);
			RefreshFunctionPins(ReturnNode, false);

			Notify(ERigVMGraphNotifType::NodeAdded, ReturnNode);
		}
	}

	// create the new nodes within the collapse node
	TArray<FName> ContainedNodeNames;
	{
		FString TextContent = ExportNodesToText(NodeNames);

		FRigVMControllerGraphGuard GraphGuard(this, CollapseNode->GetContainedGraph(), false);
		ContainedNodeNames = ImportNodesFromText(TextContent, false);

		// move the nodes to the right place
		for (const FName& ContainedNodeName : ContainedNodeNames)
		{
			if (URigVMNode* ContainedNode = CollapseNode->GetContainedGraph()->FindNodeByName(ContainedNodeName))
			{
				SetNodePosition(ContainedNode, ContainedNode->Position - Center, false, false);
			}
		}

		for (URigVMLink* LinkToRewire : LinksToRewire)
		{
			URigVMPin* SourcePin = LinkToRewire->GetSourcePin();
			URigVMPin* TargetPin = LinkToRewire->GetTargetPin();

			if (Nodes.Contains(SourcePin->GetNode()))
			{
				// if the parent pin of this was collapsed
				// it's possible that the child pin wasn't.
				if (!CollapsedPins.Contains(SourcePin))
				{
					continue;
				}

				URigVMPin* CollapsedPin = CollapsedPins.FindChecked(SourcePin);
				SourcePin = CollapseNode->ContainedGraph->FindPin(SourcePin->GetPinPath());
				TargetPin = ReturnNode->FindPin(CollapsedPin->GetName());
			}
			else
			{
				URigVMPin* CollapsedPin = CollapsedPins.FindChecked(TargetPin);
				SourcePin = EntryNode->FindPin(CollapsedPin->GetName());
				TargetPin = CollapseNode->ContainedGraph->FindPin(TargetPin->GetPinPath());
			}

			if (SourcePin && TargetPin)
			{
				if (!SourcePin->IsLinkedTo(TargetPin))
				{
					AddLink(SourcePin, TargetPin, false);
				}
			}
		}
	}

	TArray<URigVMLink*> RewiredLinks;
	for (URigVMLink* LinkToRewire : LinksToRewire)
	{
		if (RewiredLinks.Contains(LinkToRewire))
		{
			continue;
		}

		URigVMPin* SourcePin = LinkToRewire->GetSourcePin();
		URigVMPin* TargetPin = LinkToRewire->GetTargetPin();

		if (Nodes.Contains(SourcePin->GetNode()))
		{
			FString SegmentPath;
			URigVMPin* PinToCheck = SourcePin;

			URigVMPin** CollapsedPinPtr = CollapsedPins.Find(PinToCheck);
			while (CollapsedPinPtr == nullptr)
			{
				if (SegmentPath.IsEmpty())
				{
					SegmentPath = PinToCheck->GetName();
				}
				else
				{
					SegmentPath = URigVMPin::JoinPinPath(PinToCheck->GetName(), SegmentPath);
				}

				PinToCheck = PinToCheck->GetParentPin();
				check(PinToCheck);

				CollapsedPinPtr = CollapsedPins.Find(PinToCheck);
			}

			URigVMPin* CollapsedPin = *CollapsedPinPtr;
			check(CollapsedPin);

			if (!SegmentPath.IsEmpty())
			{
				CollapsedPin = CollapsedPin->FindSubPin(SegmentPath);
				check(CollapsedPin);
			}

			TArray<URigVMLink*> TargetLinks = SourcePin->GetTargetLinks(false);
			for (URigVMLink* TargetLink : TargetLinks)
			{
				TargetPin = TargetLink->GetTargetPin();
				if (!CollapsedPin->IsLinkedTo(TargetPin))
				{
					AddLink(CollapsedPin, TargetPin, false);
				}
			}
			RewiredLinks.Append(TargetLinks);
		}
		else
		{
			URigVMPin* CollapsedPin = CollapsedPins.FindChecked(TargetPin);
			if (!SourcePin->IsLinkedTo(CollapsedPin))
			{
				AddLink(SourcePin, CollapsedPin, false);
			}
		}

		RewiredLinks.Add(LinkToRewire);
	}

	if (ReturnNode)
	{
		struct Local
		{
			static bool IsLinkedToEntryNode(URigVMNode* InNode, TMap<URigVMNode*, bool>& CachedMap)
			{
				if (InNode->IsA<URigVMFunctionEntryNode>())
				{
					return true;
				}

				if (!CachedMap.Contains(InNode))
				{
					CachedMap.Add(InNode, false);

					if (URigVMPin* ExecuteContextPin = InNode->FindPin(FRigVMStruct::ExecuteContextName.ToString()))
					{
						TArray<URigVMPin*> SourcePins = ExecuteContextPin->GetLinkedSourcePins();
						for (URigVMPin* SourcePin : SourcePins)
						{
							if (IsLinkedToEntryNode(SourcePin->GetNode(), CachedMap))
							{
								CachedMap.FindOrAdd(InNode) = true;
								break;
							}
						}
					}
				}

				return CachedMap.FindChecked(InNode);
			}
		};

		// check if there is a last node on the top level block what we need to hook up
		TMap<URigVMNode*, bool> IsContainedNodeLinkedToEntryNode;
		for (URigVMNode* ContainedNode : CollapseNode->GetContainedNodes())
		{
			if (!ContainedNode->IsMutable())
			{
				continue;
			}

			// make sure the node doesn't have any mutable nodes connected to its executecontext
			if (URigVMPin* ExecuteContextPin = ContainedNode->FindPin(FRigVMStruct::ExecuteContextName.ToString()))
			{
				if (ExecuteContextPin->GetDirection() != ERigVMPinDirection::IO)
				{
					continue;
				}

				if (ExecuteContextPin->GetTargetLinks().Num() > 0)
				{
					continue;
				}

				if (!Local::IsLinkedToEntryNode(ContainedNode, IsContainedNodeLinkedToEntryNode))
				{
					continue;
				}

				FRigVMControllerGraphGuard GraphGuard(this, CollapseNode->GetContainedGraph(), false);
				AddLink(ExecuteContextPin, ReturnNode->FindPin(FRigVMStruct::ExecuteContextName.ToString()), false);
				break;
			}
		}
	}

	for (const FName& NodeToRemove : NodeNames)
	{
		RemoveNodeByName(NodeToRemove, false, true);
	}

	if (bSetupUndoRedo)
	{
		CollapseAction.LibraryNodePath = CollapseNode->GetName();
		for (URigVMNode* InNode : InNodes)
		{
			CollapseAction.CollapsedNodesPaths.Add(InNode->GetName());
		}
		ActionStack->EndAction(CollapseAction);
	}

	return CollapseNode;
}

TArray<URigVMNode*> URigVMController::ExpandLibraryNode(URigVMLibraryNode* InNode, bool bSetupUndoRedo)
{
	if (!IsValidNodeForGraph(InNode))
	{
		return TArray<URigVMNode*>();
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (Graph->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Cannot expand nodes in function library graphs."));
		return TArray<URigVMNode*>();
	}

	TArray<URigVMNode*> ContainedNodes = InNode->GetContainedNodes();
	TArray<URigVMLink*> ContainedLinks = InNode->GetContainedLinks();
	if (ContainedNodes.Num() == 0)
	{
		return TArray<URigVMNode*>();
	}

	FRigVMExpandNodeAction ExpandAction;
	ExpandAction.Title = FString::Printf(TEXT("Expand '%s' Node"), *InNode->GetName());

	if (bSetupUndoRedo)
	{
		ActionStack->BeginAction(ExpandAction);
	}

	TArray<FName> NodeNames;
	FBox2D Bounds = FBox2D(EForceInit::ForceInit);
	{
		TArray<URigVMNode*> FilteredNodes;
		for (URigVMNode* Node : ContainedNodes)
		{
			if (Cast<URigVMFunctionEntryNode>(Node) != nullptr ||
				Cast<URigVMFunctionReturnNode>(Node) != nullptr)
			{
				continue;
			}

			NodeNames.Add(Node->GetFName());
			FilteredNodes.Add(Node);
			Bounds += Node->GetPosition();
		}
		ContainedNodes = FilteredNodes;
	}

	if (ContainedNodes.Num() == 0)
	{
		return TArray<URigVMNode*>();
	}

	FVector2D Diagonal = Bounds.Max - Bounds.Min;
	FVector2D Center = (Bounds.Min + Bounds.Max) * 0.5f;

	FString TextContent;
	{
		FRigVMControllerGraphGuard GraphGuard(this, InNode->GetContainedGraph(), false);
		TextContent = ExportNodesToText(NodeNames);
	}

	TArray<FName> ExpandedNodeNames = ImportNodesFromText(TextContent, false);
	TArray<URigVMNode*> ExpandedNodes;
	for (const FName& ExpandedNodeName : ExpandedNodeNames)
	{
		URigVMNode* ExpandedNode = Graph->FindNodeByName(ExpandedNodeName);
		check(ExpandedNode);
		ExpandedNodes.Add(ExpandedNode);
	}

	check(ExpandedNodeNames.Num() == NodeNames.Num());

	TMap<FName, FName> NodeNameMap;
	for (int32 NodeNameIndex = 0; NodeNameIndex < NodeNames.Num(); NodeNameIndex++)
	{
		NodeNameMap.Add(NodeNames[NodeNameIndex], ExpandedNodeNames[NodeNameIndex]);
		SetNodePosition(ExpandedNodes[NodeNameIndex], InNode->Position + ContainedNodes[NodeNameIndex]->Position - Center, false, false);
	}

	// a) store all of the pin defaults off the library node
	TMap<FString, FPinState> PinStates = GetPinStates(InNode);

	// b) create a map of new links to create by following the links to / from the library node
	TMap<FString, TArray<FString>> ToLibraryNode;
	TMap<FString, TArray<FString>> FromLibraryNode;
	TArray<URigVMPin*> LibraryPinsToReroute;

	TArray<URigVMLink*> LibraryLinks = InNode->GetLinks();
	for (URigVMLink* Link : LibraryLinks)
	{
		if (Link->GetTargetPin()->GetNode() == InNode)
		{
			if (!Link->GetTargetPin()->IsRootPin())
			{
				LibraryPinsToReroute.AddUnique(Link->GetTargetPin()->GetRootPin());
			}

			FString NodeName, PinPath;
			URigVMPin::SplitPinPathAtStart(Link->GetTargetPin()->GetPinPath(), NodeName, PinPath);
			ToLibraryNode.FindOrAdd(PinPath).Add(Link->GetSourcePin()->GetPinPath());
		}
		else
		{
			if (!Link->GetSourcePin()->IsRootPin())
			{
				LibraryPinsToReroute.AddUnique(Link->GetSourcePin()->GetRootPin());
			}

			FString NodeName, PinPath;
			URigVMPin::SplitPinPathAtStart(Link->GetSourcePin()->GetPinPath(), NodeName, PinPath);
			FromLibraryNode.FindOrAdd(PinPath).Add(Link->GetTargetPin()->GetPinPath());
		}
	}

	// c) create a map from the entry node to the contained graph
	TMap<FString, TArray<FString>> FromEntryNode;
	if (URigVMFunctionEntryNode* EntryNode = InNode->GetEntryNode())
	{
		TArray<URigVMLink*> EntryLinks = EntryNode->GetLinks();
		for (URigVMLink* Link : EntryLinks)
		{
			if (Link->GetSourcePin()->GetNode() != EntryNode)
			{
				continue;
			}

			if (!Link->GetSourcePin()->IsRootPin())
			{
				LibraryPinsToReroute.AddUnique(InNode->FindPin(Link->GetSourcePin()->GetRootPin()->GetName()));
			}

			FString NodeName, PinPath;
			URigVMPin::SplitPinPathAtStart(Link->GetSourcePin()->GetPinPath(), NodeName, PinPath);

			TArray<FString>& LinkedPins = FromEntryNode.FindOrAdd(PinPath);

			URigVMPin::SplitPinPathAtStart(Link->GetTargetPin()->GetPinPath(), NodeName, PinPath);
			NodeName = NodeNameMap.FindChecked(*NodeName).ToString();
			LinkedPins.Add(URigVMPin::JoinPinPath(NodeName, PinPath));
		}
	}

	// d) create a map from the contained graph from to the return node
	TMap<FString, TArray<FString>> ToReturnNode;
	if (URigVMFunctionReturnNode* ReturnNode = InNode->GetReturnNode())
	{
		TArray<URigVMLink*> ReturnLinks = ReturnNode->GetLinks();
		for (URigVMLink* Link : ReturnLinks)
		{
			if (Link->GetTargetPin()->GetNode() != ReturnNode)
			{
				continue;
			}

			if (!Link->GetTargetPin()->IsRootPin())
			{
				LibraryPinsToReroute.AddUnique(InNode->FindPin(Link->GetTargetPin()->GetRootPin()->GetName()));
			}

			FString NodeName, PinPath;
			URigVMPin::SplitPinPathAtStart(Link->GetTargetPin()->GetPinPath(), NodeName, PinPath);

			TArray<FString>& LinkedPins = ToReturnNode.FindOrAdd(PinPath);

			URigVMPin::SplitPinPathAtStart(Link->GetSourcePin()->GetPinPath(), NodeName, PinPath);
			NodeName = NodeNameMap.FindChecked(*NodeName).ToString();
			LinkedPins.Add(URigVMPin::JoinPinPath(NodeName, PinPath));
		}
	}

	// e) restore all pin states on pins linked to the entry node
	for (const TPair<FString, TArray<FString>>& FromEntryPair : FromEntryNode)
	{
		FString EntryPinPath = FromEntryPair.Key;
		const FPinState* CollapsedPinState = PinStates.Find(EntryPinPath);
		if (CollapsedPinState == nullptr)
		{
			continue;
		}

		for (const FString& EntryTargetLinkPinPath : FromEntryPair.Value)
		{
			if (URigVMPin* TargetPin = GetGraph()->FindPin(EntryTargetLinkPinPath))
			{
				ApplyPinState(TargetPin, *CollapsedPinState);
			}
		}
	}

	// f) create reroutes for all pins which had wires on sub pins
	TMap<FString, URigVMPin*> ReroutedInputPins;
	TMap<FString, URigVMPin*> ReroutedOutputPins;
	FVector2D RerouteInputPosition = InNode->Position + FVector2D(-Diagonal.X, -Diagonal.Y) * 0.5 + FVector2D(-200.f, 0.f);
	FVector2D RerouteOutputPosition = InNode->Position + FVector2D(Diagonal.X, -Diagonal.Y) * 0.5 + FVector2D(250.f, 0.f);
	for (URigVMPin* LibraryPinToReroute : LibraryPinsToReroute)
	{
		if (LibraryPinToReroute->GetDirection() == ERigVMPinDirection::Input ||
			LibraryPinToReroute->GetDirection() == ERigVMPinDirection::IO)
		{
			URigVMRerouteNode* RerouteNode =
				AddFreeRerouteNode(
					true,
					LibraryPinToReroute->GetCPPType(),
					*LibraryPinToReroute->GetCPPTypeObject()->GetPathName(),
					false,
					NAME_None,
					LibraryPinToReroute->GetDefaultValue(),
					RerouteInputPosition,
					FString::Printf(TEXT("Reroute_%s"), *LibraryPinToReroute->GetName()),
					false);

			RerouteInputPosition += FVector2D(0.f, 150.f);

			URigVMPin* ReroutePin = RerouteNode->FindPin(URigVMRerouteNode::ValueName);
			ApplyPinState(ReroutePin, GetPinState(LibraryPinToReroute));
			ReroutedInputPins.Add(LibraryPinToReroute->GetName(), ReroutePin);
			ExpandedNodes.Add(RerouteNode);
		}

		if (LibraryPinToReroute->GetDirection() == ERigVMPinDirection::Output ||
			LibraryPinToReroute->GetDirection() == ERigVMPinDirection::IO)
		{
			URigVMRerouteNode* RerouteNode =
				AddFreeRerouteNode(
					true,
					LibraryPinToReroute->GetCPPType(),
					*LibraryPinToReroute->GetCPPTypeObject()->GetPathName(),
					false,
					NAME_None,
					LibraryPinToReroute->GetDefaultValue(),
					RerouteOutputPosition,
					FString::Printf(TEXT("Reroute_%s"), *LibraryPinToReroute->GetName()),
					false);

			RerouteOutputPosition += FVector2D(0.f, 150.f);

			URigVMPin* ReroutePin = RerouteNode->FindPin(URigVMRerouteNode::ValueName);
			ApplyPinState(ReroutePin, GetPinState(LibraryPinToReroute));
			ReroutedOutputPins.Add(LibraryPinToReroute->GetName(), ReroutePin);
			ExpandedNodes.Add(RerouteNode);
		}
	}

	// g) remap all output / source pins and create a final list of links to create
	TMap<FString, FString> RemappedSourcePinsForInputs;
	TMap<FString, FString> RemappedSourcePinsForOutputs;
	TArray<URigVMPin*> LibraryPins = InNode->GetAllPinsRecursively();
	for (URigVMPin* LibraryPin : LibraryPins)
	{
		FString LibraryPinPath = LibraryPin->GetPinPath();
		FString LibraryNodeName;
		URigVMPin::SplitPinPathAtStart(LibraryPinPath, LibraryNodeName, LibraryPinPath);


		struct Local
		{
			static void UpdateRemappedSourcePins(FString SourcePinPath, FString TargetPinPath, TMap<FString, FString>& RemappedSourcePins)
			{
				while (!SourcePinPath.IsEmpty() && !TargetPinPath.IsEmpty())
				{
					RemappedSourcePins.FindOrAdd(SourcePinPath) = TargetPinPath;

					FString SourceLastSegment, TargetLastSegment;
					if (!URigVMPin::SplitPinPathAtEnd(SourcePinPath, SourcePinPath, SourceLastSegment))
					{
						break;
					}
					if (!URigVMPin::SplitPinPathAtEnd(TargetPinPath, TargetPinPath, TargetLastSegment))
					{
						break;
					}
				}
			}
		};

		if (LibraryPin->GetDirection() == ERigVMPinDirection::Input ||
			LibraryPin->GetDirection() == ERigVMPinDirection::IO)
		{
			if (const TArray<FString>* LibraryPinLinksPtr = ToLibraryNode.Find(LibraryPinPath))
			{
				const TArray<FString>& LibraryPinLinks = *LibraryPinLinksPtr;
				ensure(LibraryPinLinks.Num() == 1);

				Local::UpdateRemappedSourcePins(LibraryPinPath, LibraryPinLinks[0], RemappedSourcePinsForInputs);
			}
		}
		if (LibraryPin->GetDirection() == ERigVMPinDirection::Output ||
			LibraryPin->GetDirection() == ERigVMPinDirection::IO)
		{
			if (const TArray<FString>* LibraryPinLinksPtr = ToReturnNode.Find(LibraryPinPath))
			{
				const TArray<FString>& LibraryPinLinks = *LibraryPinLinksPtr;
				ensure(LibraryPinLinks.Num() == 1);

				Local::UpdateRemappedSourcePins(LibraryPinPath, LibraryPinLinks[0], RemappedSourcePinsForOutputs);
			}
		}
	}

	// h) re-establish all of the links going to the left of the library node
	//    in this pass we only care about pins which have reroutes
	for (const TPair<FString, TArray<FString>>& ToLibraryNodePair : ToLibraryNode)
	{
		FString LibraryNodePinName, LibraryNodePinPathSuffix;
		if (!URigVMPin::SplitPinPathAtStart(ToLibraryNodePair.Key, LibraryNodePinName, LibraryNodePinPathSuffix))
		{
			LibraryNodePinName = ToLibraryNodePair.Key;
		}

		if (!ReroutedInputPins.Contains(LibraryNodePinName))
		{
			continue;
		}

		URigVMPin* ReroutedPin = ReroutedInputPins.FindChecked(LibraryNodePinName);
		URigVMPin* TargetPin = LibraryNodePinPathSuffix.IsEmpty() ? ReroutedPin : ReroutedPin->FindSubPin(LibraryNodePinPathSuffix);
		check(TargetPin);

		for (const FString& SourcePinPath : ToLibraryNodePair.Value)
		{
			URigVMPin* SourcePin = GetGraph()->FindPin(*SourcePinPath);
			if (SourcePin && TargetPin)
			{
				if (!SourcePin->IsLinkedTo(TargetPin))
				{
					AddLink(SourcePin, TargetPin, false);
				}
			}
		}
	}

	// i) re-establish all of the links going to the left of the library node (based on the entry node)
	for (const TPair<FString, TArray<FString>>& FromEntryNodePair : FromEntryNode)
	{
		FString EntryPinPath = FromEntryNodePair.Key;
		FString EntryPinPathSuffix;

		const FString* RemappedSourcePin = RemappedSourcePinsForInputs.Find(EntryPinPath);
		while (RemappedSourcePin == nullptr)
		{
			FString LastSegment;
			if (!URigVMPin::SplitPinPathAtEnd(EntryPinPath, EntryPinPath, LastSegment))
			{
				break;
			}

			if (EntryPinPathSuffix.IsEmpty())
			{
				EntryPinPathSuffix = LastSegment;
			}
			else
			{
				EntryPinPathSuffix = URigVMPin::JoinPinPath(LastSegment, EntryPinPathSuffix);
			}

			RemappedSourcePin = RemappedSourcePinsForInputs.Find(EntryPinPath);
		}

		if (RemappedSourcePin == nullptr)
		{
			continue;
		}

		FString RemappedSourcePinPath = *RemappedSourcePin;
		if (!EntryPinPathSuffix.IsEmpty())
		{
			RemappedSourcePinPath = URigVMPin::JoinPinPath(RemappedSourcePinPath, EntryPinPathSuffix);
		}

		// remap the top level pin in case we need to insert a reroute
		FString EntryPinName;
		if (!URigVMPin::SplitPinPathAtStart(FromEntryNodePair.Key, EntryPinPath, EntryPinPathSuffix))
		{
			EntryPinName = FromEntryNodePair.Key;
			EntryPinPathSuffix.Reset();
		}
		if (ReroutedInputPins.Contains(EntryPinName))
		{
			URigVMPin* ReroutedPin = ReroutedInputPins.FindChecked(EntryPinName);
			URigVMPin* TargetPin = EntryPinPathSuffix.IsEmpty() ? ReroutedPin : ReroutedPin->FindSubPin(EntryPinPathSuffix);
			check(TargetPin);
			RemappedSourcePinPath = TargetPin->GetPinPath();
		}

		for (const FString& FromEntryNodeTargetPinPath : FromEntryNodePair.Value)
		{
			URigVMPin* SourcePin = GetGraph()->FindPin(*RemappedSourcePinPath);
			URigVMPin* TargetPin = GetGraph()->FindPin(FromEntryNodeTargetPinPath);
			if (SourcePin && TargetPin)
			{
				if (!SourcePin->IsLinkedTo(TargetPin))
				{
					AddLink(SourcePin, TargetPin, false);
				}
			}
		}
	}

	// j) re-establish all of the links going from the right of the library node
	//    in this pass we only check pins which have a reroute
	for (const TPair<FString, TArray<FString>>& ToReturnNodePair : ToReturnNode)
	{
		FString LibraryNodePinName, LibraryNodePinPathSuffix;
		if (!URigVMPin::SplitPinPathAtStart(ToReturnNodePair.Key, LibraryNodePinName, LibraryNodePinPathSuffix))
		{
			LibraryNodePinName = ToReturnNodePair.Key;
		}

		if (!ReroutedOutputPins.Contains(LibraryNodePinName))
		{
			continue;
		}

		URigVMPin* ReroutedPin = ReroutedOutputPins.FindChecked(LibraryNodePinName);
		URigVMPin* TargetPin = LibraryNodePinPathSuffix.IsEmpty() ? ReroutedPin : ReroutedPin->FindSubPin(LibraryNodePinPathSuffix);
		check(TargetPin);

		for (const FString& SourcePinpath : ToReturnNodePair.Value)
		{
			URigVMPin* SourcePin = GetGraph()->FindPin(*SourcePinpath);
			if (SourcePin && TargetPin)
			{
				if (!SourcePin->IsLinkedTo(TargetPin))
				{
					AddLink(SourcePin, TargetPin, false);
				}
			}
		}
	}

	// k) re-establish all of the links going from the right of the library node
	for (const TPair<FString, TArray<FString>>& FromLibraryNodePair : FromLibraryNode)
	{
		FString FromLibraryNodePinPath = FromLibraryNodePair.Key;
		FString FromLibraryNodePinPathSuffix;

		const FString* RemappedSourcePin = RemappedSourcePinsForOutputs.Find(FromLibraryNodePinPath);
		while (RemappedSourcePin == nullptr)
		{
			FString LastSegment;
			if (!URigVMPin::SplitPinPathAtEnd(FromLibraryNodePinPath, FromLibraryNodePinPath, LastSegment))
			{
				break;
			}

			if (FromLibraryNodePinPathSuffix.IsEmpty())
			{
				FromLibraryNodePinPathSuffix = LastSegment;
			}
			else
			{
				FromLibraryNodePinPathSuffix = URigVMPin::JoinPinPath(LastSegment, FromLibraryNodePinPathSuffix);
			}

			RemappedSourcePin = RemappedSourcePinsForOutputs.Find(FromLibraryNodePinPath);
		}

		if (RemappedSourcePin == nullptr)
		{
			continue;
		}

		FString RemappedSourcePinPath = *RemappedSourcePin;
		if (!FromLibraryNodePinPathSuffix.IsEmpty())
		{
			RemappedSourcePinPath = URigVMPin::JoinPinPath(RemappedSourcePinPath, FromLibraryNodePinPathSuffix);
		}

		// remap the top level pin in case we need to insert a reroute
		FString ReturnPinName, ReturnPinPathSuffix;
		if (!URigVMPin::SplitPinPathAtStart(FromLibraryNodePair.Key, ReturnPinName, ReturnPinPathSuffix))
		{
			ReturnPinName = FromLibraryNodePair.Key;
			ReturnPinPathSuffix.Reset();
		}
		if (ReroutedOutputPins.Contains(ReturnPinName))
		{
			URigVMPin* ReroutedPin = ReroutedOutputPins.FindChecked(ReturnPinName);
			URigVMPin* SourcePin = ReturnPinPathSuffix.IsEmpty() ? ReroutedPin : ReroutedPin->FindSubPin(ReturnPinPathSuffix);
			check(SourcePin);
			RemappedSourcePinPath = SourcePin->GetPinPath();
		}

		for (const FString& FromLibraryNodeTargetPinPath : FromLibraryNodePair.Value)
		{
			URigVMPin* SourcePin = GetGraph()->FindPin(*RemappedSourcePinPath);
			URigVMPin* TargetPin = GetGraph()->FindPin(FromLibraryNodeTargetPinPath);
			if (SourcePin && TargetPin)
			{
				if (!SourcePin->IsLinkedTo(TargetPin))
				{
					AddLink(SourcePin, TargetPin, false);
				}
			}
		}
	}

	// l) remove the library node from the graph
	if (bSetupUndoRedo)
	{
		ExpandAction.LibraryNodePath = InNode->GetName();
	}
	RemoveNode(InNode, false, true);

	if (bSetupUndoRedo)
	{
		for (URigVMNode* ExpandedNode : ExpandedNodes)
		{
			ExpandAction.ExpandedNodePaths.Add(ExpandedNode->GetName());
		}
		ActionStack->EndAction(ExpandAction);
	}

	return ExpandedNodes;
}

FName URigVMController::PromoteCollapseNodeToFunctionReferenceNode(const FName& InNodeName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return NAME_None;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMNode* Result = PromoteCollapseNodeToFunctionReferenceNode(Cast<URigVMCollapseNode>(Graph->FindNodeByName(InNodeName)), bSetupUndoRedo);
	if (Result)
	{
		return Result->GetFName();
	}
	return NAME_None;
}

FName URigVMController::PromoteFunctionReferenceNodeToCollapseNode(const FName& InNodeName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return NAME_None;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMNode* Result = PromoteFunctionReferenceNodeToCollapseNode(Cast<URigVMFunctionReferenceNode>(Graph->FindNodeByName(InNodeName)), bSetupUndoRedo);
	if (Result)
	{
		return Result->GetFName();
	}
	return NAME_None;
}

URigVMFunctionReferenceNode* URigVMController::PromoteCollapseNodeToFunctionReferenceNode(URigVMCollapseNode* InCollapseNode, bool bSetupUndoRedo)
{
	if (!IsValidNodeForGraph(InCollapseNode))
	{
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMFunctionLibrary* FunctionLibrary = Graph->GetDefaultFunctionLibrary();
	if (FunctionLibrary == nullptr)
	{
		return nullptr;
	}

	if (bSetupUndoRedo)
	{
		OpenUndoBracket(TEXT("Promote to Function"));
	}

	URigVMFunctionReferenceNode* FunctionRefNode = nullptr;
	URigVMCollapseNode* FunctionDefinition = nullptr;
	{
		FRigVMControllerGraphGuard GraphGuard(this, FunctionLibrary, bSetupUndoRedo);
		FString FunctionName = GetValidNodeName(InCollapseNode->GetName());
		FunctionDefinition = DuplicateObject<URigVMCollapseNode>(InCollapseNode, FunctionLibrary, *FunctionName);

		if (FunctionDefinition)
		{
			FunctionLibrary->Nodes.Add(FunctionDefinition);
			Notify(ERigVMGraphNotifType::NodeAdded, FunctionDefinition);
		}
	}

	if (FunctionDefinition)
	{
		FString NodeName = InCollapseNode->GetName();
		FVector2D NodePosition = InCollapseNode->GetPosition();
		TMap<FString, FPinState> PinStates = GetPinStates(InCollapseNode);

		TArray<URigVMLink*> Links = InCollapseNode->GetLinks();
		TArray< TPair< FString, FString > > LinkPaths;
		for (URigVMLink* Link : Links)
		{
			LinkPaths.Add(TPair< FString, FString >(Link->GetSourcePin()->GetPinPath(), Link->GetTargetPin()->GetPinPath()));
		}

		RemoveNode(InCollapseNode, bSetupUndoRedo);

		FunctionRefNode = AddFunctionReferenceNode(FunctionDefinition, NodePosition, NodeName, bSetupUndoRedo);

		if (FunctionRefNode)
		{
			ApplyPinStates(FunctionRefNode, PinStates);
			for (const TPair<FString, FString>& LinkPath : LinkPaths)
			{
				AddLink(LinkPath.Key, LinkPath.Value, bSetupUndoRedo);
			}
		}
	}

	if (bSetupUndoRedo)
	{
		CloseUndoBracket();
	}

	return FunctionRefNode;
}

URigVMCollapseNode* URigVMController::PromoteFunctionReferenceNodeToCollapseNode(URigVMFunctionReferenceNode* InFunctionRefNode, bool bSetupUndoRedo)
{
	if (!IsValidNodeForGraph(InFunctionRefNode))
	{
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMCollapseNode* FunctionDefinition = Cast<URigVMCollapseNode>(InFunctionRefNode->GetReferencedNode());
	if (FunctionDefinition == nullptr)
	{
		return nullptr;
	}

	if (bSetupUndoRedo)
	{
		OpenUndoBracket(TEXT("Promote to Collapse Node"));
	}

	FString NodeName = InFunctionRefNode->GetName();
	FVector2D NodePosition = InFunctionRefNode->GetPosition();
	TMap<FString, FPinState> PinStates = GetPinStates(InFunctionRefNode);

	TArray<URigVMLink*> Links = InFunctionRefNode->GetLinks();
	TArray< TPair< FString, FString > > LinkPaths;
	for (URigVMLink* Link : Links)
	{
		LinkPaths.Add(TPair< FString, FString >(Link->GetSourcePin()->GetPinPath(), Link->GetTargetPin()->GetPinPath()));
	}

	RemoveNode(InFunctionRefNode, bSetupUndoRedo);

	URigVMCollapseNode* CollapseNode = DuplicateObject<URigVMCollapseNode>(FunctionDefinition, Graph, *NodeName);
	if(CollapseNode)
	{
		CollapseNode->Position = NodePosition;
		Graph->Nodes.Add(CollapseNode);
		Notify(ERigVMGraphNotifType::NodeAdded, CollapseNode);

		ApplyPinStates(CollapseNode, PinStates);
		for (const TPair<FString, FString>& LinkPath : LinkPaths)
		{
			AddLink(LinkPath.Key, LinkPath.Value, bSetupUndoRedo);
		}
	}

	if (bSetupUndoRedo)
	{
		CloseUndoBracket();
	}

	return CollapseNode;
}

void URigVMController::RefreshFunctionPins(URigVMNode* InNode, bool bNotify)
{
	if (InNode == nullptr)
	{
		return;
	}

	URigVMFunctionEntryNode* EntryNode = Cast<URigVMFunctionEntryNode>(InNode);
	URigVMFunctionReturnNode* ReturnNode = Cast<URigVMFunctionReturnNode>(InNode);

	if (EntryNode || ReturnNode)
	{
		TArray<URigVMLink*> Links = InNode->GetLinks();
		DetachLinksFromPinObjects(&Links, bNotify);
		RepopulatePinsOnNode(InNode, false, bNotify);
		ReattachLinksToPinObjects(false, &Links, bNotify);
	}
}

bool URigVMController::RemoveNode(URigVMNode* InNode, bool bSetupUndoRedo, bool bRecursive)
{
	if (!IsValidNodeForGraph(InNode))
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (bSetupUndoRedo)
	{
		// don't allow deletion of function entry / return nodes
		if (Cast<URigVMFunctionEntryNode>(InNode) != nullptr ||
			Cast<URigVMFunctionReturnNode>(InNode) != nullptr)
		{
			return false;
		}
	}

	TGuardValue<bool> GuardCompactness(bIgnoreRerouteCompactnessChanges, true);

	FRigVMBaseAction Action;
	if (bSetupUndoRedo)
	{
		Action = FRigVMBaseAction();
		Action.Title = FString::Printf(TEXT("Remove %s Node"), *InNode->GetNodeTitle());
		ActionStack->BeginAction(Action);
	}

	if (URigVMInjectionInfo* InjectionInfo = InNode->GetInjectionInfo())
	{
		URigVMPin* Pin = InjectionInfo->GetPin();
		check(Pin);

		Pin->InjectionInfos.Remove(InjectionInfo);

		if (InjectionInfo->bInjectedAsInput)
		{
			URigVMPin* LastInputPin = Pin;
			RewireLinks(InjectionInfo->InputPin, LastInputPin, true, false);
		}
		else
		{
			URigVMPin* LastOutputPin = Pin;
			RewireLinks(InjectionInfo->OutputPin, LastOutputPin, false, false);
		}
	}

	if (bSetupUndoRedo || bRecursive)
	{
		SelectNode(InNode, false, bSetupUndoRedo);

		for (URigVMPin* Pin : InNode->GetPins())
		{
			TArray<URigVMInjectionInfo*> InjectedNodes = Pin->GetInjectedNodes();
			for (URigVMInjectionInfo* InjectedNode : InjectedNodes)
			{
				RemoveNode(InjectedNode->UnitNode, bSetupUndoRedo);
			}

			BreakAllLinks(Pin, true, bSetupUndoRedo);
			BreakAllLinks(Pin, false, bSetupUndoRedo);
			BreakAllLinksRecursive(Pin, true, false, bSetupUndoRedo);
			BreakAllLinksRecursive(Pin, false, false, bSetupUndoRedo);
		}

		if (bSetupUndoRedo)
		{
			ActionStack->AddAction(FRigVMRemoveNodeAction(InNode, this));
		}

		if (URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(InNode))
		{
			URigVMGraph* SubGraph = CollapseNode->GetContainedGraph();
			FRigVMControllerGraphGuard GraphGuard(this, SubGraph, false);

			TArray<URigVMNode*> ContainedNodes = SubGraph->GetNodes();
			for (URigVMNode* ContainedNode : ContainedNodes)
			{
				if(Cast<URigVMFunctionEntryNode>(ContainedNode) != nullptr ||
					Cast<URigVMFunctionReturnNode>(ContainedNode) != nullptr)
				{
					continue;
				}
				RemoveNode(ContainedNode, false, true);
			}
		}
	}

	Graph->Nodes.Remove(InNode);
	if (!bSuspendNotifications)
	{
		Graph->MarkPackageDirty();
	}

	Notify(ERigVMGraphNotifType::NodeRemoved, InNode);

	if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(InNode))
	{
		if(URigVMFunctionReferenceNode* FunctionReferenceNode = Cast<URigVMFunctionReferenceNode>(LibraryNode))
		{
			if (URigVMFunctionLibrary* FunctionLibrary = Cast<URigVMFunctionLibrary>(FunctionReferenceNode->GetLibrary()))
			{
				FRigVMFunctionReferenceArray* References = FunctionLibrary->FunctionReferences.Find(FunctionReferenceNode->GetReferencedNode());
				if (References)
				{
					References->FunctionReferences.RemoveAll(
						[FunctionReferenceNode](TSoftObjectPtr<URigVMFunctionReferenceNode>& FunctionReferencePtr) {

							/*
							if (!FunctionReferencePtr.IsValid())
							{
								FunctionReferencePtr.LoadSynchronous();
							}
							*/

							if (!FunctionReferencePtr.IsValid())
							{
								return true;
							}
							return FunctionReferencePtr.Get() == FunctionReferenceNode;
					});
				}
			}
		}
		else if (URigVMFunctionLibrary* FunctionLibrary = Cast<URigVMFunctionLibrary>(LibraryNode->GetGraph()))
		{
			const FRigVMFunctionReferenceArray* FunctionReferencesPtr = FunctionLibrary->FunctionReferences.Find(LibraryNode);
			if (FunctionReferencesPtr)
			{
				for (const TSoftObjectPtr<URigVMFunctionReferenceNode>& FunctionReferencePtr : FunctionReferencesPtr->FunctionReferences)
				{
					if (FunctionReferencePtr.IsValid())
					{
						{
							TGuardValue<TSoftObjectPtr<URigVMLibraryNode>> ClearReferencedNodePtr(
								FunctionReferencePtr->ReferencedNodePtr,
								TSoftObjectPtr<URigVMLibraryNode>());

							FRigVMControllerGraphGuard GraphGuard(this, FunctionReferencePtr->GetGraph(), false);
							RepopulatePinsOnNode(FunctionReferencePtr.Get(), false, true);
						}
						FunctionReferencePtr->ReferencedNodePtr.ResetWeakPtr();
					}
				}
			}
			FunctionLibrary->FunctionReferences.Remove(LibraryNode);
		}
	}

	if (URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(InNode))
	{
		Notify(ERigVMGraphNotifType::VariableRemoved, VariableNode);
	}
	if (URigVMParameterNode* ParameterNode = Cast<URigVMParameterNode>(InNode))
	{
		Notify(ERigVMGraphNotifType::ParameterRemoved, ParameterNode);
	}

	if (URigVMInjectionInfo* InjectionInfo = InNode->GetInjectionInfo())
	{
		DestroyObject(InjectionInfo);
	}

	DestroyObject(InNode);

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action);
	}

	return true;
}

bool URigVMController::RemoveNodeByName(const FName& InNodeName, bool bSetupUndoRedo, bool bRecursive)
{
	if (!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	return RemoveNode(Graph->FindNodeByName(InNodeName), bSetupUndoRedo, bRecursive);
}

bool URigVMController::RenameNode(URigVMNode* InNode, const FName& InNewName, bool bSetupUndoRedo)
{
	if (!IsValidNodeForGraph(InNode))
	{
		return false;
	}

	FName ValidNewName = *GetValidNodeName(InNewName.ToString());
	if (InNode->GetFName() == ValidNewName)
	{
		return false;
	}

	FRigVMRenameNodeAction Action;
	if (bSetupUndoRedo)
	{
		Action = FRigVMRenameNodeAction(InNode->GetFName(), ValidNewName);
		ActionStack->BeginAction(Action);
	}

	// loop over all links and remove them
	TArray<URigVMLink*> Links = InNode->GetLinks();
	for (URigVMLink* Link : Links)
	{
		Link->PrepareForCopy();
		Notify(ERigVMGraphNotifType::LinkRemoved, Link);
	}

	InNode->PreviousName = InNode->GetFName();
	if (!InNode->Rename(*ValidNewName.ToString()))
	{
		ActionStack->CancelAction(Action);
		return false;
	}

	Notify(ERigVMGraphNotifType::NodeRenamed, InNode);

	// update the links once more
	for (URigVMLink* Link : Links)
	{
		Link->PrepareForCopy();
		Notify(ERigVMGraphNotifType::LinkAdded, Link);
	}

	if(URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(InNode))
	{
		if (URigVMFunctionLibrary* FunctionLibrary = Cast<URigVMFunctionLibrary>(LibraryNode->GetGraph()))
		{
			FRigVMFunctionReferenceArray* ReferencesEntry = FunctionLibrary->FunctionReferences.Find(LibraryNode);
			if (ReferencesEntry)
			{
				for (TSoftObjectPtr<URigVMFunctionReferenceNode> FunctionReferencePtr : ReferencesEntry->FunctionReferences)
				{
					// only update valid, living references
					if (FunctionReferencePtr.IsValid())
					{
						FRigVMControllerGraphGuard GraphGuard(this, FunctionReferencePtr->GetGraph(), false);
						RenameNode(FunctionReferencePtr.Get(), InNewName, false);
					}
				}
			}
		}
	}

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action);
	}

	return true;
}

bool URigVMController::SelectNode(URigVMNode* InNode, bool bSelect, bool bSetupUndoRedo)
{
	if (!IsValidNodeForGraph(InNode))
	{
		return false;
	}

	if (InNode->IsSelected() == bSelect)
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	TArray<FName> NewSelection = Graph->GetSelectNodes();
	if (bSelect)
	{
		NewSelection.AddUnique(InNode->GetFName());
	}
	else
	{
		NewSelection.Remove(InNode->GetFName());
	}

	return SetNodeSelection(NewSelection, bSetupUndoRedo);
}

bool URigVMController::SelectNodeByName(const FName& InNodeName, bool bSelect, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	return SelectNode(Graph->FindNodeByName(InNodeName), bSelect, bSetupUndoRedo);
}

bool URigVMController::ClearNodeSelection(bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return false;
	}

	return SetNodeSelection(TArray<FName>(), bSetupUndoRedo);
}

bool URigVMController::SetNodeSelection(const TArray<FName>& InNodeNames, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	FRigVMSetNodeSelectionAction Action;
	if (bSetupUndoRedo)
	{
		Action = FRigVMSetNodeSelectionAction(Graph, InNodeNames);
		ActionStack->BeginAction(Action);
	}

	bool bSelectedSomething = false;

	TArray<FName> PreviousSelection = Graph->GetSelectNodes();
	for (const FName& PreviouslySelectedNode : PreviousSelection)
	{
		if (!InNodeNames.Contains(PreviouslySelectedNode))
		{
			if(Graph->SelectedNodes.Remove(PreviouslySelectedNode) > 0)
			{
				Notify(ERigVMGraphNotifType::NodeDeselected, Graph->FindNodeByName(PreviouslySelectedNode));
				bSelectedSomething = true;
			}
		}
	}

	for (const FName& InNodeName : InNodeNames)
	{
		if (URigVMNode* NodeToSelect = Graph->FindNodeByName(InNodeName))
		{
			int32 PreviousNum = Graph->SelectedNodes.Num();
			Graph->SelectedNodes.AddUnique(InNodeName);
			if (PreviousNum != Graph->SelectedNodes.Num())
			{
				Notify(ERigVMGraphNotifType::NodeSelected, NodeToSelect);
				bSelectedSomething = true;
			}
		}
	}

	if (bSetupUndoRedo)
	{
		if (bSelectedSomething)
		{
			const TArray<FName>& SelectedNodes = Graph->GetSelectNodes();
			if (SelectedNodes.Num() == 0)
			{
				Action.Title = TEXT("Deselect all nodes.");
			}
			else
			{
				if (SelectedNodes.Num() == 1)
				{
					Action.Title = FString::Printf(TEXT("Selected node '%s'."), *SelectedNodes[0].ToString());
				}
				else
				{
					Action.Title = TEXT("Selected multiple nodes.");
				}
			}
			ActionStack->EndAction(Action);
		}
		else
		{
			ActionStack->CancelAction(Action);
		}
	}

	if (bSelectedSomething)
	{
		Notify(ERigVMGraphNotifType::NodeSelectionChanged, nullptr);
	}

	return bSelectedSomething;
}

bool URigVMController::SetNodePosition(URigVMNode* InNode, const FVector2D& InPosition, bool bSetupUndoRedo, bool bMergeUndoAction)
{
	if (!IsValidNodeForGraph(InNode))
	{
		return false;
	}

	if ((InNode->Position - InPosition).IsNearlyZero())
	{
		return false;
	}

	FRigVMSetNodePositionAction Action;
	if (bSetupUndoRedo)
	{
		Action = FRigVMSetNodePositionAction(InNode, InPosition);
		Action.Title = FString::Printf(TEXT("Set Node Position"));
		ActionStack->BeginAction(Action);
	}

	InNode->Position = InPosition;
	Notify(ERigVMGraphNotifType::NodePositionChanged, InNode);

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action, bMergeUndoAction);
	}

	return true;
}

bool URigVMController::SetNodePositionByName(const FName& InNodeName, const FVector2D& InPosition, bool bSetupUndoRedo, bool bMergeUndoAction)
{
	if(!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMNode* Node = Graph->FindNodeByName(InNodeName);
	return SetNodePosition(Node, InPosition, bSetupUndoRedo, bMergeUndoAction);
}

bool URigVMController::SetNodeSize(URigVMNode* InNode, const FVector2D& InSize, bool bSetupUndoRedo, bool bMergeUndoAction)
{
	if (!IsValidNodeForGraph(InNode))
	{
		return false;
	}

	if ((InNode->Size - InSize).IsNearlyZero())
	{
		return false;
	}

	FRigVMSetNodeSizeAction Action;
	if (bSetupUndoRedo)
	{
		Action = FRigVMSetNodeSizeAction(InNode, InSize);
		Action.Title = FString::Printf(TEXT("Set Node Size"));
		ActionStack->BeginAction(Action);
	}

	InNode->Size = InSize;
	Notify(ERigVMGraphNotifType::NodeSizeChanged, InNode);

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action, bMergeUndoAction);
	}

	return true;
}

bool URigVMController::SetNodeSizeByName(const FName& InNodeName, const FVector2D& InSize, bool bSetupUndoRedo, bool bMergeUndoAction)
{
	if(!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMNode* Node = Graph->FindNodeByName(InNodeName);
	return SetNodeSize(Node, InSize, bSetupUndoRedo, bMergeUndoAction);
}

bool URigVMController::SetNodeColor(URigVMNode* InNode, const FLinearColor& InColor, bool bSetupUndoRedo, bool bMergeUndoAction)
{
	if (!IsValidNodeForGraph(InNode))
	{
		return false;
	}

	if ((FVector4(InNode->NodeColor) - FVector4(InColor)).IsNearlyZero3())
	{
		return false;
	}

	FRigVMSetNodeColorAction Action;
	if (bSetupUndoRedo)
	{
		Action = FRigVMSetNodeColorAction(InNode, InColor);
		Action.Title = FString::Printf(TEXT("Set Node Color"));
		ActionStack->BeginAction(Action);
	}

	InNode->NodeColor = InColor;
	Notify(ERigVMGraphNotifType::NodeColorChanged, InNode);

	if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(InNode))
	{
		if (URigVMFunctionLibrary* FunctionLibrary = Cast<URigVMFunctionLibrary>(LibraryNode->GetGraph()))
		{
			FRigVMFunctionReferenceArray* ReferencesEntry = FunctionLibrary->FunctionReferences.Find(LibraryNode);
			if (ReferencesEntry)
			{
				for (TSoftObjectPtr<URigVMFunctionReferenceNode> FunctionReferencePtr : ReferencesEntry->FunctionReferences)
				{
					if (FunctionReferencePtr.IsValid())
					{
						URigVMNode* ReferenceNode = FunctionReferencePtr.Get();
						FRigVMControllerGraphGuard GraphGuard(this, ReferenceNode->GetGraph(), false);
						Notify(ERigVMGraphNotifType::NodeColorChanged, ReferenceNode);
					}
				}
			}
		}
	}

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action, bMergeUndoAction);
	}

	return true;
}

bool URigVMController::SetNodeColorByName(const FName& InNodeName, const FLinearColor& InColor, bool bSetupUndoRedo, bool bMergeUndoAction)
{
	if(!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMNode* Node = Graph->FindNodeByName(InNodeName);
	return SetNodeColor(Node, InColor, bSetupUndoRedo, bMergeUndoAction);
}

bool URigVMController::SetNodeCategory(URigVMCollapseNode* InNode, const FString& InCategory, bool bSetupUndoRedo, bool bMergeUndoAction)
{
	if (!IsValidNodeForGraph(InNode))
	{
		return false;
	}

	if (InNode->GetNodeCategory() == InCategory)
	{
		return false;
	}

	FRigVMSetNodeCategoryAction Action;
	if (bSetupUndoRedo)
	{
		Action = FRigVMSetNodeCategoryAction(InNode, InCategory);
		Action.Title = FString::Printf(TEXT("Set Node Category"));
		ActionStack->BeginAction(Action);
	}

	InNode->NodeCategory = InCategory;
	Notify(ERigVMGraphNotifType::NodeCategoryChanged, InNode);

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action, bMergeUndoAction);
	}

	return true;
}

bool URigVMController::SetNodeCategoryByName(const FName& InNodeName, const FString& InCategory, bool bSetupUndoRedo, bool bMergeUndoAction)
{
	if (!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMCollapseNode* Node = Cast<URigVMCollapseNode>(Graph->FindNodeByName(InNodeName));
	return SetNodeCategory(Node, InCategory, bSetupUndoRedo, bMergeUndoAction);
}

bool URigVMController::SetNodeKeywords(URigVMCollapseNode* InNode, const FString& InKeywords, bool bSetupUndoRedo, bool bMergeUndoAction)
{
	if (!IsValidNodeForGraph(InNode))
	{
		return false;
	}

	if (InNode->GetNodeKeywords() == InKeywords)
	{
		return false;
	}

	FRigVMSetNodeKeywordsAction Action;
	if (bSetupUndoRedo)
	{
		Action = FRigVMSetNodeKeywordsAction(InNode, InKeywords);
		Action.Title = FString::Printf(TEXT("Set Node Keywords"));
		ActionStack->BeginAction(Action);
	}

	InNode->NodeKeywords = InKeywords;
	Notify(ERigVMGraphNotifType::NodeKeywordsChanged, InNode);

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action, bMergeUndoAction);
	}

	return true;
}

bool URigVMController::SetNodeKeywordsByName(const FName& InNodeName, const FString& InKeywords, bool bSetupUndoRedo, bool bMergeUndoAction)
{
	if (!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMCollapseNode* Node = Cast<URigVMCollapseNode>(Graph->FindNodeByName(InNodeName));
	return SetNodeKeywords(Node, InKeywords, bSetupUndoRedo, bMergeUndoAction);
}

bool URigVMController::SetCommentText(URigVMNode* InNode, const FString& InCommentText, bool bSetupUndoRedo)
{
	if (!IsValidNodeForGraph(InNode))
	{
		return false;
	}

	if (URigVMCommentNode* CommentNode = Cast<URigVMCommentNode>(InNode))
	{
		if(CommentNode->CommentText == InCommentText)
		{
			return false;
		}

		FRigVMSetCommentTextAction Action;
		if (bSetupUndoRedo)
		{
			Action = FRigVMSetCommentTextAction(CommentNode, InCommentText);
			Action.Title = FString::Printf(TEXT("Set Comment Text"));
			ActionStack->BeginAction(Action);
		}

		CommentNode->CommentText = InCommentText;
		Notify(ERigVMGraphNotifType::CommentTextChanged, InNode);

		if (bSetupUndoRedo)
		{
			ActionStack->EndAction(Action);
		}

		return true;
	}

	return false;
}

bool URigVMController::SetCommentTextByName(const FName& InNodeName, const FString& InCommentText, bool bSetupUndoRedo)
{
	if(!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMNode* Node = Graph->FindNodeByName(InNodeName);
	return SetCommentText(Node, InCommentText, bSetupUndoRedo);
}

bool URigVMController::SetRerouteCompactness(URigVMNode* InNode, bool bShowAsFullNode, bool bSetupUndoRedo)
{
	if (!IsValidNodeForGraph(InNode))
	{
		return false;
	}

	if (URigVMRerouteNode* RerouteNode = Cast<URigVMRerouteNode>(InNode))
	{
		if (RerouteNode->bShowAsFullNode == bShowAsFullNode)
		{
			return false;
		}

		FRigVMSetRerouteCompactnessAction Action;
		if (bSetupUndoRedo)
		{
			Action = FRigVMSetRerouteCompactnessAction(RerouteNode, bShowAsFullNode);
			Action.Title = FString::Printf(TEXT("Set Reroute Size"));
			ActionStack->BeginAction(Action);
		}

		RerouteNode->bShowAsFullNode = bShowAsFullNode;
		Notify(ERigVMGraphNotifType::RerouteCompactnessChanged, InNode);

		if (bSetupUndoRedo)
		{
			ActionStack->EndAction(Action);
		}

		return true;
	}

	return false;
}

bool URigVMController::SetRerouteCompactnessByName(const FName& InNodeName, bool bShowAsFullNode, bool bSetupUndoRedo)
{
	if(!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMNode* Node = Graph->FindNodeByName(InNodeName);
	return SetRerouteCompactness(Node, bShowAsFullNode, bSetupUndoRedo);
}

bool URigVMController::RenameVariable(const FName& InOldName, const FName& InNewName, bool bSetupUndoRedo)
{
	if(!IsValidGraph())
	{
		return false;
	}

	if (InOldName == InNewName)
	{
		ReportWarning(TEXT("RenameVariable: InOldName and InNewName are equal."));
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	TArray<FRigVMGraphVariableDescription> ExistingVariables = Graph->GetVariableDescriptions();
	for (const FRigVMGraphVariableDescription& ExistingVariable : ExistingVariables)
	{
		if (ExistingVariable.Name == InNewName)
		{
			ReportErrorf(TEXT("Cannot rename variable to '%s' - variable already exists."), *InNewName.ToString());
			return false;
		}
	}

	FRigVMRenameVariableAction Action;
	if (bSetupUndoRedo)
	{
		Action = FRigVMRenameVariableAction(InOldName, InNewName);
		Action.Title = FString::Printf(TEXT("Rename Variable"));
		ActionStack->BeginAction(Action);
	}

	TArray<URigVMNode*> RenamedNodes;
	for (URigVMNode* Node : Graph->Nodes)
	{
		if(URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(Node))
		{
			if (VariableNode->GetVariableName() == InOldName)
			{
				VariableNode->FindPin(URigVMVariableNode::VariableName)->DefaultValue = InNewName.ToString();
				RenamedNodes.Add(Node);
			}
		}
	}

	for (URigVMNode* RenamedNode : RenamedNodes)
	{
		Notify(ERigVMGraphNotifType::VariableRenamed, RenamedNode);
		if (!bSuspendNotifications)
		{
			Graph->MarkPackageDirty();
		}
	}

	if (bSetupUndoRedo)
	{
		if (RenamedNodes.Num() > 0)
		{
			ActionStack->EndAction(Action);
		}
		else
		{
			ActionStack->CancelAction(Action);
		}
	}

	return RenamedNodes.Num() > 0;
}

bool URigVMController::RenameParameter(const FName& InOldName, const FName& InNewName, bool bSetupUndoRedo)
{
	if(!IsValidGraph())
	{
		return false;
	}

	if (InOldName == InNewName)
	{
		ReportWarning(TEXT("RenameParameter: InOldName and InNewName are equal."));
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	TArray<FRigVMGraphParameterDescription> ExistingParameters = Graph->GetParameterDescriptions();
	for (const FRigVMGraphParameterDescription& ExistingParameter : ExistingParameters)
	{
		if (ExistingParameter.Name == InNewName)
		{
			ReportErrorf(TEXT("Cannot rename parameter to '%s' - parameter already exists."), *InNewName.ToString());
			return false;
		}
	}

	FRigVMRenameParameterAction Action;
	if (bSetupUndoRedo)
	{
		Action = FRigVMRenameParameterAction(InOldName, InNewName);
		Action.Title = FString::Printf(TEXT("Rename Parameter"));
		ActionStack->BeginAction(Action);
	}

	TArray<URigVMNode*> RenamedNodes;
	for (URigVMNode* Node : Graph->Nodes)
	{
		if(URigVMParameterNode* ParameterNode = Cast<URigVMParameterNode>(Node))
		{
			if (ParameterNode->GetParameterName() == InOldName)
			{
				ParameterNode->FindPin(URigVMParameterNode::ParameterName)->DefaultValue = InNewName.ToString();
				RenamedNodes.Add(Node);
			}
		}
	}

	for (URigVMNode* RenamedNode : RenamedNodes)
	{
		Notify(ERigVMGraphNotifType::ParameterRenamed, RenamedNode);
		if (!bSuspendNotifications)
		{
			Graph->MarkPackageDirty();
		}
	}

	if (bSetupUndoRedo)
	{
		if (RenamedNodes.Num() > 0)
		{
			ActionStack->EndAction(Action);
		}
		else
		{
			ActionStack->CancelAction(Action);
		}
	}

	return RenamedNodes.Num() > 0;
}

void URigVMController::UpdateRerouteNodeAfterChangingLinks(URigVMPin* PinChanged, bool bSetupUndoRedo)
{
	if (bIgnoreRerouteCompactnessChanges)
	{
		return;
	}

	if (!IsValidGraph())
	{
		return;
	}

	URigVMRerouteNode* Node = Cast<URigVMRerouteNode>(PinChanged->GetNode());
	if (Node == nullptr)
	{
		return;
	}

	int32 NbTotalSources = Node->Pins[0]->GetSourceLinks(true /* recursive */).Num();
	int32 NbTotalTargets = Node->Pins[0]->GetTargetLinks(true /* recursive */).Num();
	int32 NbToplevelSources = Node->Pins[0]->GetSourceLinks(false /* recursive */).Num();
	int32 NbToplevelTargets = Node->Pins[0]->GetTargetLinks(false /* recursive */).Num();

	bool bJustTopLevelConnections = (NbTotalSources == NbToplevelSources) && (NbTotalTargets == NbToplevelTargets);
	bool bOnlyConnectionsOnOneSide = (NbTotalSources == 0) || (NbTotalTargets == 0);
	bool bShowAsFullNode = (!bJustTopLevelConnections) || bOnlyConnectionsOnOneSide;

	SetRerouteCompactness(Node, bShowAsFullNode, bSetupUndoRedo);
}

bool URigVMController::SetPinExpansion(const FString& InPinPath, bool bIsExpanded, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMPin* Pin = Graph->FindPin(InPinPath);
	if (Pin == nullptr)
	{
		ReportErrorf(TEXT("Cannot find pin '%s'."), *InPinPath);
		return false;
	}

	return SetPinExpansion(Pin, bIsExpanded, bSetupUndoRedo);
}

bool URigVMController::SetPinExpansion(URigVMPin* InPin, bool bIsExpanded, bool bSetupUndoRedo)
{
	if (InPin->GetSubPins().Num() == 0)
	{
		return false;
	}

	if (InPin->IsExpanded() == bIsExpanded)
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	FRigVMSetPinExpansionAction Action;
	if (bSetupUndoRedo)
	{
		Action = FRigVMSetPinExpansionAction(InPin, bIsExpanded);
		Action.Title = bIsExpanded ? TEXT("Expand Pin") : TEXT("Collapse Pin");
		ActionStack->BeginAction(Action);
	}

	InPin->bIsExpanded = bIsExpanded;

	Notify(ERigVMGraphNotifType::PinExpansionChanged, InPin);
	if (!bSuspendNotifications)
	{
		Graph->MarkPackageDirty();
	}

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action);
	}

	return true;
}

bool URigVMController::SetPinIsWatched(const FString& InPinPath, bool bIsWatched, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMPin* Pin = Graph->FindPin(InPinPath);
	if (Pin == nullptr)
	{
		ReportErrorf(TEXT("Cannot find pin '%s'."), *InPinPath);
		return false;
	}

	return SetPinIsWatched(Pin, bIsWatched, bSetupUndoRedo);
}

bool URigVMController::SetPinIsWatched(URigVMPin* InPin, bool bIsWatched, bool bSetupUndoRedo)
{
	if (!IsValidPinForGraph(InPin))
	{
		return false;
	}

	if (InPin->GetParentPin() != nullptr)
	{
		return false;
	}

	if (InPin->RequiresWatch() == bIsWatched)
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (Graph->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Cannot watch pins in function library graphs."));
		return false;
	}

	FRigVMSetPinWatchAction Action;
	if (bSetupUndoRedo)
	{
		Action = FRigVMSetPinWatchAction(InPin, bIsWatched);
		Action.Title = bIsWatched ? TEXT("Watch Pin") : TEXT("Unwatch Pin");
		ActionStack->BeginAction(Action);
	}

	InPin->bRequiresWatch = bIsWatched;

	Notify(ERigVMGraphNotifType::PinWatchedChanged, InPin);
	if (!bSuspendNotifications)
	{
		Graph->MarkPackageDirty();
	}

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action);
	}

	return true;
}

FString URigVMController::GetPinDefaultValue(const FString& InPinPath)
{
	if (!IsValidGraph())
	{
		return FString();
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMPin* Pin = Graph->FindPin(InPinPath);
	if (Pin == nullptr)
	{
		ReportErrorf(TEXT("Cannot find pin '%s'."), *InPinPath);
		return FString();
	}
	Pin = Pin->GetPinForLink();

	return Pin->GetDefaultValue();
}

bool URigVMController::SetPinDefaultValue(const FString& InPinPath, const FString& InDefaultValue, bool bResizeArrays, bool bSetupUndoRedo, bool bMergeUndoAction)
{
	if (!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMPin* Pin = Graph->FindPin(InPinPath);
	if (Pin == nullptr)
	{
		ReportErrorf(TEXT("Cannot find pin '%s'."), *InPinPath);
		return false;
	}

	if (URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(Pin->GetNode()))
	{
		if (Pin->GetName() == URigVMVariableNode::VariableName)
		{
			return SetVariableName(VariableNode, *InDefaultValue, bSetupUndoRedo);
		}
	}
	if (URigVMParameterNode* ParameterNode = Cast<URigVMParameterNode>(Pin->GetNode()))
	{
		if (Pin->GetName() == URigVMParameterNode::ParameterName)
		{
			return SetParameterName(ParameterNode, *InDefaultValue, bSetupUndoRedo);
		}
	}

	if (!SetPinDefaultValue(Pin, InDefaultValue, bResizeArrays, bSetupUndoRedo, bMergeUndoAction))
	{
		return false;
	}

	URigVMPin* PinForLink = Pin->GetPinForLink();
	if (PinForLink != Pin)
	{
		if (!SetPinDefaultValue(PinForLink, InDefaultValue, bResizeArrays, false, bMergeUndoAction))
		{
			return false;
		}
	}

	return true;
}

bool URigVMController::SetPinDefaultValue(URigVMPin* InPin, const FString& InDefaultValue, bool bResizeArrays, bool bSetupUndoRedo, bool bMergeUndoAction)
{
	check(InPin);
	ensure(!InDefaultValue.IsEmpty());

	if (InPin->GetDirection() == ERigVMPinDirection::Hidden)
	{
		return false;
	}
	
	URigVMGraph* Graph = GetGraph();
	check(Graph);
 
	if (bValidatePinDefaults)
	{
		if (!InPin->IsValidDefaultValue(InDefaultValue))
		{
			return false;
		}
	}

	FRigVMSetPinDefaultValueAction Action;
	if (bSetupUndoRedo)
	{
		Action = FRigVMSetPinDefaultValueAction(InPin, InDefaultValue);
		Action.Title = FString::Printf(TEXT("Set Pin Default Value"));
		ActionStack->BeginAction(Action);
	}

	bool bSetPinDefaultValueSucceeded = false;
	if (InPin->IsArray())
	{
		if (ShouldPinBeUnfolded(InPin))
		{
			TArray<FString> Elements = URigVMPin::SplitDefaultValue(InDefaultValue);

			if (bResizeArrays)
			{
				while (Elements.Num() > InPin->SubPins.Num())
				{
					InsertArrayPin(InPin, INDEX_NONE, FString(), bSetupUndoRedo);
				}
				while (Elements.Num() < InPin->SubPins.Num())
				{
					RemoveArrayPin(InPin->SubPins.Last()->GetPinPath(), bSetupUndoRedo);
				}
			}
			else
			{
				ensure(Elements.Num() == InPin->SubPins.Num());
			}

			for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++)
			{
				URigVMPin* SubPin = InPin->SubPins[ElementIndex];
				PostProcessDefaultValue(SubPin, Elements[ElementIndex]);
				if (!Elements[ElementIndex].IsEmpty())
				{
					SetPinDefaultValue(SubPin, Elements[ElementIndex], bResizeArrays, false, false);
					bSetPinDefaultValueSucceeded = true;
				}
			}
		}
	}
	else if (InPin->IsStruct())
	{
		TArray<FString> MemberValuePairs = URigVMPin::SplitDefaultValue(InDefaultValue);

		for (const FString& MemberValuePair : MemberValuePairs)
		{
			FString MemberName, MemberValue;
			if (MemberValuePair.Split(TEXT("="), &MemberName, &MemberValue))
			{
				URigVMPin* SubPin = InPin->FindSubPin(MemberName);
				if (SubPin && !MemberValue.IsEmpty())
				{
					PostProcessDefaultValue(SubPin, MemberValue);
					if (!MemberValue.IsEmpty())
					{
						SetPinDefaultValue(SubPin, MemberValue, bResizeArrays, false, false);
						bSetPinDefaultValueSucceeded = true;
					}
				}
			}
		}
	}
	
	if(!bSetPinDefaultValueSucceeded)
	{
		if (InPin->GetSubPins().Num() == 0)
		{
			InPin->DefaultValue = InDefaultValue;
			Notify(ERigVMGraphNotifType::PinDefaultValueChanged, InPin);
			if (!bSuspendNotifications)
			{
				Graph->MarkPackageDirty();
			}
		}
	}

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action, bMergeUndoAction);
	}

	return true;
}

bool URigVMController::ResetPinDefaultValue(const FString& InPinPath, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMPin* Pin = Graph->FindPin(InPinPath);
	if (Pin == nullptr)
	{
		ReportErrorf(TEXT("Cannot find pin '%s'."), *InPinPath);
		return false;
	}

	if (Cast<URigVMUnitNode>(Pin->GetNode()) == nullptr)
	{
		ReportErrorf(TEXT("Pin '%s' is not on a unit node."), *InPinPath);
		return false;
	}

	return ResetPinDefaultValue(Pin, bSetupUndoRedo);
}

bool URigVMController::ResetPinDefaultValue(URigVMPin* InPin, bool bSetupUndoRedo)
{
	check(InPin);

	if (URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(InPin->GetNode()))
	{
		TSharedPtr<FStructOnScope> StructOnScope = UnitNode->ConstructStructInstance(true /* use default */);

		TArray<FString> Parts;
		if (!URigVMPin::SplitPinPath(InPin->GetPinPath(), Parts))
		{
			return false;
		}

		int32 PartIndex = 1; // cut off the first one since it's the node

		UStruct* Struct = UnitNode->ScriptStruct;
		FProperty* Property = Struct->FindPropertyByName(*Parts[PartIndex++]);
		check(Property);

		uint8* Memory = StructOnScope->GetStructMemory();
		Memory = Property->ContainerPtrToValuePtr<uint8>(Memory);

		while (PartIndex < Parts.Num() && Property != nullptr)
		{
			if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
			{
				Property = ArrayProperty->Inner;
				check(Property);
				PartIndex++;

				if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					UScriptStruct* InnerStruct = StructProperty->Struct;
					StructOnScope = MakeShareable(new FStructOnScope(InnerStruct));
					Memory = (uint8 *)StructOnScope->GetStructMemory();
					InnerStruct->InitializeDefaultValue(Memory);
				}
				continue;
			}

			if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				Struct = StructProperty->Struct;
				Property = Struct->FindPropertyByName(*Parts[PartIndex++]);
				check(Property);
				Memory = Property->ContainerPtrToValuePtr<uint8>(Memory);
				continue;
			}

			break;
		}

		if (Memory)
		{
			FString DefaultValue;
			check(Property);
			Property->ExportTextItem(DefaultValue, Memory, nullptr, nullptr, PPF_None);

			if (!DefaultValue.IsEmpty())
			{
				SetPinDefaultValue(InPin, DefaultValue, true, bSetupUndoRedo, false);
				return true;
			}
		}
	}

	return false;
}



FString URigVMController::AddArrayPin(const FString& InArrayPinPath, const FString& InDefaultValue, bool bSetupUndoRedo)
{
	return InsertArrayPin(InArrayPinPath, INDEX_NONE, InDefaultValue, bSetupUndoRedo);
}

FString URigVMController::DuplicateArrayPin(const FString& InArrayElementPinPath, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return FString();
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMPin* ElementPin = Graph->FindPin(InArrayElementPinPath);
	if (ElementPin == nullptr)
	{
		ReportErrorf(TEXT("Cannot find pin '%s'."), *InArrayElementPinPath);
		return FString();
	}

	if (!ElementPin->IsArrayElement())
	{
		ReportErrorf(TEXT("Pin '%s' is not an array element."), *InArrayElementPinPath);
		return FString();
	}

	URigVMPin* ArrayPin = ElementPin->GetParentPin();
	check(ArrayPin);
	ensure(ArrayPin->IsArray());

	FString DefaultValue = ElementPin->GetDefaultValue();
	return InsertArrayPin(ArrayPin->GetPinPath(), ElementPin->GetPinIndex() + 1, DefaultValue, bSetupUndoRedo);
}

FString URigVMController::InsertArrayPin(const FString& InArrayPinPath, int32 InIndex, const FString& InDefaultValue, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return FString();
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMPin* ArrayPin = Graph->FindPin(InArrayPinPath);
	if (ArrayPin == nullptr)
	{
		ReportErrorf(TEXT("Cannot find pin '%s'."), *InArrayPinPath);
		return FString();
	}

	URigVMPin* ElementPin = InsertArrayPin(ArrayPin, InIndex, InDefaultValue, bSetupUndoRedo);
	if (ElementPin)
	{
		return ElementPin->GetPinPath();
	}

	return FString();
}

URigVMPin* URigVMController::InsertArrayPin(URigVMPin* ArrayPin, int32 InIndex, const FString& InDefaultValue, bool bSetupUndoRedo)
{
	if (!ArrayPin->IsArray())
	{
		ReportErrorf(TEXT("Pin '%s' is not an array."), *ArrayPin->GetPinPath());
		return nullptr;
	}

	if (!ShouldPinBeUnfolded(ArrayPin))
	{
		ReportErrorf(TEXT("Cannot insert array pin under '%s'."), *ArrayPin->GetPinPath());
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (InIndex == INDEX_NONE)
	{
		InIndex = ArrayPin->GetSubPins().Num();
	}

	FRigVMInsertArrayPinAction Action;
	if (bSetupUndoRedo)
	{
		Action = FRigVMInsertArrayPinAction(ArrayPin, InIndex, InDefaultValue);
		Action.Title = FString::Printf(TEXT("Insert Array Pin"));
		ActionStack->BeginAction(Action);
	}

	for (int32 ExistingIndex = ArrayPin->GetSubPins().Num() - 1; ExistingIndex >= InIndex; ExistingIndex--)
	{
		URigVMPin* ExistingPin = ArrayPin->GetSubPins()[ExistingIndex];
		ExistingPin->Rename(*FString::FormatAsNumber(ExistingIndex + 1));
	}

	URigVMPin* Pin = NewObject<URigVMPin>(ArrayPin, *FString::FormatAsNumber(InIndex));
	ConfigurePinFromPin(Pin, ArrayPin);
	Pin->CPPType = ArrayPin->GetArrayElementCppType();
	ArrayPin->SubPins.Insert(Pin, InIndex);

	if (Pin->IsStruct())
	{
		UScriptStruct* ScriptStruct = Pin->GetScriptStruct();
		if (ScriptStruct)
		{
			FString DefaultValue = InDefaultValue;
			CreateDefaultValueForStructIfRequired(ScriptStruct, DefaultValue);
			AddPinsForStruct(ScriptStruct, Pin->GetNode(), Pin, Pin->Direction, DefaultValue, false);
		}
	}
	else if (Pin->IsArray())
	{
		FArrayProperty * ArrayProperty = CastField<FArrayProperty>(FindPropertyForPin(Pin->GetPinPath()));
		if (ArrayProperty)
		{
			TArray<FString> ElementDefaultValues = URigVMPin::SplitDefaultValue(InDefaultValue);
			AddPinsForArray(ArrayProperty, Pin->GetNode(), Pin, Pin->Direction, ElementDefaultValues, false);
		}
	}
	else
	{
		FString DefaultValue = InDefaultValue;
		PostProcessDefaultValue(Pin, DefaultValue);
		Pin->DefaultValue = DefaultValue;
	}

	Notify(ERigVMGraphNotifType::PinArraySizeChanged, ArrayPin);
	if (!bSuspendNotifications)
	{
		Graph->MarkPackageDirty();
	}

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action);
	}

	return Pin;
}

bool URigVMController::RemoveArrayPin(const FString& InArrayElementPinPath, bool bSetupUndoRedo)
{
	if(!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMPin* ArrayElementPin = Graph->FindPin(InArrayElementPinPath);
	if (ArrayElementPin == nullptr)
	{
		ReportErrorf(TEXT("Cannot find pin '%s'."), *InArrayElementPinPath);
		return false;
	}

	if (!ArrayElementPin->IsArrayElement())
	{
		ReportErrorf(TEXT("Pin '%s' is not an array element."), *InArrayElementPinPath);
		return false;
	}

	URigVMPin* ArrayPin = ArrayElementPin->GetParentPin();
	check(ArrayPin);
	ensure(ArrayPin->IsArray());

	FRigVMRemoveArrayPinAction Action;
	if (bSetupUndoRedo)
	{
		Action = FRigVMRemoveArrayPinAction(ArrayElementPin);
		Action.Title = FString::Printf(TEXT("Remove Array Pin"));
		ActionStack->BeginAction(Action);
	}

	int32 IndexToRemove = ArrayElementPin->GetPinIndex();
	if (!RemovePin(ArrayElementPin, bSetupUndoRedo, false))
	{
		return false;
	}

	for (int32 ExistingIndex = ArrayPin->GetSubPins().Num() - 1; ExistingIndex >= IndexToRemove; ExistingIndex--)
	{
		URigVMPin* ExistingPin = ArrayPin->GetSubPins()[ExistingIndex];
		ExistingPin->SetNameFromIndex();
	}

	if (!bSuspendNotifications)
	{
		Graph->MarkPackageDirty();
	}
	Notify(ERigVMGraphNotifType::PinArraySizeChanged, ArrayPin);

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action);
	}

	return true;
}

bool URigVMController::RemovePin(URigVMPin* InPinToRemove, bool bSetupUndoRedo, bool bNotify)
{
	if (bSetupUndoRedo)
	{
		BreakAllLinks(InPinToRemove, true, bSetupUndoRedo);
		BreakAllLinks(InPinToRemove, false, bSetupUndoRedo);
		BreakAllLinksRecursive(InPinToRemove, true, false, bSetupUndoRedo);
		BreakAllLinksRecursive(InPinToRemove, false, false, bSetupUndoRedo);
	}

	if (URigVMPin* ParentPin = InPinToRemove->GetParentPin())
	{
		ParentPin->SubPins.Remove(InPinToRemove);
	}
	else if(URigVMNode* Node = InPinToRemove->GetNode())
	{
		Node->Pins.Remove(InPinToRemove);
	}

	TArray<URigVMPin*> SubPins = InPinToRemove->GetSubPins();
	for (URigVMPin* SubPin : SubPins)
	{
		if (!RemovePin(SubPin, bSetupUndoRedo, bNotify))
		{
			return false;
		}
	}

	if (bNotify)
	{
		Notify(ERigVMGraphNotifType::PinRemoved, InPinToRemove);
	}

	DestroyObject(InPinToRemove);

	return true;
}

bool URigVMController::ClearArrayPin(const FString& InArrayPinPath, bool bSetupUndoRedo)
{
	return SetArrayPinSize(InArrayPinPath, 0, FString(), bSetupUndoRedo);
}

bool URigVMController::SetArrayPinSize(const FString& InArrayPinPath, int32 InSize, const FString& InDefaultValue, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMPin* Pin = Graph->FindPin(InArrayPinPath);
	if (Pin == nullptr)
	{
		ReportErrorf(TEXT("Cannot find pin '%s'."), *InArrayPinPath);
		return false;
	}

	if (!Pin->IsArray())
	{
		ReportErrorf(TEXT("Pin '%s' is not an array."), *InArrayPinPath);
		return false;
	}

	FRigVMBaseAction Action;
	if (bSetupUndoRedo)
	{
		Action.Title = FString::Printf(TEXT("Set Array Pin Size (%d)"), InSize);
		ActionStack->BeginAction(Action);
	}

	InSize = FMath::Max<int32>(InSize, 0);
	int32 AddedPins = 0;
	int32 RemovedPins = 0;

	FString DefaultValue = InDefaultValue;
	if (DefaultValue.IsEmpty())
	{
		if (Pin->GetSubPins().Num() > 0)
		{
			DefaultValue = Pin->GetSubPins().Last()->GetDefaultValue();
		}
		CreateDefaultValueForStructIfRequired(Pin->GetScriptStruct(), DefaultValue);
	}

	while (Pin->GetSubPins().Num() > InSize)
	{
		if (!RemoveArrayPin(Pin->GetSubPins()[Pin->GetSubPins().Num()-1]->GetPinPath(), bSetupUndoRedo))
		{
			if (bSetupUndoRedo)
			{
				ActionStack->CancelAction(Action);
			}
			return false;
		}
		RemovedPins++;
	}

	while (Pin->GetSubPins().Num() < InSize)
	{
		if (AddArrayPin(Pin->GetPinPath(), DefaultValue, bSetupUndoRedo).IsEmpty())
		{
			if (bSetupUndoRedo)
			{
				ActionStack->CancelAction(Action);
			}
			return false;
		}
		AddedPins++;
	}

	if (bSetupUndoRedo)
	{
		if (RemovedPins > 0 || AddedPins > 0)
		{
			ActionStack->EndAction(Action);
		}
		else
		{
			ActionStack->CancelAction(Action);
		}
	}

	return RemovedPins > 0 || AddedPins > 0;
}

bool URigVMController::BindPinToVariable(const FString& InPinPath, const FString& InNewBoundVariablePath, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMPin* Pin = Graph->FindPin(InPinPath);
	if (Pin == nullptr)
	{
		ReportErrorf(TEXT("Cannot find pin '%s'."), *InPinPath);
		return false;
	}

	return BindPinToVariable(Pin, InNewBoundVariablePath, bSetupUndoRedo);
}

bool URigVMController::UnbindPinFromVariable(const FString& InPinPath, bool bSetupUndoRedo)
{
	return BindPinToVariable(InPinPath, FString(), bSetupUndoRedo);
}

bool URigVMController::BindPinToVariable(URigVMPin* InPin, const FString& InNewBoundVariablePath, bool bSetupUndoRedo)
{
	if (!IsValidPinForGraph(InPin))
	{
		return false;
	}

	if (InPin->GetBoundVariablePath() == InNewBoundVariablePath)
	{
		return false;
	}

	if (GetGraph()->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Cannot bind pins to variables in function library graphs."));
		return false;
	}

	// check that the variable is compatible
	if (!InNewBoundVariablePath.IsEmpty())
	{
		FString VariableName = InNewBoundVariablePath, SegmentPath;
		InNewBoundVariablePath.Split(TEXT("."), &VariableName, &SegmentPath);

		FRigVMExternalVariable ExternalVariable = GetExternalVariableByName(*VariableName);
		if (ExternalVariable.IsValid(true))
		{
			FRigVMRegisterOffset RegisterOffset;
			if (!SegmentPath.IsEmpty())
			{
				RegisterOffset = FRigVMRegisterOffset(Cast<UScriptStruct>(ExternalVariable.TypeObject), SegmentPath);
			}
			if (!InPin->CanBeBoundToVariable(ExternalVariable, RegisterOffset))
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	FRigVMBaseAction Action;
	if (bSetupUndoRedo)
	{
		if (InNewBoundVariablePath.IsEmpty())
		{
			Action.Title = TEXT("Unbind pin from variable");
		}
		else
		{
			Action.Title = TEXT("Bind pin to variable");
		}
		ActionStack->BeginAction(Action);
	}

	if (!InPin->IsBoundToVariable() && bSetupUndoRedo)
	{
		// break all links on pin towards parent + children
		BreakAllLinks(InPin, true, bSetupUndoRedo);
		BreakAllLinksRecursive(InPin, true, true, bSetupUndoRedo);
		BreakAllLinksRecursive(InPin, true, false, bSetupUndoRedo);
	}

	if (bSetupUndoRedo)
	{
		ActionStack->AddAction(FRigVMSetPinBoundVariableAction(InPin, InNewBoundVariablePath));
	}

	InPin->BoundVariablePath = InNewBoundVariablePath;
	Notify(ERigVMGraphNotifType::PinBoundVariableChanged, InPin);

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action);
	}

	return true;
}

bool URigVMController::MakeBindingsFromVariableNode(const FName& InNodeName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(Graph->FindNodeByName(InNodeName)))
	{
		return MakeBindingsFromVariableNode(VariableNode, bSetupUndoRedo);
	}

	return false;
}

bool URigVMController::MakeBindingsFromVariableNode(URigVMVariableNode* InNode, bool bSetupUndoRedo)
{
	check(InNode);

	TArray<TPair<URigVMPin*, URigVMPin*>> Pairs;
	TArray<URigVMNode*> NodesToRemove;
	NodesToRemove.Add(InNode);

	if (URigVMPin* ValuePin = InNode->FindPin(URigVMVariableNode::ValueName))
	{
		TArray<URigVMLink*> Links = ValuePin->GetTargetLinks(true);
		for (URigVMLink* Link : Links)
		{
			URigVMPin* SourcePin = Link->GetSourcePin();

			TArray<URigVMPin*> TargetPins;
			TargetPins.Add(Link->GetTargetPin());

			for (int32 TargetPinIndex = 0; TargetPinIndex < TargetPins.Num(); TargetPinIndex++)
			{
				URigVMPin* TargetPin = TargetPins[TargetPinIndex];
				if (Cast<URigVMRerouteNode>(TargetPin->GetNode()))
				{
					NodesToRemove.AddUnique(TargetPin->GetNode());
					TargetPins.Append(TargetPin->GetLinkedTargetPins(false /* recursive */));
				}
				else
				{
					Pairs.Add(TPair<URigVMPin*, URigVMPin*>(SourcePin, TargetPin));
				}
			}
		}
	}

	FName VariableName = InNode->GetVariableName();
	FRigVMExternalVariable ExternalVariable = GetExternalVariableByName(VariableName);
	if (!ExternalVariable.IsValid(true /* allow nullptr */))
	{
		return false;
	}

	if (Pairs.Num() > 0)
	{
		if (bSetupUndoRedo)
		{
			OpenUndoBracket(TEXT("Turn Variable Node into Bindings"));
		}

		for (const TPair<URigVMPin*, URigVMPin*>& Pair : Pairs)
		{
			URigVMPin* SourcePin = Pair.Key;
			URigVMPin* TargetPin = Pair.Value;
			FString SegmentPath = SourcePin->GetSegmentPath();
			FString VariablePathToBind = VariableName.ToString();
			if (!SegmentPath.IsEmpty())
			{
				VariablePathToBind = FString::Printf(TEXT("%s.%s"), *VariablePathToBind, *SegmentPath);
			}

			if (!BindPinToVariable(TargetPin, VariablePathToBind, bSetupUndoRedo))
			{
				CancelUndoBracket();
			}
		}

		for (URigVMNode* NodeToRemove : NodesToRemove)
		{
			RemoveNode(NodeToRemove, bSetupUndoRedo, true);
		}

		if (bSetupUndoRedo)
		{
			CloseUndoBracket();
		}
		return true;
	}

	return false;

}

bool URigVMController::MakeVariableNodeFromBinding(const FString& InPinPath, const FVector2D& InNodePosition, bool bSetupUndoRedo)
{
	return PromotePinToVariable(InPinPath, true, InNodePosition, bSetupUndoRedo);
}

bool URigVMController::PromotePinToVariable(const FString& InPinPath, bool bCreateVariableNode, const FVector2D& InNodePosition, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMPin* Pin = Graph->FindPin(InPinPath);
	if (Pin == nullptr)
	{
		ReportErrorf(TEXT("Cannot find pin '%s'."), *InPinPath);
		return false;
	}

	return PromotePinToVariable(Pin, bCreateVariableNode, InNodePosition, bSetupUndoRedo);
}

bool URigVMController::PromotePinToVariable(URigVMPin* InPin, bool bCreateVariableNode, const FVector2D& InNodePosition, bool bSetupUndoRedo)
{
	check(InPin);

	if (GetGraph()->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Cannot promote pins to variables in function library graphs."));
		return false;
	}

	if (InPin->GetDirection() != ERigVMPinDirection::Input)
	{
		return false;
	}

	FRigVMExternalVariable VariableForPin;
	FString SegmentPath;
	if (InPin->IsBoundToVariable())
	{
		VariableForPin = GetExternalVariableByName(*InPin->GetBoundVariableName());
		check(VariableForPin.IsValid(true /* allow nullptr */));
		SegmentPath = InPin->GetBoundVariablePath();
		if (SegmentPath.StartsWith(VariableForPin.Name.ToString() + TEXT(".")))
		{
			SegmentPath = SegmentPath.RightChop(VariableForPin.Name.ToString().Len());
		}
		else
		{
			SegmentPath.Empty();
		}
	}
	else
	{
		if (!UnitNodeCreatedContext.GetCreateExternalVariableDelegate().IsBound())
		{
			return false;
		}

		VariableForPin = InPin->ToExternalVariable();
		FName VariableName = UnitNodeCreatedContext.GetCreateExternalVariableDelegate().Execute(VariableForPin, InPin->GetDefaultValue());
		if (VariableName.IsNone())
		{
			return false;
		}

		VariableForPin = GetExternalVariableByName(VariableName);
		if (!VariableForPin.IsValid(true /* allow nullptr*/))
		{
			return false;
		}
	}

	if (bCreateVariableNode)
	{
		if (URigVMVariableNode* VariableNode = AddVariableNode(
			VariableForPin.Name,
			VariableForPin.TypeName.ToString(),
			VariableForPin.TypeObject,
			true,
			FString(),
			InNodePosition,
			FString(),
			bSetupUndoRedo))
		{
			if (URigVMPin* ValuePin = VariableNode->FindPin(URigVMVariableNode::ValueName))
			{
				return AddLink(ValuePin->GetPinPath() + SegmentPath, InPin->GetPinPath(), bSetupUndoRedo);
			}
		}
	}
	else
	{
		return BindPinToVariable(InPin, VariableForPin.Name.ToString(), bSetupUndoRedo);
	}

	return false;
}

bool URigVMController::AddLink(const FString& InOutputPinPath, const FString& InInputPinPath, bool bSetupUndoRedo)
{
	if(!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	FString OutputPinPath = InOutputPinPath;
	FString InputPinPath = InInputPinPath;

	if (FString* RedirectedOutputPinPath = OutputPinRedirectors.Find(OutputPinPath))
	{
		OutputPinPath = *RedirectedOutputPinPath;
	}
	if (FString* RedirectedInputPinPath = InputPinRedirectors.Find(InputPinPath))
	{
		InputPinPath = *RedirectedInputPinPath;
	}

	URigVMPin* OutputPin = Graph->FindPin(OutputPinPath);
	if (OutputPin == nullptr)
	{
		ReportErrorf(TEXT("Cannot find pin '%s'."), *OutputPinPath);
		return false;
	}
	OutputPin = OutputPin->GetPinForLink();

	URigVMPin* InputPin = Graph->FindPin(InputPinPath);
	if (InputPin == nullptr)
	{
		ReportErrorf(TEXT("Cannot find pin '%s'."), *InputPinPath);
		return false;
	}
	InputPin = InputPin->GetPinForLink();

	return AddLink(OutputPin, InputPin, bSetupUndoRedo);
}

bool URigVMController::AddLink(URigVMPin* OutputPin, URigVMPin* InputPin, bool bSetupUndoRedo)
{
	if(OutputPin == nullptr)
	{
		ReportError(TEXT("OutputPin is nullptr."));
		return false;
	}

	if(InputPin == nullptr)
	{
		ReportError(TEXT("InputPin is nullptr."));
		return false;
	}

	if(!IsValidPinForGraph(OutputPin) || !IsValidPinForGraph(InputPin))
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (Graph->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Cannot add links in function library graphs."));
		return false;
	}

	{
		// Temporarily remove the bound variable from the pin
		// if it exists, so that link validation can work.
		// BreakAllPins will remove the bound variable for real later.
		TGuardValue<FString> OutputBoundVariableGuard(OutputPin->BoundVariablePath, FString());
		TGuardValue<FString> InputBoundVariableGuard(InputPin->BoundVariablePath, FString());

		FString FailureReason;
		if (!Graph->CanLink(OutputPin, InputPin, &FailureReason, GetCurrentByteCode()))
		{
			ReportErrorf(TEXT("Cannot link '%s' to '%s': %s."), *OutputPin->GetPinPath(), *InputPin->GetPinPath(), *FailureReason, GetCurrentByteCode());
			return false;
		}
	}

	ensure(!OutputPin->IsLinkedTo(InputPin));
	ensure(!InputPin->IsLinkedTo(OutputPin));

	FRigVMAddLinkAction Action;
	if (bSetupUndoRedo)
	{
		Action = FRigVMAddLinkAction(OutputPin, InputPin);
		Action.Title = FString::Printf(TEXT("Add Link"));
		ActionStack->BeginAction(Action);
	}

	if (OutputPin->IsExecuteContext())
	{
		BreakAllLinks(OutputPin, false, bSetupUndoRedo);
	}

	BreakAllLinks(InputPin, true, bSetupUndoRedo);
	if (bSetupUndoRedo)
	{
		BreakAllLinksRecursive(InputPin, true, true, bSetupUndoRedo);
		BreakAllLinksRecursive(InputPin, true, false, bSetupUndoRedo);
	}

	if (bSetupUndoRedo)
	{
		ExpandPinRecursively(OutputPin->GetParentPin(), true);
		ExpandPinRecursively(InputPin->GetParentPin(), true);
	}

	URigVMLink* Link = NewObject<URigVMLink>(Graph);
	Link->SourcePin = OutputPin;
	Link->TargetPin = InputPin;
	Link->SourcePinPath = OutputPin->GetPinPath();
	Link->TargetPinPath = InputPin->GetPinPath();
	Graph->Links.Add(Link);
	OutputPin->Links.Add(Link);
	InputPin->Links.Add(Link);

	if (!bSuspendNotifications)
	{
		Graph->MarkPackageDirty();
	}
	Notify(ERigVMGraphNotifType::LinkAdded, Link);

	UpdateRerouteNodeAfterChangingLinks(OutputPin, bSetupUndoRedo);
	UpdateRerouteNodeAfterChangingLinks(InputPin, bSetupUndoRedo);

	TArray<URigVMNode*> NodesVisited;
	PotentiallyResolvePrototypeNode(Cast<URigVMPrototypeNode>(InputPin->GetNode()), bSetupUndoRedo, NodesVisited);
	PotentiallyResolvePrototypeNode(Cast<URigVMPrototypeNode>(OutputPin->GetNode()), bSetupUndoRedo, NodesVisited);

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action);
	}

	return true;
}

bool URigVMController::BreakLink(const FString& InOutputPinPath, const FString& InInputPinPath, bool bSetupUndoRedo)
{
	if(!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMPin* OutputPin = Graph->FindPin(InOutputPinPath);
	if (OutputPin == nullptr)
	{
		ReportErrorf(TEXT("Cannot find pin '%s'."), *InOutputPinPath);
		return false;
	}
	OutputPin = OutputPin->GetPinForLink();

	URigVMPin* InputPin = Graph->FindPin(InInputPinPath);
	if (InputPin == nullptr)
	{
		ReportErrorf(TEXT("Cannot find pin '%s'."), *InInputPinPath);
		return false;
	}
	InputPin = InputPin->GetPinForLink();

	return BreakLink(OutputPin, InputPin, bSetupUndoRedo);
}

bool URigVMController::BreakLink(URigVMPin* OutputPin, URigVMPin* InputPin, bool bSetupUndoRedo)
{
	if(!IsValidPinForGraph(OutputPin) || !IsValidPinForGraph(InputPin))
	{
		return false;
	}

	if (!OutputPin->IsLinkedTo(InputPin))
	{
		return false;
	}
	ensure(InputPin->IsLinkedTo(OutputPin));

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (Graph->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Cannot break links in function library graphs."));
		return false;
	}

	for (URigVMLink* Link : InputPin->Links)
	{
		if (Link->SourcePin == OutputPin && Link->TargetPin == InputPin)
		{
			FRigVMBreakLinkAction Action;
			if (bSetupUndoRedo)
			{
				Action = FRigVMBreakLinkAction(OutputPin, InputPin);
				Action.Title = FString::Printf(TEXT("Break Link"));
				ActionStack->BeginAction(Action);
			}

			OutputPin->Links.Remove(Link);
			InputPin->Links.Remove(Link);
			Graph->Links.Remove(Link);
			
			if (!bSuspendNotifications)
			{
				Graph->MarkPackageDirty();
			}
			Notify(ERigVMGraphNotifType::LinkRemoved, Link);

			DestroyObject(Link);

			UpdateRerouteNodeAfterChangingLinks(OutputPin, bSetupUndoRedo);
			UpdateRerouteNodeAfterChangingLinks(InputPin, bSetupUndoRedo);

			if (bSetupUndoRedo)
			{
				ActionStack->EndAction(Action);
			}

			return true;
		}
	}

	return false;
}

bool URigVMController::BreakAllLinks(const FString& InPinPath, bool bAsInput, bool bSetupUndoRedo)
{
	if(!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMPin* Pin = Graph->FindPin(InPinPath);
	if (Pin == nullptr)
	{
		ReportErrorf(TEXT("Cannot find pin '%s'."), *InPinPath);
		return false;
	}
	Pin = Pin->GetPinForLink();

	if (!IsValidPinForGraph(Pin))
	{
		return false;
	}

	return BreakAllLinks(Pin, bAsInput, bSetupUndoRedo);
}

bool URigVMController::BreakAllLinks(URigVMPin* Pin, bool bAsInput, bool bSetupUndoRedo)
{
	FRigVMBaseAction Action;
	if (bSetupUndoRedo)
	{
		Action.Title = FString::Printf(TEXT("Break All Links"));
		ActionStack->BeginAction(Action);
	}

	int32 LinksBroken = 0;
	if (Pin->IsBoundToVariable() && bAsInput && bSetupUndoRedo)
	{
		BindPinToVariable(Pin, FString(), bSetupUndoRedo);
		LinksBroken++;
	}

	TArray<URigVMLink*> Links = Pin->GetLinks();
	for (int32 LinkIndex = Links.Num() - 1; LinkIndex >= 0; LinkIndex--)
	{
		URigVMLink* Link = Links[LinkIndex];
		if (bAsInput && Link->GetTargetPin() == Pin)
		{
			LinksBroken += BreakLink(Link->GetSourcePin(), Pin, bSetupUndoRedo) ? 1 : 0;
		}
		else if (!bAsInput && Link->GetSourcePin() == Pin)
		{
			LinksBroken += BreakLink(Pin, Link->GetTargetPin(), bSetupUndoRedo) ? 1 : 0;
		}
	}

	if (bSetupUndoRedo)
	{
		if (LinksBroken > 0)
		{
			ActionStack->EndAction(Action);
		}
		else
		{
			ActionStack->CancelAction(Action);
		}
	}

	return LinksBroken > 0;
}

void URigVMController::BreakAllLinksRecursive(URigVMPin* Pin, bool bAsInput, bool bTowardsParent, bool bSetupUndoRedo)
{
	if (bTowardsParent)
	{
		URigVMPin* ParentPin = Pin->GetParentPin();
		if (ParentPin)
		{
			BreakAllLinks(ParentPin, bAsInput, bSetupUndoRedo);
			BreakAllLinksRecursive(ParentPin, bAsInput, bTowardsParent, bSetupUndoRedo);
		}
	}
	else
	{
		for (URigVMPin* SubPin : Pin->SubPins)
		{
			BreakAllLinks(SubPin, bAsInput, bSetupUndoRedo);
			BreakAllLinksRecursive(SubPin, bAsInput, bTowardsParent, bSetupUndoRedo);
		}
	}
}

FName URigVMController::AddExposedPin(const FName& InPinName, ERigVMPinDirection InDirection, const FString& InCPPType, const FName& InCPPTypeObjectPath, const FString& InDefaultValue, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return NAME_None;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (Graph->IsTopLevelGraph())
	{
		ReportError(TEXT("Exposed pins can only be edited on nested graphs."));
		return NAME_None;
	}

	if (Graph->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Cannot expose pins in function library graphs."));
		return NAME_None;
	}

	URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Graph->GetOuter());
	check(LibraryNode);

	UObject* CPPTypeObject = nullptr;
	if (!InCPPTypeObjectPath.IsNone())
	{
		if (CPPTypeObject == nullptr)
		{
			CPPTypeObject = URigVMCompiler::GetScriptStructForCPPType(InCPPTypeObjectPath.ToString());
		}
		if (CPPTypeObject == nullptr)
		{
			CPPTypeObject = URigVMPin::FindObjectFromCPPTypeObjectPath<UObject>(InCPPTypeObjectPath.ToString());
		}
	}

	if (CPPTypeObject)
	{
		if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(CPPTypeObject))
		{
			if (ScriptStruct->IsChildOf(FRigVMExecuteContext::StaticStruct()))
			{
				for (URigVMPin* ExistingPin : LibraryNode->Pins)
				{
					if (ExistingPin->IsExecuteContext())
					{
						return NAME_None;
					}
				}

				InDirection = ERigVMPinDirection::IO;
			}
		}
	}

	FName PinName = GetUniqueName(InPinName, [LibraryNode](const FName& InName) {

		return LibraryNode->FindPin(InName.ToString()) == nullptr;

	});

	URigVMPin* Pin = NewObject<URigVMPin>(LibraryNode, PinName);
	Pin->CPPType = InCPPType;
	Pin->CPPTypeObjectPath = InCPPTypeObjectPath;
	Pin->bIsConstant = false;
	Pin->Direction = InDirection;
	LibraryNode->Pins.Add(Pin);

	if (Pin->IsStruct())
	{
		FRigVMControllerGraphGuard GraphGuard(this, LibraryNode->GetGraph(), false);

		FString DefaultValue = InDefaultValue;
		CreateDefaultValueForStructIfRequired(Pin->GetScriptStruct(), DefaultValue);
		AddPinsForStruct(Pin->GetScriptStruct(), LibraryNode, Pin, Pin->Direction, DefaultValue, false);
	}

	FRigVMAddExposedPinAction Action(Pin);
	if (bSetupUndoRedo)
	{
		ActionStack->BeginAction(Action);
	}

	{
		FRigVMControllerGraphGuard GraphGuard(this, LibraryNode->GetGraph(), false);
		Notify(ERigVMGraphNotifType::PinAdded, Pin);
	}

	if (!InDefaultValue.IsEmpty())
	{
		FRigVMControllerGraphGuard GraphGuard(this, Pin->GetGraph(), false);
		SetPinDefaultValue(Pin, InDefaultValue, true, bSetupUndoRedo, false);
	}

	RefreshFunctionPins(Graph->GetEntryNode(), true);
	RefreshFunctionPins(Graph->GetReturnNode(), true);
	RefreshFunctionReferences(LibraryNode, false);

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action);
	}
	
	return PinName;
}

bool URigVMController::RemoveExposedPin(const FName& InPinName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (Graph->IsTopLevelGraph())
	{
		ReportError(TEXT("Exposed pins can only be edited on nested graphs."));
		return false;
	}

	if (Graph->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Cannot remove exposed pins in function library graphs."));
		return false;
	}

	URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Graph->GetOuter());
	check(LibraryNode);

	URigVMPin* Pin = LibraryNode->FindPin(InPinName.ToString());
	if (Pin == nullptr)
	{
		return false;
	}

	FRigVMRemoveExposedPinAction Action(Pin);
	if (bSetupUndoRedo)
	{
		ActionStack->BeginAction(Action);
	}

	bool bSuccessfullyRemovedPin = false;
	{
		FRigVMControllerGraphGuard GraphGuard(this, LibraryNode->GetGraph(), false);
		bSuccessfullyRemovedPin = RemovePin(Pin, bSetupUndoRedo, true);
	}

	RefreshFunctionPins(Graph->GetEntryNode(), true);
	RefreshFunctionPins(Graph->GetReturnNode(), true);
	RefreshFunctionReferences(LibraryNode, false);

	if (bSetupUndoRedo)
	{
		if (bSuccessfullyRemovedPin)
		{
			ActionStack->EndAction(Action);
		}
		else
		{
			ActionStack->CancelAction(Action);
		}
	}

	return bSuccessfullyRemovedPin;
}

bool URigVMController::RenameExposedPin(const FName& InOldPinName, const FName& InNewPinName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (Graph->IsTopLevelGraph())
	{
		ReportError(TEXT("Exposed pins can only be edited on nested graphs."));
		return false;
	}

	if (Graph->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Cannot rename exposed pins in function library graphs."));
		return false;
	}

	URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Graph->GetOuter());
	check(LibraryNode);

	URigVMPin* Pin = LibraryNode->FindPin(InOldPinName.ToString());
	if (Pin == nullptr)
	{
		return false;
	}

	if (Pin->GetFName() == InNewPinName)
	{
		return false;
	}

	FName PinName = GetUniqueName(InNewPinName, [LibraryNode](const FName& InName) {
		return LibraryNode->FindPin(InName.ToString()) == nullptr;
		});

	FRigVMRenameExposedPinAction Action;
	if (bSetupUndoRedo)
	{
		Action = FRigVMRenameExposedPinAction(Pin->GetFName(), PinName);
		ActionStack->BeginAction(Action);
	}

	struct Local
	{
		static bool RenamePin(URigVMController* InController, URigVMPin* InPin, const FName& InNewName)
		{
			FRigVMControllerGraphGuard GraphGuard(InController, InPin->GetGraph(), false);

			TArray<URigVMLink*> Links;
			Links.Append(InPin->GetSourceLinks(true));
			Links.Append(InPin->GetTargetLinks(true));

			// store both the ptr + pin path
			for (URigVMLink* Link : Links)
			{
				Link->PrepareForCopy();
				InController->Notify(ERigVMGraphNotifType::LinkRemoved, Link);
			}

			if (!InPin->Rename(*InNewName.ToString()))
			{
				return false;
			}

			// update the eventually stored pin path to the new name
			for (URigVMLink* Link : Links)
			{
				Link->PrepareForCopy();
			}

			InController->Notify(ERigVMGraphNotifType::PinRenamed, InPin);

			for (URigVMLink* Link : Links)
			{
				InController->Notify(ERigVMGraphNotifType::LinkAdded, Link);
			}

			return true;
		}
	};

	if (!Local::RenamePin(this, Pin, PinName))
	{
		ActionStack->CancelAction(Action);
		return false;
	}

	if (URigVMFunctionEntryNode* EntryNode = Graph->GetEntryNode())
	{
		if (URigVMPin* EntryPin = EntryNode->FindPin(InOldPinName.ToString()))
		{
			Local::RenamePin(this, EntryPin, PinName);
		}
	}

	if (URigVMFunctionReturnNode* ReturnNode = Graph->GetReturnNode())
	{
		if (URigVMPin* ReturnPin = ReturnNode->FindPin(InOldPinName.ToString()))
		{
			Local::RenamePin(this, ReturnPin, PinName);
		}
	}

	if (URigVMFunctionLibrary* FunctionLibrary = Cast<URigVMFunctionLibrary>(LibraryNode->GetGraph()))
	{
		FRigVMFunctionReferenceArray* ReferencesEntry = FunctionLibrary->FunctionReferences.Find(LibraryNode);
		if (ReferencesEntry)
		{
			for (TSoftObjectPtr<URigVMFunctionReferenceNode> FunctionReferencePtr : ReferencesEntry->FunctionReferences)
			{
				// only update valid, living references
				if (FunctionReferencePtr.IsValid())
				{
					if (URigVMPin* EntryPin = FunctionReferencePtr->FindPin(InOldPinName.ToString()))
					{
						FRigVMControllerGraphGuard GraphGuard(this, FunctionReferencePtr->GetGraph(), false);
						Local::RenamePin(this, EntryPin, PinName);
					}
				}
			}
		}
	}

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action);
	}

	return true;
}

bool URigVMController::ChangeExposedPinType(const FName& InPinName, const FString& InCPPType, const FName& InCPPTypeObjectPath, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (Graph->IsTopLevelGraph())
	{
		ReportError(TEXT("Exposed pins can only be edited on nested graphs."));
		return false;
	}

	if (Graph->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Cannot change exposed pin types in function library graphs."));
		return false;
	}

	URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Graph->GetOuter());
	check(LibraryNode);

	URigVMPin* Pin = LibraryNode->FindPin(InPinName.ToString());
	if (Pin == nullptr)
	{
		return false;
	}

	FRigVMBaseAction Action;
	if (bSetupUndoRedo)
	{
		Action.Title = FString::Printf(TEXT("Change Pin Type"));
		ActionStack->BeginAction(Action);
	}

	if (bSetupUndoRedo)
	{
		FRigVMControllerGraphGuard GraphGuard(this, LibraryNode->GetGraph(), bSetupUndoRedo);
		if (!ChangePinType(Pin, InCPPType, InCPPTypeObjectPath, bSetupUndoRedo))
		{
			if (bSetupUndoRedo)
			{
				ActionStack->CancelAction(Action);
			}
			return false;
		}
	}

	if (URigVMFunctionEntryNode* EntryNode = Graph->GetEntryNode())
	{
		if (URigVMPin* EntryPin = EntryNode->FindPin(Pin->GetName()))
		{
			ChangePinType(EntryPin, InCPPType, InCPPTypeObjectPath, bSetupUndoRedo);
		}
	}
	if (URigVMFunctionReturnNode* ReturnNode = Graph->GetReturnNode())
	{
		if (URigVMPin* ReturnPin = ReturnNode->FindPin(Pin->GetName()))
		{
			ChangePinType(ReturnPin, InCPPType, InCPPTypeObjectPath, bSetupUndoRedo);
		}
	}

	if (URigVMFunctionLibrary* FunctionLibrary = Cast<URigVMFunctionLibrary>(LibraryNode->GetGraph()))
	{
		FRigVMFunctionReferenceArray* ReferencesEntry = FunctionLibrary->FunctionReferences.Find(LibraryNode);
		if (ReferencesEntry)
		{
			for (TSoftObjectPtr<URigVMFunctionReferenceNode> FunctionReferencePtr : ReferencesEntry->FunctionReferences)
			{
				// only update valid, living references
				if (FunctionReferencePtr.IsValid())
				{
					if (URigVMPin* ReferencedNodePin = FunctionReferencePtr->FindPin(Pin->GetName()))
					{
						FRigVMControllerGraphGuard GraphGuard(this, FunctionReferencePtr->GetGraph(), false);
						ChangePinType(ReferencedNodePin, InCPPType, InCPPTypeObjectPath, bSetupUndoRedo);
					}
				}
			}
		}
	}

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action);
	}

	return true;
}

bool URigVMController::SetExposedPinIndex(const FName& InPinName, int32 InNewIndex, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	FString PinPath = InPinName.ToString();
	if (PinPath.Contains(TEXT(".")))
	{
		ReportError(TEXT("Cannot change pin index for pins on nodes for now - only within collapse nodes."));
		return false;
	}

	URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Graph->GetOuter());
	if (LibraryNode == nullptr)
	{
		ReportError(TEXT("Graph is not under a Collapse Node"));
		return false;
	}

	URigVMPin* Pin = LibraryNode->FindPin(PinPath);
	if (Pin == nullptr)
	{
		ReportErrorf(TEXT("Cannot find exposed pin '%s'."), *PinPath);
		return false;
	}

	if (Pin->GetPinIndex() == InNewIndex)
	{
		return false;
	}

	if (InNewIndex < 0 || InNewIndex >= LibraryNode->GetPins().Num())
	{
		ReportErrorf(TEXT("Invalid new pin index '%d'."), InNewIndex);
		return false;
	}

	FRigVMSetPinIndexAction PinIndexAction(Pin, InNewIndex);
	{
		LibraryNode->Pins.Remove(Pin);
		LibraryNode->Pins.Insert(Pin, InNewIndex);

		FRigVMControllerGraphGuard GraphGuard(this, LibraryNode->GetGraph(), false);
		Notify(ERigVMGraphNotifType::PinIndexChanged, Pin);
	}

	RefreshFunctionPins(LibraryNode->GetEntryNode(), true);
	RefreshFunctionPins(LibraryNode->GetReturnNode(), true);
	RefreshFunctionReferences(LibraryNode, false);
	
	if (bSetupUndoRedo)
	{
		ActionStack->AddAction(PinIndexAction);
	}

	return true;
}

URigVMFunctionReferenceNode* URigVMController::AddFunctionReferenceNode(URigVMLibraryNode* InFunctionDefinition, const FVector2D& InNodePosition, const FString& InNodeName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (Graph->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Cannot add function reference nodes to function library graphs."));
		return nullptr;
	}

	if (InFunctionDefinition == nullptr)
	{
		ReportError(TEXT("Cannot add a function reference node without a valid function definition."));
		return nullptr;
	}

	if (!InFunctionDefinition->GetGraph()->IsA<URigVMFunctionLibrary>())
	{
		ReportAndNotifyError(TEXT("Cannot use the function definition for a function reference node."));
		return nullptr;
	}

	if (Graph->GetOutermost() == GetTransientPackage())
	{
		// don't allow linking to transient graphs, which usually mean template nodes
		return nullptr;
	}

	if(!CanAddFunctionRefForDefinition(InFunctionDefinition, true))
	{
		return nullptr;
	}

	FString NodeName = GetValidNodeName(InNodeName.IsEmpty() ? InFunctionDefinition->GetName() : InNodeName);
	URigVMFunctionReferenceNode* FunctionRefNode = NewObject<URigVMFunctionReferenceNode>(Graph, *NodeName);
	FunctionRefNode->Position = InNodePosition;
	FunctionRefNode->SetReferencedNode(InFunctionDefinition);
	Graph->Nodes.Add(FunctionRefNode);

	RepopulatePinsOnNode(FunctionRefNode, false, false);

	Notify(ERigVMGraphNotifType::NodeAdded, FunctionRefNode);

	if (URigVMFunctionLibrary* FunctionLibrary = InFunctionDefinition->GetLibrary())
	{
		FunctionLibrary->FunctionReferences.FindOrAdd(InFunctionDefinition).FunctionReferences.Add(FunctionRefNode);
	}

	for (URigVMPin* SourcePin : InFunctionDefinition->Pins)
	{
		if (URigVMPin* TargetPin = FunctionRefNode->FindPin(SourcePin->GetName()))
		{
			FString DefaultValue = SourcePin->GetDefaultValue();
			if (!DefaultValue.IsEmpty())
			{
				SetPinDefaultValue(TargetPin, DefaultValue, true, false, false);
			}
		}
	}

	if (bSetupUndoRedo)
	{
		FRigVMInverseAction InverseAction;
		InverseAction.Title = TEXT("Add function node");

		ActionStack->BeginAction(InverseAction);
		ActionStack->AddAction(FRigVMRemoveNodeAction(FunctionRefNode, this));
		ActionStack->EndAction(InverseAction);
	}

	return FunctionRefNode;
}

URigVMLibraryNode* URigVMController::AddFunctionToLibrary(const FName& InFunctionName, bool bMutable, const FVector2D& InNodePosition, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (!Graph->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Can only add function definitions to function library graphs."));
		return nullptr;
	}

	FString FunctionName = GetValidNodeName(InFunctionName.IsNone() ? FString(TEXT("Function")) : InFunctionName.ToString());
	URigVMCollapseNode* CollapseNode = NewObject<URigVMCollapseNode>(Graph, *FunctionName);
	CollapseNode->ContainedGraph = NewObject<URigVMGraph>(CollapseNode, TEXT("ContainedGraph"));
	CollapseNode->Position = InNodePosition;
	Graph->Nodes.Add(CollapseNode);

	if (bMutable)
	{
		URigVMPin* ExecutePin = NewObject<URigVMPin>(CollapseNode, FRigVMStruct::ExecuteContextName);
		ExecutePin->CPPType = FString::Printf(TEXT("F%s"), *ExecuteContextStruct->GetName());
		ExecutePin->CPPTypeObject = ExecuteContextStruct;
		ExecutePin->CPPTypeObjectPath = *ExecutePin->CPPTypeObject->GetPathName();
		ExecutePin->Direction = ERigVMPinDirection::IO;
		CollapseNode->Pins.Add(ExecutePin);
	}

	Notify(ERigVMGraphNotifType::NodeAdded, CollapseNode);

	{
		FRigVMControllerGraphGuard GraphGuard(this, CollapseNode->GetContainedGraph(), false);

		URigVMFunctionEntryNode* EntryNode = NewObject<URigVMFunctionEntryNode>(CollapseNode->ContainedGraph, TEXT("Entry"));
		CollapseNode->ContainedGraph->Nodes.Add(EntryNode);
		EntryNode->Position = FVector2D(-250.f, 0.f);
		RefreshFunctionPins(EntryNode, false);
		Notify(ERigVMGraphNotifType::NodeAdded, EntryNode);

		URigVMFunctionReturnNode* ReturnNode = NewObject<URigVMFunctionReturnNode>(CollapseNode->ContainedGraph, TEXT("Return"));
		CollapseNode->ContainedGraph->Nodes.Add(ReturnNode);
		ReturnNode->Position = FVector2D(250.f, 0.f);
		RefreshFunctionPins(ReturnNode, false);
		Notify(ERigVMGraphNotifType::NodeAdded, ReturnNode);

		if (bMutable)
		{
			AddLink(EntryNode->FindPin(FRigVMStruct::ExecuteContextName.ToString()), ReturnNode->FindPin(FRigVMStruct::ExecuteContextName.ToString()), false);
		}
	}

	if (bSetupUndoRedo)
	{
		FRigVMInverseAction InverseAction;
		InverseAction.Title = TEXT("Add function to library");

		ActionStack->BeginAction(InverseAction);
		ActionStack->AddAction(FRigVMRemoveNodeAction(CollapseNode, this));
		ActionStack->EndAction(InverseAction);
	}

	return CollapseNode;
}

bool URigVMController::RemoveFunctionFromLibrary(const FName& InFunctionName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);


	if (!Graph->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Can only remove function definitions from function library graphs."));
		return false;
	}

	return RemoveNodeByName(InFunctionName, bSetupUndoRedo);
}

void URigVMController::ExpandPinRecursively(URigVMPin* InPin, bool bSetupUndoRedo)
{
	if (InPin == nullptr)
	{
		return;
	}

	if (bSetupUndoRedo)
	{
		OpenUndoBracket(TEXT("Expand Pin Recursively"));
	}

	bool bExpandedSomething = false;
	while (InPin)
	{
		if (SetPinExpansion(InPin, true, bSetupUndoRedo))
		{
			bExpandedSomething = true;
		}
		InPin = InPin->GetParentPin();
	}

	if (bSetupUndoRedo)
	{
		if (bExpandedSomething)
		{
			CloseUndoBracket();
		}
		else
		{
			CancelUndoBracket();
		}
	}
}

bool URigVMController::SetVariableName(URigVMVariableNode* InVariableNode, const FName& InVariableName, bool bSetupUndoRedo)
{
	if (!IsValidNodeForGraph(InVariableNode))
	{
		return false;
	}

	if (InVariableNode->GetVariableName() == InVariableName)
	{
		return false;
	}

	if (InVariableName == NAME_None)
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	TArray<FRigVMGraphVariableDescription> Descriptions = Graph->GetVariableDescriptions();
	TMap<FName, int32> NameToIndex;
	for (int32 VariableIndex = 0; VariableIndex < Descriptions.Num(); VariableIndex++)
	{
		NameToIndex.Add(Descriptions[VariableIndex].Name, VariableIndex);
	}

	FName VariableName = GetUniqueName(InVariableName, [Descriptions, NameToIndex, InVariableNode](const FName& InName) {
		const int32* FoundIndex = NameToIndex.Find(InName);
		if (FoundIndex == nullptr)
		{
			return true;
		}
		return InVariableNode->GetCPPType() == Descriptions[*FoundIndex].CPPType;
	});

	int32 NodesSharingName = 0;
	for (URigVMNode* Node : Graph->Nodes)
	{
		if (URigVMVariableNode* OtherVariableNode = Cast<URigVMVariableNode>(Node))
		{
			if (OtherVariableNode->GetVariableName() == InVariableNode->GetVariableName())
			{
				NodesSharingName++;
			}
		}
	}

	if (NodesSharingName == 1)
	{
		Notify(ERigVMGraphNotifType::VariableRemoved, InVariableNode);
	}

	SetPinDefaultValue(InVariableNode->FindPin(URigVMVariableNode::VariableName), VariableName.ToString(), false, bSetupUndoRedo, false);

	Notify(ERigVMGraphNotifType::VariableAdded, InVariableNode);

	return true;
}

bool URigVMController::SetParameterName(URigVMParameterNode* InParameterNode, const FName& InParameterName, bool bSetupUndoRedo)
{
	if (!IsValidNodeForGraph(InParameterNode))
	{
		return false;
	}

	if (InParameterNode->GetParameterName() == InParameterName)
	{
		return false;
	}

	if (InParameterName == NAME_None)
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	TArray<FRigVMGraphParameterDescription> Descriptions = Graph->GetParameterDescriptions();
	TMap<FName, int32> NameToIndex;
	for (int32 ParameterIndex = 0; ParameterIndex < Descriptions.Num(); ParameterIndex++)
	{
		NameToIndex.Add(Descriptions[ParameterIndex].Name, ParameterIndex);
	}

	FName ParameterName = GetUniqueName(InParameterName, [Descriptions, NameToIndex, InParameterNode](const FName& InName) {
		const int32* FoundIndex = NameToIndex.Find(InName);
		if (FoundIndex == nullptr)
		{
			return true;
		}
		return InParameterNode->GetCPPType() == Descriptions[*FoundIndex].CPPType && InParameterNode->IsInput() == Descriptions[*FoundIndex].bIsInput;
	});

	int32 NodesSharingName = 0;
	for (URigVMNode* Node : Graph->Nodes)
	{
		if (URigVMParameterNode* OtherParameterNode = Cast<URigVMParameterNode>(Node))
		{
			if (OtherParameterNode->GetParameterName() == InParameterNode->GetParameterName())
			{
				NodesSharingName++;
			}
		}
	}

	if (NodesSharingName == 1)
	{
		Notify(ERigVMGraphNotifType::ParameterRemoved, InParameterNode);
	}

	SetPinDefaultValue(InParameterNode->FindPin(URigVMParameterNode::ParameterName), ParameterName.ToString(), false, bSetupUndoRedo, false);

	Notify(ERigVMGraphNotifType::ParameterAdded, InParameterNode);

	return true;
}

URigVMRerouteNode* URigVMController::AddFreeRerouteNode(bool bShowAsFullNode, const FString& InCPPType, const FName& InCPPTypeObjectPath, bool bIsConstant, const FName& InCustomWidgetName, const FString& InDefaultValue, const FVector2D& InPosition, const FString& InNodeName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (Graph->IsA<URigVMFunctionLibrary>())
	{
		ReportError(TEXT("Cannot add reroutes to function library graphs."));
		return nullptr;
	}

	FRigVMBaseAction Action;
	if (bSetupUndoRedo)
	{
		Action.Title = FString::Printf(TEXT("Add Reroute"));
		ActionStack->BeginAction(Action);
	}

	FString Name = GetValidNodeName(InNodeName.IsEmpty() ? FString(TEXT("RerouteNode")) : InNodeName);
	URigVMRerouteNode* Node = NewObject<URigVMRerouteNode>(Graph, *Name);
	Node->Position = InPosition;
	Node->bShowAsFullNode = bShowAsFullNode;

	URigVMPin* ValuePin = NewObject<URigVMPin>(Node, *URigVMRerouteNode::ValueName);
	ValuePin->CPPType = InCPPType;
	ValuePin->CPPTypeObjectPath = InCPPTypeObjectPath;
	ValuePin->bIsConstant = bIsConstant;
	ValuePin->CustomWidgetName = InCustomWidgetName;
	ValuePin->Direction = ERigVMPinDirection::IO;
	Node->Pins.Add(ValuePin);
	Graph->Nodes.Add(Node);

	if (ValuePin->IsStruct())
	{
		FString DefaultValue = InDefaultValue;
		CreateDefaultValueForStructIfRequired(ValuePin->GetScriptStruct(), DefaultValue);
		AddPinsForStruct(ValuePin->GetScriptStruct(), Node, ValuePin, ValuePin->Direction, DefaultValue, false);
	}
	else if (!InDefaultValue.IsEmpty() && InDefaultValue != TEXT("()"))
	{
		SetPinDefaultValue(ValuePin, InDefaultValue, true, false, false);
	}

	if (bSetupUndoRedo)
	{
		ActionStack->AddAction(FRigVMAddRerouteNodeAction(Node));
	}

	Notify(ERigVMGraphNotifType::NodeAdded, Node);

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action);
	}

	return Node;
}

URigVMBranchNode* URigVMController::AddBranchNode(const FVector2D& InPosition, const FString& InNodeName, bool bSetupUndoRedo)
{
	if(!IsValidGraph())
	{
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	FString Name = GetValidNodeName(InNodeName.IsEmpty() ? FString(TEXT("BranchNode")) : InNodeName);
	URigVMBranchNode* Node = NewObject<URigVMBranchNode>(Graph, *Name);
	Node->Position = InPosition;

	URigVMPin* ExecutePin = NewObject<URigVMPin>(Node, FRigVMStruct::ExecuteContextName);
	ExecutePin->DisplayName = FRigVMStruct::ExecuteName;
	ExecutePin->CPPType = FString::Printf(TEXT("F%s"), *ExecuteContextStruct->GetName());
	ExecutePin->CPPTypeObject = ExecuteContextStruct;
	ExecutePin->CPPTypeObjectPath = *ExecutePin->CPPTypeObject->GetPathName();
	ExecutePin->Direction = ERigVMPinDirection::Input;
	Node->Pins.Add(ExecutePin);

	URigVMPin* ConditionPin = NewObject<URigVMPin>(Node, *URigVMBranchNode::ConditionName);
	ConditionPin->CPPType = TEXT("bool");
	ConditionPin->Direction = ERigVMPinDirection::Input;
	Node->Pins.Add(ConditionPin);

	URigVMPin* TruePin = NewObject<URigVMPin>(Node, *URigVMBranchNode::TrueName);
	TruePin->CPPType = ExecutePin->CPPType;
	TruePin->CPPTypeObject = ExecutePin->CPPTypeObject;
	TruePin->CPPTypeObjectPath = ExecutePin->CPPTypeObjectPath;
	TruePin->Direction = ERigVMPinDirection::Output;
	Node->Pins.Add(TruePin);

	URigVMPin* FalsePin = NewObject<URigVMPin>(Node, *URigVMBranchNode::FalseName);
	FalsePin->CPPType = ExecutePin->CPPType;
	FalsePin->CPPTypeObject = ExecutePin->CPPTypeObject;
	FalsePin->CPPTypeObjectPath = ExecutePin->CPPTypeObjectPath;
	FalsePin->Direction = ERigVMPinDirection::Output;
	Node->Pins.Add(FalsePin);

	Graph->Nodes.Add(Node);

	Notify(ERigVMGraphNotifType::NodeAdded, Node);

	if (bSetupUndoRedo)
	{
		ActionStack->AddAction(FRigVMAddBranchNodeAction(Node));
	}

	return Node;
}

URigVMIfNode* URigVMController::AddIfNode(const FString& InCPPType, const FName& InCPPTypeObjectPath, const FVector2D& InPosition, const FString& InNodeName, bool bSetupUndoRedo)
{
	if(!IsValidGraph())
	{
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	ensure(!InCPPType.IsEmpty());

	FString CPPType = InCPPType;
	UObject* CPPTypeObject = nullptr;
	if(!InCPPTypeObjectPath.IsNone())
	{
		CPPTypeObject = URigVMPin::FindObjectFromCPPTypeObjectPath<UObject>(InCPPTypeObjectPath.ToString());
		if (CPPTypeObject == nullptr)
		{
			ReportErrorf(TEXT("Cannot find cpp type object for path '%s'."), *InCPPTypeObjectPath.ToString());
			return nullptr;
		}
	}

	FString DefaultValue;
	if(UScriptStruct* ScriptStruct = Cast<UScriptStruct>(CPPTypeObject))
	{
		if (ScriptStruct->IsChildOf(FRigVMExecuteContext::StaticStruct()))
		{
			ReportErrorf(TEXT("Cannot create an if node for this type '%s'."), *InCPPTypeObjectPath.ToString());
			return nullptr;
		}
		CreateDefaultValueForStructIfRequired(ScriptStruct, DefaultValue);

		CPPType = ScriptStruct->GetStructCPPName();
	}
	
	FString Name = GetValidNodeName(InNodeName.IsEmpty() ? FString(TEXT("IfNode")) : InNodeName);
	URigVMIfNode* Node = NewObject<URigVMIfNode>(Graph, *Name);
	Node->Position = InPosition;

	URigVMPin* ConditionPin = NewObject<URigVMPin>(Node, *URigVMIfNode::ConditionName);
	ConditionPin->CPPType = TEXT("bool");
	ConditionPin->Direction = ERigVMPinDirection::Input;
	Node->Pins.Add(ConditionPin);

	URigVMPin* TruePin = NewObject<URigVMPin>(Node, *URigVMIfNode::TrueName);
	TruePin->CPPType = CPPType;
	TruePin->CPPTypeObject = CPPTypeObject;
	TruePin->CPPTypeObjectPath = InCPPTypeObjectPath;
	TruePin->Direction = ERigVMPinDirection::Input;
	TruePin->DefaultValue = DefaultValue;
	Node->Pins.Add(TruePin);

	if (TruePin->IsStruct())
	{
		AddPinsForStruct(TruePin->GetScriptStruct(), Node, TruePin, TruePin->Direction, FString(), false);
	}

	URigVMPin* FalsePin = NewObject<URigVMPin>(Node, *URigVMIfNode::FalseName);
	FalsePin->CPPType = CPPType;
	FalsePin->CPPTypeObject = CPPTypeObject;
	FalsePin->CPPTypeObjectPath = InCPPTypeObjectPath;
	FalsePin->Direction = ERigVMPinDirection::Input;
	FalsePin->DefaultValue = DefaultValue;
	Node->Pins.Add(FalsePin);

	if (FalsePin->IsStruct())
	{
		AddPinsForStruct(FalsePin->GetScriptStruct(), Node, FalsePin, FalsePin->Direction, FString(), false);
	}

	URigVMPin* ResultPin = NewObject<URigVMPin>(Node, *URigVMIfNode::ResultName);
	ResultPin->CPPType = CPPType;
	ResultPin->CPPTypeObject = CPPTypeObject;
	ResultPin->CPPTypeObjectPath = InCPPTypeObjectPath;
	ResultPin->Direction = ERigVMPinDirection::Output;
	Node->Pins.Add(ResultPin);

	if (ResultPin->IsStruct())
	{
		AddPinsForStruct(ResultPin->GetScriptStruct(), Node, ResultPin, ResultPin->Direction, FString(), false);
	}

	Graph->Nodes.Add(Node);

	Notify(ERigVMGraphNotifType::NodeAdded, Node);

	if (bSetupUndoRedo)
	{
		ActionStack->AddAction(FRigVMAddIfNodeAction(Node));
	}

	return Node;
}

URigVMSelectNode* URigVMController::AddSelectNode(const FString& InCPPType, const FName& InCPPTypeObjectPath, const FVector2D& InPosition, const FString& InNodeName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	ensure(!InCPPType.IsEmpty());

	FString CPPType = InCPPType;
	UObject* CPPTypeObject = nullptr;
	if (!InCPPTypeObjectPath.IsNone())
	{
		CPPTypeObject = URigVMPin::FindObjectFromCPPTypeObjectPath<UObject>(InCPPTypeObjectPath.ToString());
		if (CPPTypeObject == nullptr)
		{
			ReportErrorf(TEXT("Cannot find cpp type object for path '%s'."), *InCPPTypeObjectPath.ToString());
			return nullptr;
		}
	}

	FString DefaultValue;
	if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(CPPTypeObject))
	{
		if (ScriptStruct->IsChildOf(FRigVMExecuteContext::StaticStruct()))
		{
			ReportErrorf(TEXT("Cannot create a select node for this type '%s'."), *InCPPTypeObjectPath.ToString());
			return nullptr;
		}
		CreateDefaultValueForStructIfRequired(ScriptStruct, DefaultValue);

		CPPType = ScriptStruct->GetStructCPPName();
	}

	FString Name = GetValidNodeName(InNodeName.IsEmpty() ? FString(TEXT("IfNode")) : InNodeName);
	URigVMSelectNode* Node = NewObject<URigVMSelectNode>(Graph, *Name);
	Node->Position = InPosition;

	URigVMPin* IndexPin = NewObject<URigVMPin>(Node, *URigVMSelectNode::IndexName);
	IndexPin->CPPType = TEXT("int32");
	IndexPin->Direction = ERigVMPinDirection::Input;
	Node->Pins.Add(IndexPin);

	URigVMPin* ValuePin = NewObject<URigVMPin>(Node, *URigVMSelectNode::ValueName);
	ValuePin->CPPType = FString::Printf(TEXT("TArray<%s>"), *CPPType);
	ValuePin->CPPTypeObject = CPPTypeObject;
	ValuePin->CPPTypeObjectPath = InCPPTypeObjectPath;
	ValuePin->Direction = ERigVMPinDirection::Input;
	ValuePin->bIsExpanded = true;
	Node->Pins.Add(ValuePin);

	URigVMPin* ResultPin = NewObject<URigVMPin>(Node, *URigVMSelectNode::ResultName);
	ResultPin->CPPType = CPPType;
	ResultPin->CPPTypeObject = CPPTypeObject;
	ResultPin->CPPTypeObjectPath = InCPPTypeObjectPath;
	ResultPin->Direction = ERigVMPinDirection::Output;
	Node->Pins.Add(ResultPin);

	Graph->Nodes.Add(Node);

	Notify(ERigVMGraphNotifType::NodeAdded, Node);

	SetArrayPinSize(ValuePin->GetPinPath(), 2, DefaultValue, false);

	if (bSetupUndoRedo)
	{
		ActionStack->AddAction(FRigVMAddSelectNodeAction(Node));
	}

	return Node;
}

URigVMPrototypeNode* URigVMController::AddPrototypeNode(const FName& InNotation, const FVector2D& InPosition, const FString& InNodeName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	ensure(!InNotation.IsNone());

	const FRigVMPrototype* Prototype = FRigVMRegistry::Get().FindPrototype(InNotation);
	if (Prototype == nullptr)
	{
		ReportErrorf(TEXT("Prototype '%s' cannot be found."), *InNotation.ToString());
		return nullptr;
	}

	FString Name = GetValidNodeName(InNodeName.IsEmpty() ? Prototype->GetName().ToString() : InNodeName);
	URigVMPrototypeNode* Node = NewObject<URigVMPrototypeNode>(Graph, *Name);
	Node->PrototypeNotation = Prototype->GetNotation();
	Node->Position = InPosition;

	int32 FunctionIndex = INDEX_NONE;
	FRigVMPrototype::FTypeMap Types;
	Prototype->Resolve(Types, FunctionIndex);

	for (int32 ArgIndex = 0; ArgIndex < Prototype->NumArgs(); ArgIndex++)
	{
		const FRigVMPrototypeArg* Arg = Prototype->GetArg(ArgIndex);

		URigVMPin* Pin = NewObject<URigVMPin>(Node, Arg->GetName());
		const FRigVMPrototypeArg::FType& Type = Types.FindChecked(Arg->GetName());
		Pin->CPPType = Type.CPPType;
		Pin->CPPTypeObject = Type.CPPTypeObject;
		if (Pin->CPPTypeObject)
		{
			Pin->CPPTypeObjectPath = *Pin->CPPTypeObject->GetPathName();
		}
		Pin->Direction = Arg->GetDirection();
		Node->Pins.Add(Pin);
	}

	Graph->Nodes.Add(Node);

	Notify(ERigVMGraphNotifType::NodeAdded, Node);

	if (bSetupUndoRedo)
	{
		ActionStack->AddAction(FRigVMAddPrototypeNodeAction(Node));
	}

	return Node;
}


URigVMEnumNode* URigVMController::AddEnumNode(const FName& InCPPTypeObjectPath, const FVector2D& InPosition, const FString& InNodeName, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

    UObject* CPPTypeObject = URigVMPin::FindObjectFromCPPTypeObjectPath<UObject>(InCPPTypeObjectPath.ToString());
	if (CPPTypeObject == nullptr)
	{
		ReportErrorf(TEXT("Cannot find cpp type object for path '%s'."), *InCPPTypeObjectPath.ToString());
		return nullptr;
	}

	UEnum* Enum = Cast<UEnum>(CPPTypeObject);
	if(Enum == nullptr)
	{
		ReportErrorf(TEXT("Cpp type object for path '%s' is not an enum."), *InCPPTypeObjectPath.ToString());
		return nullptr;
	}

	FString Name = GetValidNodeName(InNodeName.IsEmpty() ? FString(TEXT("IfNode")) : InNodeName);
	URigVMEnumNode* Node = NewObject<URigVMEnumNode>(Graph, *Name);
	Node->Position = InPosition;

	URigVMPin* EnumValuePin = NewObject<URigVMPin>(Node, *URigVMEnumNode::EnumValueName);
	EnumValuePin->CPPType = CPPTypeObject->GetName();
	EnumValuePin->CPPTypeObject = CPPTypeObject;
	EnumValuePin->CPPTypeObjectPath = InCPPTypeObjectPath;
	EnumValuePin->Direction = ERigVMPinDirection::Visible;
	EnumValuePin->DefaultValue = Enum->GetNameStringByValue(0);
	Node->Pins.Add(EnumValuePin);

	URigVMPin* EnumIndexPin = NewObject<URigVMPin>(Node, *URigVMEnumNode::EnumIndexName);
	EnumIndexPin->CPPType = TEXT("int32");
	EnumIndexPin->Direction = ERigVMPinDirection::Output;
	EnumIndexPin->DisplayName = TEXT("Result");
	Node->Pins.Add(EnumIndexPin);

	Graph->Nodes.Add(Node);

	Notify(ERigVMGraphNotifType::NodeAdded, Node);

	if (bSetupUndoRedo)
	{
		ActionStack->AddAction(FRigVMAddEnumNodeAction(Node));
	}

	return Node;
}

void URigVMController::ForEveryPinRecursively(URigVMPin* InPin, TFunction<void(URigVMPin*)> OnEachPinFunction)
{
	OnEachPinFunction(InPin);
	for (URigVMPin* SubPin : InPin->SubPins)
	{
		ForEveryPinRecursively(SubPin, OnEachPinFunction);
	}
}

void URigVMController::ForEveryPinRecursively(URigVMNode* InNode, TFunction<void(URigVMPin*)> OnEachPinFunction)
{
	for (URigVMPin* Pin : InNode->GetPins())
	{
		ForEveryPinRecursively(Pin, OnEachPinFunction);
	}
}

void URigVMController::SetExecuteContextStruct(UStruct* InExecuteContextStruct)
{
	check(InExecuteContextStruct);
	ensure(InExecuteContextStruct->IsChildOf(FRigVMExecuteContext::StaticStruct()));
	ExecuteContextStruct = InExecuteContextStruct;
}

FString URigVMController::GetValidNodeName(const FString& InPrefix)
{
	URigVMGraph* Graph = GetGraph();
	check(Graph);

	return GetUniqueName(*InPrefix, [&](const FName& InName) {
		return Graph->IsNameAvailable(InName.ToString());
	}).ToString();
}

bool URigVMController::IsValidGraph()
{
	URigVMGraph* Graph = GetGraph();
	if (Graph == nullptr)
	{
		ReportError(TEXT("Controller does not have a graph associated - use SetGraph / set_graph."));
		return false;
	}
	return true;
}

bool URigVMController::IsValidNodeForGraph(URigVMNode* InNode)
{
	if(!IsValidGraph())
	{
		return false;
	}

	if (InNode == nullptr)
	{
		ReportError(TEXT("InNode is nullptr."));
		return false;
	}

	if (InNode->GetGraph() != GetGraph())
	{
		ReportWarningf(TEXT("InNode '%s' is on a different graph. InNode graph is %s, this graph is %s"), *InNode->GetNodePath(), *GetNameSafe(InNode->GetGraph()), *GetNameSafe(GetGraph()));
		return false;
	}

	if (InNode->GetNodeIndex() == INDEX_NONE)
	{
		ReportErrorf(TEXT("InNode '%s' is transient (not yet nested to a graph)."), *InNode->GetName());
	}

	return true;
}

bool URigVMController::IsValidPinForGraph(URigVMPin* InPin)
{
	if(!IsValidGraph())
	{
		return false;
	}

	if (InPin == nullptr)
	{
		ReportError(TEXT("InPin is nullptr."));
		return false;
	}

	if (!IsValidNodeForGraph(InPin->GetNode()))
	{
		return false;
	}

	if (InPin->GetPinIndex() == INDEX_NONE)
	{
		ReportErrorf(TEXT("InPin '%s' is transient (not yet nested properly)."), *InPin->GetName());
	}

	return true;
}

bool URigVMController::IsValidLinkForGraph(URigVMLink* InLink)
{
	if(!IsValidGraph())
	{
		return false;
	}

	if (InLink == nullptr)
	{
		ReportError(TEXT("InLink is nullptr."));
		return false;
	}

	if (InLink->GetGraph() != GetGraph())
	{
		ReportError(TEXT("InLink is on a different graph."));
		return false;
	}

	if(InLink->GetSourcePin() == nullptr)
	{
		ReportError(TEXT("InLink has no source pin."));
		return false;
	}

	if(InLink->GetTargetPin() == nullptr)
	{
		ReportError(TEXT("InLink has no target pin."));
		return false;
	}

	if (InLink->GetLinkIndex() == INDEX_NONE)
	{
		ReportError(TEXT("InLink is transient (not yet nested properly)."));
	}

	if(!IsValidPinForGraph(InLink->GetSourcePin()))
	{
		return false;
	}

	if(!IsValidPinForGraph(InLink->GetTargetPin()))
	{
		return false;
	}

	return true;
}

bool URigVMController::CanAddNode(URigVMNode* InNode, bool bReportErrors)
{
	if(!IsValidGraph())
	{
		return false;
	}

	check(InNode);

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (URigVMFunctionReferenceNode* FunctionRefNode = Cast<URigVMFunctionReferenceNode>(InNode))
	{
		if (URigVMFunctionLibrary* FunctionLibrary = FunctionRefNode->GetLibrary())
		{
			if(Graph->GetDefaultFunctionLibrary() != FunctionLibrary)
			{
				// this node references a function that is not part of our function library
				if(bReportErrors)
				{
					ReportError(TEXT("Cannot import function reference node."));
				}
				DestroyObject(InNode);
				return false;
			}
			else if (URigVMLibraryNode* FunctionDefinition = FunctionRefNode->GetReferencedNode())
			{
				if(!CanAddFunctionRefForDefinition(FunctionDefinition, bReportErrors))
				{
					DestroyObject(InNode);
					return false;
				}
			}
		}			
	}

	return true;
}

bool URigVMController::CanAddFunctionRefForDefinition(URigVMLibraryNode* InFunctionDefinition, bool bReportErrors)
{
	if(!IsValidGraph())
	{
		return false;
	}

	check(InFunctionDefinition);

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMLibraryNode* ParentLibraryNode = Cast<URigVMLibraryNode>(Graph->GetOuter());
	while (ParentLibraryNode)
	{
		if (ParentLibraryNode == InFunctionDefinition)
		{
			if(bReportErrors)
			{
				ReportAndNotifyError(TEXT("You cannot place functions inside of itself or an indirect recursion."));
			}
			return false;
		}
		ParentLibraryNode = Cast<URigVMLibraryNode>(ParentLibraryNode->GetGraph()->GetOuter());
	}

	return true;
}

void URigVMController::AddPinsForStruct(UStruct* InStruct, URigVMNode* InNode, URigVMPin* InParentPin, ERigVMPinDirection InPinDirection, const FString& InDefaultValue, bool bAutoExpandArrays, bool bNotify)
{
	TArray<FString> MemberNameValuePairs = URigVMPin::SplitDefaultValue(InDefaultValue);
	TMap<FName, FString> MemberValues;
	for (const FString& MemberNameValuePair : MemberNameValuePairs)
	{
		FString MemberName, MemberValue;
		if (MemberNameValuePair.Split(TEXT("="), &MemberName, &MemberValue))
		{
			MemberValues.Add(*MemberName, MemberValue);
		}
	}

	for (TFieldIterator<FProperty> It(InStruct); It; ++It)
	{
		FName PropertyName = It->GetFName();

		URigVMPin* Pin = NewObject<URigVMPin>(InParentPin == nullptr ? Cast<UObject>(InNode) : Cast<UObject>(InParentPin), PropertyName);
		ConfigurePinFromProperty(*It, Pin, InPinDirection);

		if (InParentPin)
		{
			InParentPin->SubPins.Add(Pin);
		}
		else
		{
			InNode->Pins.Add(Pin);
		}

		FString* DefaultValuePtr = MemberValues.Find(Pin->GetFName());

		FStructProperty* StructProperty = CastField<FStructProperty>(*It);
		if (StructProperty)
		{
			if (ShouldStructBeUnfolded(StructProperty->Struct))
			{
				FString DefaultValue;
				if (DefaultValuePtr != nullptr)
				{
					DefaultValue = *DefaultValuePtr;
				}
				CreateDefaultValueForStructIfRequired(StructProperty->Struct, DefaultValue);

				AddPinsForStruct(StructProperty->Struct, InNode, Pin, Pin->GetDirection(), DefaultValue, bAutoExpandArrays);
			}
			else if(DefaultValuePtr != nullptr)
			{
				Pin->DefaultValue = *DefaultValuePtr;
			}
		}

		FArrayProperty* ArrayProperty = CastField<FArrayProperty>(*It);
		if (ArrayProperty)
		{
			ensure(Pin->IsArray());

			if (DefaultValuePtr)
			{
				if (ShouldPinBeUnfolded(Pin))
				{
					TArray<FString> ElementDefaultValues = URigVMPin::SplitDefaultValue(*DefaultValuePtr);
					AddPinsForArray(ArrayProperty, InNode, Pin, Pin->Direction, ElementDefaultValues, bAutoExpandArrays);
				}
				else
				{
					FString DefaultValue = *DefaultValuePtr;
					PostProcessDefaultValue(Pin, DefaultValue);
					Pin->DefaultValue = *DefaultValuePtr;
				}
			}
		}
		
		if (!Pin->IsArray() && !Pin->IsStruct() && DefaultValuePtr != nullptr)
		{
			FString DefaultValue = *DefaultValuePtr;
			PostProcessDefaultValue(Pin, DefaultValue);
			Pin->DefaultValue = DefaultValue;
		}

		if (bNotify)
		{
			Notify(ERigVMGraphNotifType::PinAdded, Pin);
		}
	}
}

void URigVMController::AddPinsForArray(FArrayProperty* InArrayProperty, URigVMNode* InNode, URigVMPin* InParentPin, ERigVMPinDirection InPinDirection, const TArray<FString>& InDefaultValues, bool bAutoExpandArrays)
{
	check(InParentPin);
	if (!ShouldPinBeUnfolded(InParentPin))
	{
		return;
	}

	for (int32 ElementIndex = 0; ElementIndex < InDefaultValues.Num(); ElementIndex++)
	{
		FString ElementName = FString::FormatAsNumber(InParentPin->SubPins.Num());
		URigVMPin* Pin = NewObject<URigVMPin>(InParentPin, *ElementName);

		ConfigurePinFromProperty(InArrayProperty->Inner, Pin, InPinDirection);
		FString DefaultValue = InDefaultValues[ElementIndex];

		InParentPin->SubPins.Add(Pin);

		if (bAutoExpandArrays)
		{
			TGuardValue<bool> ErrorGuard(bReportWarningsAndErrors, false);
			ExpandPinRecursively(Pin, false);
		}

		FStructProperty* StructProperty = CastField<FStructProperty>(InArrayProperty->Inner);
		if (StructProperty)
		{
			if (ShouldPinBeUnfolded(Pin))
			{
				// DefaultValue before this point only contains parent struct overrides,
				// see comments in CreateDefaultValueForStructIfRequired
				UScriptStruct* ScriptStruct = Pin->GetScriptStruct();
				if (ScriptStruct)
				{
					CreateDefaultValueForStructIfRequired(ScriptStruct, DefaultValue);
				}
				AddPinsForStruct(StructProperty->Struct, InNode, Pin, Pin->Direction, DefaultValue, bAutoExpandArrays);
			}
			else if (!DefaultValue.IsEmpty())
			{
				PostProcessDefaultValue(Pin, DefaultValue);
				Pin->DefaultValue = DefaultValue;
			}
		}

		FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InArrayProperty->Inner);
		if (ArrayProperty)
		{
			if (ShouldPinBeUnfolded(Pin))
			{
				TArray<FString> ElementDefaultValues = URigVMPin::SplitDefaultValue(DefaultValue);
				AddPinsForArray(ArrayProperty, InNode, Pin, Pin->Direction, ElementDefaultValues, bAutoExpandArrays);
			}
			else if (!DefaultValue.IsEmpty())
			{
				PostProcessDefaultValue(Pin, DefaultValue);
				Pin->DefaultValue = DefaultValue;
			}
		}

		if (!Pin->IsArray() && !Pin->IsStruct())
		{
			PostProcessDefaultValue(Pin, DefaultValue);
			Pin->DefaultValue = DefaultValue;
		}
	}
}

void URigVMController::ConfigurePinFromProperty(FProperty* InProperty, URigVMPin* InOutPin, ERigVMPinDirection InPinDirection)
{
	if (InPinDirection == ERigVMPinDirection::Invalid)
	{
		InOutPin->Direction = FRigVMStruct::GetPinDirectionFromProperty(InProperty);
	}
	else
	{
		InOutPin->Direction = InPinDirection;
	}

#if WITH_EDITOR

	if (!InOutPin->IsArrayElement())
	{
		FString DisplayNameText = InProperty->GetDisplayNameText().ToString();
		if (!DisplayNameText.IsEmpty())
		{
			InOutPin->DisplayName = *DisplayNameText;
		}
		else
		{
			InOutPin->DisplayName = NAME_None;
		}
	}
	InOutPin->bIsConstant = InProperty->HasMetaData(TEXT("Constant"));
	FString CustomWidgetName = InProperty->GetMetaData(TEXT("CustomWidget"));
	InOutPin->CustomWidgetName = CustomWidgetName.IsEmpty() ? FName(NAME_None) : FName(*CustomWidgetName);

	if (InProperty->HasMetaData(FRigVMStruct::ExpandPinByDefaultMetaName))
	{
		InOutPin->bIsExpanded = true;
	}

#endif

	FString ExtendedCppType;
	InOutPin->CPPType = InProperty->GetCPPType(&ExtendedCppType);
	InOutPin->CPPType += ExtendedCppType;

	InOutPin->bIsDynamicArray = false;
#if WITH_EDITOR
	if (InOutPin->Direction == ERigVMPinDirection::Hidden)
	{
		if (!InProperty->HasMetaData(TEXT("ArraySize")))
		{
			InOutPin->bIsDynamicArray = true;
		}
	}

	if (InOutPin->bIsDynamicArray)
	{
		if (InProperty->HasMetaData(FRigVMStruct::SingletonMetaName))
		{
			InOutPin->bIsDynamicArray = false;
		}
	}
#endif

	FProperty* PropertyForType = InProperty;
	FArrayProperty* ArrayProperty = CastField<FArrayProperty>(PropertyForType);
	if (ArrayProperty)
	{
		PropertyForType = ArrayProperty->Inner;
	}

	if (FStructProperty* StructProperty = CastField<FStructProperty>(PropertyForType))
	{
		InOutPin->CPPTypeObject = StructProperty->Struct;
	}
	else if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(PropertyForType))
	{
		InOutPin->CPPTypeObject = EnumProperty->GetEnum();
	}
	else if (FByteProperty* ByteProperty = CastField<FByteProperty>(PropertyForType))
	{
		InOutPin->CPPTypeObject = ByteProperty->Enum;
	}

	if (InOutPin->CPPTypeObject)
	{
		InOutPin->CPPTypeObjectPath = *InOutPin->CPPTypeObject->GetPathName();
	}
}

void URigVMController::ConfigurePinFromPin(URigVMPin* InOutPin, URigVMPin* InPin)
{
	InOutPin->bIsConstant = InPin->bIsConstant;
	InOutPin->Direction = InPin->Direction;
	InOutPin->CPPType = InPin->CPPType;
	InOutPin->CPPTypeObjectPath = InPin->CPPTypeObjectPath;
	InOutPin->CPPTypeObject = InPin->CPPTypeObject;
	InOutPin->DefaultValue = InPin->DefaultValue;
}

bool URigVMController::ShouldStructBeUnfolded(const UStruct* Struct)
{
	if (Struct == nullptr)
	{
		return false;
	}
	if (Struct->IsChildOf(UClass::StaticClass()))
	{
		return false;
	}
	if(Struct->IsChildOf(FRigVMExecuteContext::StaticStruct()))
	{
		return false;
	}
	if (UnfoldStructDelegate.IsBound())
	{
		if (!UnfoldStructDelegate.Execute(Struct))
		{
			return false;
		}
	}
	return true;
}

bool URigVMController::ShouldPinBeUnfolded(URigVMPin* InPin)
{
	if (InPin->IsStruct())
	{
		return ShouldStructBeUnfolded(InPin->GetScriptStruct());
	}
	else if (InPin->IsArray())
	{
		return InPin->GetDirection() == ERigVMPinDirection::Input ||
			InPin->GetDirection() == ERigVMPinDirection::IO;
	}
	return false;
}

FProperty* URigVMController::FindPropertyForPin(const FString& InPinPath)
{
	if(!IsValidGraph())
	{
		return nullptr;
	}

	TArray<FString> Parts;
	if (!URigVMPin::SplitPinPath(InPinPath, Parts))
	{
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMPin* Pin = Graph->FindPin(InPinPath);
	if (Pin == nullptr)
	{
		ReportErrorf(TEXT("Cannot find pin '%s'."), *InPinPath);
		return nullptr;
	}

	URigVMNode* Node = Pin->GetNode();

	URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(Node);
	if (UnitNode)
	{
		int32 PartIndex = 1; // cut off the first one since it's the node

		UStruct* Struct = UnitNode->ScriptStruct;
		FProperty* Property = Struct->FindPropertyByName(*Parts[PartIndex++]);

		while (PartIndex < Parts.Num() && Property != nullptr)
		{
			if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
			{
				Property = ArrayProperty->Inner;
				PartIndex++;
				continue;
			}

			if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				Struct = StructProperty->Struct;
				Property = Struct->FindPropertyByName(*Parts[PartIndex++]);
				continue;
			}

			break;
		}

		if (PartIndex == Parts.Num())
		{
			return Property;
		}
	}

	return nullptr;
}

int32 URigVMController::DetachLinksFromPinObjects(const TArray<URigVMLink*>* InLinks, bool bNotify)
{
	URigVMGraph* Graph = GetGraph();
	check(Graph);
	TGuardValue<bool> EventuallySuspendNotifs(bSuspendNotifications, !bNotify);

	TArray<URigVMLink*> Links;
	if (InLinks)
	{
		Links = *InLinks;
	}
	else
	{
		Links = Graph->Links;
	}

	for (URigVMLink* Link : Links)
	{
		Notify(ERigVMGraphNotifType::LinkRemoved, Link);

		URigVMPin* SourcePin = Link->GetSourcePin();
		URigVMPin* TargetPin = Link->GetTargetPin();

		if (SourcePin)
		{
			Link->SourcePinPath = SourcePin->GetPinPath();
			SourcePin->Links.Remove(Link);
		}

		if (TargetPin)
		{
			Link->TargetPinPath = TargetPin->GetPinPath();
			TargetPin->Links.Remove(Link);
		}

		Link->SourcePin = nullptr;
		Link->TargetPin = nullptr;
	}

	if (InLinks == nullptr)
	{
		for (URigVMNode* Node : Graph->Nodes)
		{
			if (URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(Node))
			{
				FRigVMControllerGraphGuard GraphGuard(this, CollapseNode->GetContainedGraph(), false);
				DetachLinksFromPinObjects(InLinks, bNotify);
			}
		}
	}

	return Links.Num();
}

int32 URigVMController::ReattachLinksToPinObjects(bool bFollowCoreRedirectors, const TArray<URigVMLink*>* InLinks, bool bNotify)
{
	URigVMGraph* Graph = GetGraph();
	check(Graph);
	TGuardValue<bool> EventuallySuspendNotifs(bSuspendNotifications, !bNotify);
	FScopeLock Lock(&PinPathCoreRedirectorsLock);

	bool bReplacingAllLinks = false;
	TArray<URigVMLink*> Links;
	if (InLinks)
	{
		Links = *InLinks;
	}
	else
	{
		Links = Graph->Links;
		bReplacingAllLinks = true;
	}

	TMap<FString, FString> RedirectedPinPaths;
	if (bFollowCoreRedirectors)
	{
		for (URigVMLink* Link : Links)
		{
			FString RedirectedSourcePinPath;
			if (ShouldRedirectPin(Link->SourcePinPath, RedirectedSourcePinPath))
			{
				OutputPinRedirectors.FindOrAdd(Link->SourcePinPath, RedirectedSourcePinPath);
			}

			FString RedirectedTargetPinPath;
			if (ShouldRedirectPin(Link->TargetPinPath, RedirectedTargetPinPath))
			{
				InputPinRedirectors.FindOrAdd(Link->TargetPinPath, RedirectedTargetPinPath);
			}
		}
	}

	// fix up the pin links based on the persisted data
	TArray<URigVMLink*> NewLinks;
	for (URigVMLink* Link : Links)
	{
		if (FString* RedirectedSourcePinPath = OutputPinRedirectors.Find(Link->SourcePinPath))
		{
			ensure(Link->SourcePin == nullptr);
			Link->SourcePinPath = *RedirectedSourcePinPath;
		}

		if (FString* RedirectedTargetPinPath = InputPinRedirectors.Find(Link->TargetPinPath))
		{
			ensure(Link->TargetPin == nullptr);
			Link->TargetPinPath = *RedirectedTargetPinPath;
		}

		URigVMPin* SourcePin = Link->GetSourcePin();
		URigVMPin* TargetPin = Link->GetTargetPin();
		if (SourcePin == nullptr)
		{
			ReportWarningf(TEXT("Unable to re-create link %s -> %s"), *Link->SourcePinPath, *Link->TargetPinPath);
			if (TargetPin != nullptr)
			{
				TargetPin->Links.Remove(Link);
			}
			continue;
		}
		if (TargetPin == nullptr)
		{
			ReportWarningf(TEXT("Unable to re-create link %s -> %s"), *Link->SourcePinPath, *Link->TargetPinPath);
			if (SourcePin != nullptr)
			{
				SourcePin->Links.Remove(Link);
			}
			continue;
		}

		SourcePin->Links.AddUnique(Link);
		TargetPin->Links.AddUnique(Link);
		NewLinks.Add(Link);
	}

	if (bReplacingAllLinks)
	{
		Graph->Links = NewLinks;

		for (URigVMLink* Link : Graph->Links)
		{
			Notify(ERigVMGraphNotifType::LinkAdded, Link);
		}
	}
	else
	{
		// if we are running of a subset of links
		// find the ones we weren't able to connect
		// again and remove them.
		for (URigVMLink* Link : Links)
		{
			if (!NewLinks.Contains(Link))
			{
				Graph->Links.Remove(Link);
				Notify(ERigVMGraphNotifType::LinkRemoved, Link);
			}
			else
			{
				Notify(ERigVMGraphNotifType::LinkAdded, Link);
			}
		}
	}

	if (InLinks == nullptr)
	{
		for (URigVMNode* Node : Graph->Nodes)
		{
			if (URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(Node))
			{
				FRigVMControllerGraphGuard GraphGuard(this, CollapseNode->GetContainedGraph(), false);
				ReattachLinksToPinObjects(bFollowCoreRedirectors, nullptr);
			}
		}
	}

	InputPinRedirectors.Reset();
	OutputPinRedirectors.Reset();

	return NewLinks.Num();
}

void URigVMController::RemoveStaleNodes()
{
	if (!IsValidGraph())
	{
		return;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	Graph->Nodes.Remove(nullptr);
}

void URigVMController::AddPinRedirector(bool bInput, bool bOutput, const FString& OldPinPath, const FString& NewPinPath)
{
	if (OldPinPath.IsEmpty() || NewPinPath.IsEmpty() || OldPinPath == NewPinPath)
	{
		return;
	}

	if (bInput)
	{
		InputPinRedirectors.FindOrAdd(OldPinPath) = NewPinPath;
	}
	if (bOutput)
	{
		OutputPinRedirectors.FindOrAdd(OldPinPath) = NewPinPath;
	}
}

#if WITH_EDITOR

bool URigVMController::ShouldRedirectPin(UScriptStruct* InOwningStruct, const FString& InOldRelativePinPath, FString& InOutNewRelativePinPath) const
{
	FControlRigStructPinRedirectorKey RedirectorKey(InOwningStruct, InOldRelativePinPath);
	if (const FString* RedirectedPinPath = PinPathCoreRedirectors.Find(RedirectorKey))
	{
		InOutNewRelativePinPath = *RedirectedPinPath;
		return InOutNewRelativePinPath != InOldRelativePinPath;
	}

	FString RelativePinPath = InOldRelativePinPath;
	FString PinName, SubPinPath;
	if (!URigVMPin::SplitPinPathAtStart(RelativePinPath, PinName, SubPinPath))
	{
		PinName = RelativePinPath;
		SubPinPath.Empty();
	}

	bool bShouldRedirect = false;
	FCoreRedirectObjectName OldObjectName(*PinName, InOwningStruct->GetFName(), *InOwningStruct->GetOutermost()->GetPathName());
	FCoreRedirectObjectName NewObjectName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Property, OldObjectName);
	if (OldObjectName != NewObjectName)
	{
		PinName = NewObjectName.ObjectName.ToString();
		bShouldRedirect = true;
	}

	FProperty* Property = InOwningStruct->FindPropertyByName(*PinName);
	if (Property == nullptr)
	{
		return false;
	}

	if (!SubPinPath.IsEmpty())
	{
		if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			FString NewSubPinPath;
			if (ShouldRedirectPin(StructProperty->Struct, SubPinPath, NewSubPinPath))
			{
				SubPinPath = NewSubPinPath;
				bShouldRedirect = true;
			}
		}
		else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			FString SubPinName, SubSubPinPath;
			if (URigVMPin::SplitPinPathAtStart(SubPinPath, SubPinName, SubSubPinPath))
			{
				if (FStructProperty* InnerStructProperty = CastField<FStructProperty>(ArrayProperty->Inner))
				{
					FString NewSubSubPinPath;
					if (ShouldRedirectPin(InnerStructProperty->Struct, SubSubPinPath, NewSubSubPinPath))
					{
						SubSubPinPath = NewSubSubPinPath;
						SubPinPath = URigVMPin::JoinPinPath(SubPinName, SubSubPinPath);
						bShouldRedirect = true;
					}
				}
			}
		}
	}

	if (bShouldRedirect)
	{
		if (SubPinPath.IsEmpty())
		{
			InOutNewRelativePinPath = PinName;
			PinPathCoreRedirectors.Add(RedirectorKey, InOutNewRelativePinPath);
		}
		else
		{
			InOutNewRelativePinPath = URigVMPin::JoinPinPath(PinName, SubPinPath);

			TArray<FString> OldParts, NewParts;
			if (URigVMPin::SplitPinPath(InOldRelativePinPath, OldParts) &&
				URigVMPin::SplitPinPath(InOutNewRelativePinPath, NewParts))
			{
				ensure(OldParts.Num() == NewParts.Num());

				FString OldPath = OldParts[0];
				FString NewPath = NewParts[0];
				for (int32 PartIndex = 0; PartIndex < OldParts.Num(); PartIndex++)
				{
					if (PartIndex > 0)
					{
						OldPath = URigVMPin::JoinPinPath(OldPath, OldParts[PartIndex]);
						NewPath = URigVMPin::JoinPinPath(NewPath, NewParts[PartIndex]);
					}

					// this is also going to cache paths which haven't been redirected.
					// consumers of the table have to still compare old != new
					FControlRigStructPinRedirectorKey SubRedirectorKey(InOwningStruct, OldPath);
					if (!PinPathCoreRedirectors.Contains(SubRedirectorKey))
					{
						PinPathCoreRedirectors.Add(SubRedirectorKey, NewPath);
					}
				}
			}
		}
	}

	return bShouldRedirect;
}

bool URigVMController::ShouldRedirectPin(const FString& InOldPinPath, FString& InOutNewPinPath) const
{
	URigVMGraph* Graph = GetGraph();
	check(Graph);

	FString PinPathInNode, NodeName;
	URigVMPin::SplitPinPathAtStart(InOldPinPath, NodeName, PinPathInNode);

	URigVMNode* Node = Graph->FindNode(NodeName);
	if (URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(Node))
	{
		FString NewPinPathInNode;
		if (ShouldRedirectPin(UnitNode->GetScriptStruct(), PinPathInNode, NewPinPathInNode))
		{
			InOutNewPinPath = URigVMPin::JoinPinPath(NodeName, NewPinPathInNode);
			return true;
		}
	}
	else if (URigVMRerouteNode* RerouteNode = Cast<URigVMRerouteNode>(Node))
	{
		URigVMPin* ValuePin = RerouteNode->Pins[0];
		if (ValuePin->IsStruct())
		{
			FString ValuePinPath = ValuePin->GetPinPath();
			if (InOldPinPath == ValuePinPath)
			{
				return false;
			}
			else if (!InOldPinPath.StartsWith(ValuePinPath))
			{
				return false;
			}

			FString PinPathInStruct, NewPinPathInStruct;
			if (URigVMPin::SplitPinPathAtStart(PinPathInNode, NodeName, PinPathInStruct))
			{
				if (ShouldRedirectPin(ValuePin->GetScriptStruct(), PinPathInStruct, NewPinPathInStruct))
				{
					InOutNewPinPath = URigVMPin::JoinPinPath(ValuePin->GetPinPath(), NewPinPathInStruct);
					return true;
				}
			}
		}
	}

	return false;
}

void URigVMController::RepopulatePinsOnNode(URigVMNode* InNode, bool bFollowCoreRedirectors, bool bNotify)
{
	if (InNode == nullptr)
	{
		ReportError(TEXT("InNode is nullptr."));
		return;
	}

	URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(InNode);
	URigVMRerouteNode* RerouteNode = Cast<URigVMRerouteNode>(InNode);
	URigVMFunctionEntryNode* EntryNode = Cast<URigVMFunctionEntryNode>(InNode);
	URigVMFunctionReturnNode* ReturnNode = Cast<URigVMFunctionReturnNode>(InNode);
	URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(InNode);
	URigVMFunctionReferenceNode* FunctionRefNode = Cast<URigVMFunctionReferenceNode>(InNode);

	TGuardValue<bool> EventuallySuspendNotifs(bSuspendNotifications, !bNotify);
	FScopeLock Lock(&PinPathCoreRedirectorsLock);

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	// step 1/3: keep a record of the current state of the node's pins
	TMap<FString, FString> RedirectedPinPaths;
	if (bFollowCoreRedirectors)
	{
		RedirectedPinPaths = GetRedirectedPinPaths(InNode);
	}
	TMap<FString, FPinState> PinStates = GetPinStates(InNode);

	// also in case this node is part of an injection
	FName InjectionInputPinName = NAME_None;
	FName InjectionOutputPinName = NAME_None;
	if (URigVMInjectionInfo* InjectionInfo = InNode->GetInjectionInfo())
	{
		InjectionInputPinName = InjectionInfo->InputPin->GetFName();
		InjectionOutputPinName = InjectionInfo->OutputPin->GetFName();
	}

	// step 2/3: clear pins on the node and repopulate the node with new pins
	if (UnitNode != nullptr)
	{
		TArray<URigVMPin*> Pins = InNode->GetPins();
		for (URigVMPin* Pin : Pins)
		{
			RemovePin(Pin, false, bNotify);
		}
		InNode->Pins.Reset();
		Pins.Reset();

		UScriptStruct* ScriptStruct = UnitNode->GetScriptStruct();
		if (ScriptStruct == nullptr)
		{
			ReportWarningf(
				TEXT("Control Rig '%s', Node '%s' has no struct assigned. Do you have a broken redirect?"),
				*UnitNode->GetOutermost()->GetPathName(),
				*UnitNode->GetName()
				);

			RemoveNode(UnitNode, false, true);
			return;
		}

		FString NodeColorMetadata;
		ScriptStruct->GetStringMetaDataHierarchical(*URigVMNode::NodeColorName, &NodeColorMetadata);
		if (!NodeColorMetadata.IsEmpty())
		{
			UnitNode->NodeColor = GetColorFromMetadata(NodeColorMetadata);
		}

		FString ExportedDefaultValue;
		CreateDefaultValueForStructIfRequired(ScriptStruct, ExportedDefaultValue);
		AddPinsForStruct(ScriptStruct, UnitNode, nullptr, ERigVMPinDirection::Invalid, ExportedDefaultValue, false, bNotify);
	}
	else if (RerouteNode != nullptr)
	{
		if (RerouteNode->GetPins().Num() == 0)
		{
			return;
		}

		URigVMPin* ValuePin = RerouteNode->Pins[0];

		// only repopulate the value pin, which may host a struct
		TArray<URigVMPin*> Pins = ValuePin->SubPins;
		for (URigVMPin* Pin : Pins)
		{
			RemovePin(Pin, false, bNotify);
		}
		ValuePin->SubPins.Reset();
		Pins.Reset();

		if (ValuePin->IsStruct())
		{
			UScriptStruct* ScriptStruct = ValuePin->GetScriptStruct();
			if (ScriptStruct == nullptr)
			{
				ReportErrorf(
					TEXT("Control Rig '%s', Node '%s' has no struct assigned. Do you have a broken redirect?"),
					*RerouteNode->GetOutermost()->GetPathName(),
					*RerouteNode->GetName()
				);

				RemoveNode(RerouteNode, false, true);
				return;
			}

			FString ExportedDefaultValue;
			CreateDefaultValueForStructIfRequired(ScriptStruct, ExportedDefaultValue);
			AddPinsForStruct(ScriptStruct, RerouteNode, ValuePin, ValuePin->Direction, ExportedDefaultValue, false);
		}
	}
	else if (EntryNode || ReturnNode)
	{
		if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(InNode->GetGraph()->GetOuter()))
		{
			bool bIsEntryNode = EntryNode != nullptr;

			TArray<URigVMPin*> Pins = InNode->GetPins();
			for (URigVMPin* Pin : Pins)
			{
				RemovePin(Pin, false, bNotify);
			}
			InNode->Pins.Reset();
			Pins.Reset();

			TArray<URigVMPin*> SortedLibraryPins;

			// add execute pins first
			for (URigVMPin* LibraryPin : LibraryNode->GetPins())
			{
				if (LibraryPin->IsExecuteContext())
				{
					SortedLibraryPins.Add(LibraryPin);
				}
			}

			// add remaining pins
			for (URigVMPin* LibraryPin : LibraryNode->GetPins())
			{
				SortedLibraryPins.AddUnique(LibraryPin);
			}

			for (URigVMPin* LibraryPin : SortedLibraryPins)
			{
				if (LibraryPin->GetDirection() == ERigVMPinDirection::IO && !LibraryPin->IsExecuteContext())
				{
					continue;
				}

				if (bIsEntryNode)
				{
					if (LibraryPin->GetDirection() == ERigVMPinDirection::Output)
					{
						continue;
					}
				}
				else
				{
					if (LibraryPin->GetDirection() == ERigVMPinDirection::Input)
					{
						continue;
					}
				}

				URigVMPin* ExposedPin = NewObject<URigVMPin>(InNode, LibraryPin->GetFName());
				ConfigurePinFromPin(ExposedPin, LibraryPin);

				if (bIsEntryNode)
				{
					ExposedPin->Direction = ERigVMPinDirection::Output;
				}
				else
				{
					ExposedPin->Direction = ERigVMPinDirection::Input;
				}

				InNode->Pins.Add(ExposedPin);

				if (ExposedPin->IsStruct())
				{
					AddPinsForStruct(ExposedPin->GetScriptStruct(), InNode, ExposedPin, ExposedPin->GetDirection(), FString(), false);
				}

				Notify(ERigVMGraphNotifType::PinAdded, ExposedPin);
			}
		}
		else
		{
			// in the future we'll likely have function libraries as outers here
			checkNoEntry();
		}
	}
	else if (CollapseNode)
	{
		FRigVMControllerGraphGuard GraphGuard(this, CollapseNode->GetContainedGraph(), false);
		for (URigVMNode* ContainedNode : CollapseNode->GetContainedNodes())
		{
			RepopulatePinsOnNode(ContainedNode, bFollowCoreRedirectors);
		}
	}
	else if (FunctionRefNode)
	{
		if (URigVMLibraryNode* ReferencedNode = FunctionRefNode->GetReferencedNode())
		{
			// we want to make sure notify the graph of a potential name change
			// when repopulating the function ref node
			Notify(ERigVMGraphNotifType::NodeRenamed, FunctionRefNode);

			TArray<URigVMPin*> Pins = InNode->GetPins();
			for (URigVMPin* Pin : Pins)
			{
				RemovePin(Pin, false, bNotify);
			}
			InNode->Pins.Reset();

			TMap<FString, FPinState> ReferencedPinStates = GetPinStates(ReferencedNode);

			for (URigVMPin* ReferencedPin : ReferencedNode->Pins)
			{
				URigVMPin* NewPin = NewObject<URigVMPin>(InNode, ReferencedPin->GetFName());
				ConfigurePinFromPin(NewPin, ReferencedPin);

				InNode->Pins.Add(NewPin);

				if (NewPin->IsStruct())
				{
					AddPinsForStruct(NewPin->GetScriptStruct(), InNode, NewPin, NewPin->GetDirection(), FString(), false);
				}

				Notify(ERigVMGraphNotifType::PinAdded, NewPin);
			}

			ApplyPinStates(InNode, ReferencedPinStates);
		}
	}

	else
	{
		return;
	}

	ApplyPinStates(InNode, PinStates, RedirectedPinPaths);

	if (URigVMInjectionInfo* InjectionInfo = InNode->GetInjectionInfo())
	{
		InjectionInfo->InputPin = InNode->FindPin(InjectionInputPinName.ToString());
		InjectionInfo->OutputPin = InNode->FindPin(InjectionOutputPinName.ToString());
	}
}

#endif

void URigVMController::SetupDefaultUnitNodeDelegates(TDelegate<FName(FRigVMExternalVariable, FString)> InCreateExternalVariableDelegate)
{
	TWeakObjectPtr<URigVMController> WeakThis(this);

	UnitNodeCreatedContext.GetAllExternalVariablesDelegate().BindLambda([WeakThis]() -> TArray<FRigVMExternalVariable> {
		if (WeakThis.IsValid())
		{
			return WeakThis->GetExternalVariables();
		}
		return TArray<FRigVMExternalVariable>();
	});

	UnitNodeCreatedContext.GetBindPinToExternalVariableDelegate().BindLambda([WeakThis](FString InPinPath, FString InVariablePath) -> bool {
		if (WeakThis.IsValid())
		{
			return WeakThis->BindPinToVariable(InPinPath, InVariablePath, true);
		}
		return false;
	});

	UnitNodeCreatedContext.GetCreateExternalVariableDelegate() = InCreateExternalVariableDelegate;
}

void URigVMController::ResetUnitNodeDelegates()
{
	UnitNodeCreatedContext.GetAllExternalVariablesDelegate().Unbind();
	UnitNodeCreatedContext.GetBindPinToExternalVariableDelegate().Unbind();
	UnitNodeCreatedContext.GetCreateExternalVariableDelegate().Unbind();
}

FLinearColor URigVMController::GetColorFromMetadata(const FString& InMetadata)
{
	FLinearColor Color = FLinearColor::Black;

	FString Metadata = InMetadata;
	Metadata.TrimStartAndEndInline();
	FString SplitString(TEXT(" "));
	FString Red, Green, Blue, GreenAndBlue;
	if (Metadata.Split(SplitString, &Red, &GreenAndBlue))
	{
		Red.TrimEndInline();
		GreenAndBlue.TrimStartInline();
		if (GreenAndBlue.Split(SplitString, &Green, &Blue))
		{
			Green.TrimEndInline();
			Blue.TrimStartInline();

			float RedValue = FCString::Atof(*Red);
			float GreenValue = FCString::Atof(*Green);
			float BlueValue = FCString::Atof(*Blue);
			Color = FLinearColor(RedValue, GreenValue, BlueValue);
		}
	}

	return Color;
}

TMap<FString, FString> URigVMController::GetRedirectedPinPaths(URigVMNode* InNode) const
{
	TMap<FString, FString> RedirectedPinPaths;
	URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(InNode);
	URigVMRerouteNode* RerouteNode = Cast<URigVMRerouteNode>(InNode);

	UScriptStruct* OwningStruct = nullptr;
	if (UnitNode)
	{
		OwningStruct = UnitNode->GetScriptStruct();
	}
	else if (RerouteNode)
	{
		URigVMPin* ValuePin = RerouteNode->Pins[0];
		if (ValuePin->IsStruct())
		{
			OwningStruct = ValuePin->GetScriptStruct();
		}
	}

	if (OwningStruct)
	{
		TArray<URigVMPin*> AllPins = InNode->GetAllPinsRecursively();
		for (URigVMPin* Pin : AllPins)
		{
			FString NodeName, PinPath;
			URigVMPin::SplitPinPathAtStart(Pin->GetPinPath(), NodeName, PinPath);

			if (RerouteNode)
			{
				FString ValuePinName, SubPinPath;
				if (URigVMPin::SplitPinPathAtStart(PinPath, ValuePinName, SubPinPath))
				{
					FString RedirectedSubPinPath;
					if (ShouldRedirectPin(OwningStruct, SubPinPath, RedirectedSubPinPath))
					{
						FString RedirectedPinPath = URigVMPin::JoinPinPath(ValuePinName, RedirectedSubPinPath);
						RedirectedPinPaths.Add(PinPath, RedirectedPinPath);
					}
				}
			}
			else
			{
				FString RedirectedPinPath;
				if (ShouldRedirectPin(OwningStruct, PinPath, RedirectedPinPath))
				{
					RedirectedPinPaths.Add(PinPath, RedirectedPinPath);
				}
			}
		}
	};
	return RedirectedPinPaths;
}

URigVMController::FPinState URigVMController::GetPinState(URigVMPin* InPin) const
{
	FPinState State;
	State.DefaultValue = InPin->GetDefaultValue();
	State.BoundVariable = InPin->GetBoundVariablePath();
	State.bIsExpanded = InPin->IsExpanded();
	State.InjectionInfos = InPin->GetInjectedNodes();
	return State;
}

TMap<FString, URigVMController::FPinState> URigVMController::GetPinStates(URigVMNode* InNode) const
{
	TMap<FString, FPinState> PinStates;

	TArray<URigVMPin*> AllPins = InNode->GetAllPinsRecursively();
	for (URigVMPin* Pin : AllPins)
	{
		FString PinPath, NodeName;
		URigVMPin::SplitPinPathAtStart(Pin->GetPinPath(), NodeName, PinPath);

		FPinState State = GetPinState(Pin);
		PinStates.Add(PinPath, State);
	}

	return PinStates;
}

void URigVMController::ApplyPinState(URigVMPin* InPin, const FPinState& InPinState)
{
	for (URigVMInjectionInfo* InjectionInfo : InPinState.InjectionInfos)
	{
		InjectionInfo->Rename(nullptr, InPin, REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
		InjectionInfo->InputPin = InjectionInfo->UnitNode->FindPin(InjectionInfo->InputPin->GetName());
		InjectionInfo->OutputPin = InjectionInfo->UnitNode->FindPin(InjectionInfo->OutputPin->GetName());
		InPin->InjectionInfos.Add(InjectionInfo);
	}

	if (!InPinState.DefaultValue.IsEmpty())
	{
		SetPinDefaultValue(InPin, InPinState.DefaultValue, true, false, false);
	}

	SetPinExpansion(InPin, InPinState.bIsExpanded, false);
	BindPinToVariable(InPin, InPinState.BoundVariable, false);
}

void URigVMController::ApplyPinStates(URigVMNode* InNode, const TMap<FString, URigVMController::FPinState>& InPinStates, const TMap<FString, FString>& InRedirectedPinPaths)
{
	for (const TPair<FString, FPinState>& PinStatePair : InPinStates)
	{
		FString PinPath = PinStatePair.Key;
		const FPinState& PinState = PinStatePair.Value;

		if (InRedirectedPinPaths.Contains(PinPath))
		{
			PinPath = InRedirectedPinPaths.FindChecked(PinPath);
		}

		if (URigVMPin* Pin = InNode->FindPin(PinPath))
		{
			ApplyPinState(Pin, PinState);
		}
		else
		{
			for (URigVMInjectionInfo* InjectionInfo : PinState.InjectionInfos)
			{
				InjectionInfo->UnitNode->Rename(nullptr, InNode->GetGraph(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
				DestroyObject(InjectionInfo);
			}
		}
	}
}

void URigVMController::ReportWarning(const FString& InMessage)
{
	if(!bReportWarningsAndErrors)
	{
		return;
	}

	FString Message = InMessage;
	if (URigVMGraph* Graph = GetGraph())
	{
		if (UPackage* Package = Cast<UPackage>(Graph->GetOutermost()))
		{
			Message = FString::Printf(TEXT("%s : %s"), *Package->GetPathName(), *InMessage);
		}
	}

	FScriptExceptionHandler::Get().HandleException(ELogVerbosity::Warning, *Message, *FString());
}

void URigVMController::ReportError(const FString& InMessage)
{
	if(!bReportWarningsAndErrors)
	{
		return;
	}

	FString Message = InMessage;
	if (URigVMGraph* Graph = GetGraph())
	{
		if (UPackage* Package = Cast<UPackage>(Graph->GetOutermost()))
		{
			Message = FString::Printf(TEXT("%s : %s"), *Package->GetPathName(), *InMessage);
		}
	}

	FScriptExceptionHandler::Get().HandleException(ELogVerbosity::Error, *Message, *FString());
}

void URigVMController::ReportAndNotifyError(const FString& InMessage)
{
	if (!bReportWarningsAndErrors)
	{
		return;
	}

	ReportError(InMessage);

#if WITH_EDITOR
	FNotificationInfo Info(FText::FromString(InMessage));
	Info.bUseSuccessFailIcons = true;
	Info.Image = FEditorStyle::GetBrush(TEXT("MessageLog.Warning"));
	Info.bFireAndForget = true;
	Info.bUseThrobber = true;
	// longer message needs more time to read
	Info.FadeOutDuration = FMath::Clamp(0.1f * InMessage.Len(), 5.0f, 20.0f);
	Info.ExpireDuration = Info.FadeOutDuration;
	TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
	if (NotificationPtr)
	{
		NotificationPtr->SetCompletionState(SNotificationItem::CS_Fail);
	}
#endif
}


void URigVMController::CreateDefaultValueForStructIfRequired(UScriptStruct* InStruct, FString& InOutDefaultValue)
{
	if (InStruct != nullptr)
	{
		TArray<uint8, TAlignedHeapAllocator<16>> TempBuffer;
		TempBuffer.AddUninitialized(InStruct->GetStructureSize());

		// call the struct constructor to initialize the struct
		InStruct->InitializeDefaultValue(TempBuffer.GetData());

		// apply any higher-level value overrides
		// for example,  
		// struct B { int Test; B() {Test = 1;}}; ----> This is the constructor initialization, applied first in InitializeDefaultValue() above 
		// struct A 
		// {
		//		Array<B> TestArray;
		//		A() 
		//		{
		//			TestArray.Add(B());
		//			TestArray[0].Test = 2;  ----> This is the overrride, applied below in ImportText()
		//		}
		// }
		// See UnitTest RigVM->Graph->UnitNodeDefaultValue for more use case.
		
		if (!InOutDefaultValue.IsEmpty() && InOutDefaultValue != TEXT("()"))
		{ 
			InStruct->ImportText(*InOutDefaultValue, TempBuffer.GetData(), nullptr, PPF_None, nullptr, FString());
		}

		// in case InOutDefaultValue is not empty, it needs to be cleared
		// before ExportText() because ExportText() appends to it.
		InOutDefaultValue.Reset();

		InStruct->ExportText(InOutDefaultValue, TempBuffer.GetData(), nullptr, nullptr, PPF_None, nullptr);
		InStruct->DestroyStruct(TempBuffer.GetData());
	}
}

void URigVMController::PostProcessDefaultValue(URigVMPin* Pin, FString& OutDefaultValue)
{
	if (Pin->IsArray() && OutDefaultValue.IsEmpty())
	{
		OutDefaultValue = TEXT("()");
	}
	else if (Pin->IsStruct() && (OutDefaultValue.IsEmpty() || OutDefaultValue == TEXT("()")))
	{
		CreateDefaultValueForStructIfRequired(Pin->GetScriptStruct(), OutDefaultValue);
	}
	else if (Pin->IsStringType())
	{
		while (OutDefaultValue.StartsWith(TEXT("\"")))
		{
			OutDefaultValue = OutDefaultValue.RightChop(1);
		}
		while (OutDefaultValue.EndsWith(TEXT("\"")))
		{
			OutDefaultValue = OutDefaultValue.LeftChop(1);
		}
	}
}

void URigVMController::PotentiallyResolvePrototypeNode(URigVMPrototypeNode* InNode, bool bSetupUndoRedo)
{
	TArray<URigVMNode*> NodesVisited;
	PotentiallyResolvePrototypeNode(InNode, bSetupUndoRedo, NodesVisited);
}

void URigVMController::PotentiallyResolvePrototypeNode(URigVMPrototypeNode* InNode, bool bSetupUndoRedo, TArray<URigVMNode*>& NodesVisited)
{
	if (InNode == nullptr)
	{
		return;
	}

	if (NodesVisited.Contains(InNode))
	{
		return;
	}
	NodesVisited.Add(InNode);

	// propagate types first
	for (URigVMPin* Pin : InNode->GetPins())
	{
		if (Pin->CPPType.IsEmpty())
		{
			TArray<URigVMPin*> LinkedPins = Pin->GetLinkedSourcePins();
			LinkedPins.Append(Pin->GetLinkedTargetPins());

			for (URigVMPin* LinkedPin : LinkedPins)
			{
				if (!LinkedPin->CPPType.IsEmpty())
				{
					ChangePinType(Pin, LinkedPin->CPPType, LinkedPin->CPPTypeObjectPath, bSetupUndoRedo);
					break;
				}
			}
		}
	}

	// check if the node is resolved
	FRigVMPrototype::FTypeMap ResolvedTypes;
	int32 FunctionIndex = InNode->GetResolvedFunctionIndex(&ResolvedTypes);
	if (FunctionIndex != INDEX_NONE)
	{
		// we have a valid node - let's replace this node... first let's find all 
		// links and all default values
		TMap<FString, FString> DefaultValues;
		TArray<TPair<FString, FString>> LinkPaths;

		for (URigVMPin* Pin : InNode->GetPins())
		{
			FString DefaultValue = Pin->GetDefaultValue();
			if (!DefaultValue.IsEmpty())
			{
				DefaultValues.Add(Pin->GetPinPath(), DefaultValue);
			}

			TArray<URigVMLink*> Links = Pin->GetSourceLinks(true);
			Links.Append(Pin->GetTargetLinks(true));

			for (URigVMLink* Link : Links)
			{
				LinkPaths.Add(TPair<FString, FString>(Link->GetSourcePin()->GetPinPath(), Link->GetTargetPin()->GetPinPath()));
			}
		}

		const FRigVMFunction& Function = FRigVMRegistry::Get().GetFunctions()[FunctionIndex];
		FString NodeName = InNode->GetName();
		FVector2D NodePosition = InNode->GetPosition();

		RemoveNode(InNode, bSetupUndoRedo);

		if (URigVMNode* NewNode = AddUnitNode(Function.Struct, Function.GetMethodName(), NodePosition, NodeName, bSetupUndoRedo))
		{
			// set default values again
			for (TPair<FString, FString> Pair : DefaultValues)
			{
				SetPinDefaultValue(Pair.Key, Pair.Value, true, bSetupUndoRedo, false);
			}

			// reestablish links
			for (TPair<FString, FString> Pair : LinkPaths)
			{
				AddLink(Pair.Key, Pair.Value, bSetupUndoRedo);
			}
		}

		return;
	}
	else
	{
		// update all of the pins that might have changed now as well!
		for (URigVMPin* Pin : InNode->GetPins())
		{
			if (Pin->CPPType.IsEmpty())
			{
				if (const FRigVMPrototypeArg::FType* Type = ResolvedTypes.Find(Pin->GetFName()))
				{
					if (!Type->CPPType.IsEmpty())
					{
						ChangePinType(Pin, Type->CPPType, Type->GetCPPTypeObjectPath(), bSetupUndoRedo);
					}
				}
			}
		}
	}

	// then recursively call
	TArray<URigVMNode*> LinkedNodes = InNode->GetLinkedSourceNodes();
	LinkedNodes.Append(InNode->GetLinkedTargetNodes());
	for (URigVMNode* LinkedNode : LinkedNodes)
	{
		PotentiallyResolvePrototypeNode(Cast<URigVMPrototypeNode>(LinkedNode), bSetupUndoRedo, NodesVisited);
	}
}

bool URigVMController::ChangePinType(const FString& InPinPath, const FString& InCPPType, const FName& InCPPTypeObjectPath, bool bSetupUndoRedo)
{
	if (!IsValidGraph())
	{
		return false;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if (URigVMPin* Pin = Graph->FindPin(InPinPath))
	{
		return ChangePinType(Pin, InCPPType, InCPPTypeObjectPath, bSetupUndoRedo);
	}

	return false;
}

bool URigVMController::ChangePinType(URigVMPin* InPin, const FString& InCPPType, const FName& InCPPTypeObjectPath, bool bSetupUndoRedo)
{
	if (InPin->CPPType == InCPPType)
	{
		return false;
	}

	if (InCPPType == TEXT("None") || InCPPType.IsEmpty())
	{
		return false;
	}

	UObject* CPPTypeObject = URigVMPin::FindObjectFromCPPTypeObjectPath(InCPPTypeObjectPath.ToString());
	if (CPPTypeObject)
	{
		// for now we don't allow UObjects
		if (!CPPTypeObject->IsA<UEnum>() &&
			!CPPTypeObject->IsA<UStruct>())
		{
			return false;
		}
	}

	FRigVMBaseAction Action;
	if (bSetupUndoRedo)
	{
		Action.Title = TEXT("Change pin type");
		ActionStack->BeginAction(Action);
	}

	if (bSetupUndoRedo)
	{
		BreakAllLinks(InPin, true, true);
		BreakAllLinks(InPin, false, true);
		BreakAllLinksRecursive(InPin, true, false, true);
		BreakAllLinksRecursive(InPin, false, false, true);

		ActionStack->AddAction(FRigVMChangePinTypeAction(InPin, InCPPType, InCPPTypeObjectPath));
	}

	TArray<URigVMPin*> Pins = InPin->SubPins;
	for (URigVMPin* Pin : Pins)
	{
		RemovePin(Pin, false, true);
	}
	InPin->SubPins.Reset();

	InPin->CPPType = InCPPType;
	InPin->CPPTypeObjectPath = InCPPTypeObjectPath;
	InPin->CPPTypeObject = CPPTypeObject;
	InPin->DefaultValue = FString();

	if (InPin->IsStruct())
	{
		FString DefaultValue = InPin->DefaultValue;
		CreateDefaultValueForStructIfRequired(InPin->GetScriptStruct(), DefaultValue);
		AddPinsForStruct(InPin->GetScriptStruct(), InPin->GetNode(), InPin, InPin->Direction, DefaultValue, false, true);
	}

	Notify(ERigVMGraphNotifType::PinTypeChanged, InPin);
	Notify(ERigVMGraphNotifType::PinDefaultValueChanged, InPin);

	if (bSetupUndoRedo)
	{
		ActionStack->EndAction(Action);
	}

	return true;
}

#if WITH_EDITOR

void URigVMController::RewireLinks(URigVMPin* InOldPin, URigVMPin* InNewPin, bool bAsInput, bool bSetupUndoRedo, TArray<URigVMLink*> InLinks)
{
	ensure(InOldPin->GetRootPin() == InOldPin);
	ensure(InNewPin->GetRootPin() == InNewPin);

	if (bAsInput)
	{
		TArray<URigVMLink*> Links = InLinks;
		if (Links.Num() == 0)
		{
			Links = InOldPin->GetSourceLinks(true /* recursive */);
		}

		for (URigVMLink* Link : Links)
		{
			FString SegmentPath = Link->GetTargetPin()->GetSegmentPath();
			URigVMPin* NewPin = SegmentPath.IsEmpty() ? InNewPin : InNewPin->FindSubPin(SegmentPath);
			check(NewPin);

			BreakLink(Link->GetSourcePin(), Link->GetTargetPin(), false);
			AddLink(Link->GetSourcePin(), NewPin, false);
		}
	}
	else
	{
		TArray<URigVMLink*> Links = InLinks;
		if (Links.Num() == 0)
		{
			Links = InOldPin->GetTargetLinks(true /* recursive */);
		}

		for (URigVMLink* Link : Links)
		{
			FString SegmentPath = Link->GetSourcePin()->GetSegmentPath();
			URigVMPin* NewPin = SegmentPath.IsEmpty() ? InNewPin : InNewPin->FindSubPin(SegmentPath);
			check(NewPin);

			BreakLink(Link->GetSourcePin(), Link->GetTargetPin(), false);
			AddLink(NewPin, Link->GetTargetPin(), false);
		}
	}
}

#endif

void URigVMController::DestroyObject(UObject* InObjectToDestroy)
{
	InObjectToDestroy->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
	InObjectToDestroy->RemoveFromRoot();
}

FRigVMExternalVariable URigVMController::GetExternalVariableByName(const FName& InExternalVariableName)
{
	TArray<FRigVMExternalVariable> ExternalVariables = GetExternalVariables();
	for (const FRigVMExternalVariable& ExternalVariable : ExternalVariables)
	{
		if (ExternalVariable.Name == InExternalVariableName)
		{
			return ExternalVariable;
		}
	}
	return FRigVMExternalVariable();
}

TArray<FRigVMExternalVariable> URigVMController::GetExternalVariables()
{
	if (GetExternalVariablesDelegate.IsBound())
	{
		return GetExternalVariablesDelegate.Execute();
	}
	return TArray<FRigVMExternalVariable>();
}

const FRigVMByteCode* URigVMController::GetCurrentByteCode() const
{
	if (GetCurrentByteCodeDelegate.IsBound())
	{
		return GetCurrentByteCodeDelegate.Execute();
	}
	return nullptr;
}

void URigVMController::RefreshFunctionReferences(URigVMLibraryNode* InFunctionDefinition, bool bSetupUndoRedo)
{
	check(InFunctionDefinition);

	if (URigVMFunctionLibrary* FunctionLibrary = Cast<URigVMFunctionLibrary>(InFunctionDefinition->GetGraph()))
	{
		FRigVMFunctionReferenceArray* ReferencesEntry = FunctionLibrary->FunctionReferences.Find(InFunctionDefinition);
		if (ReferencesEntry)
		{
			for (TSoftObjectPtr<URigVMFunctionReferenceNode> FunctionReferencePtr : ReferencesEntry->FunctionReferences)
			{
				// only update valid, living references
				if (FunctionReferencePtr.IsValid())
				{
					FRigVMControllerGraphGuard GraphGuard(this, FunctionReferencePtr->GetGraph(), bSetupUndoRedo);

					TArray<URigVMLink*> Links = FunctionReferencePtr->GetLinks();
					DetachLinksFromPinObjects(&Links, true);
					RepopulatePinsOnNode(FunctionReferencePtr.Get(), false, true);
					TGuardValue<bool> ReportGuard(bReportWarningsAndErrors, false);
					ReattachLinksToPinObjects(false, &Links, true);
				}
			}
		}
	}
}
