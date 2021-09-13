// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMNode.h"
#include "RigVMLink.h"
#include "RigVMNotifications.h"
#include "Nodes/RigVMVariableNode.h"
#include "Nodes/RigVMParameterNode.h"
#include "RigVMCompiler/RigVMAST.h"
#include "RigVMGraph.generated.h"

class URigVMFunctionLibrary;

/**
 * The Graph represents a Function definition
 * using Nodes as statements.
 * Graphs can be compiled into a URigVM using the 
 * FRigVMCompiler. 
 * Graphs provide access to its Nodes, Pins and
 * Links.
 */
UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMGraph : public UObject
{
	GENERATED_BODY()

public:

	// Default constructor
	URigVMGraph();

	// Returns all of the Nodes within this Graph.
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	const TArray<URigVMNode*>& GetNodes() const;

	// Returns all of the Links within this Graph.
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	const TArray<URigVMLink*>& GetLinks() const;

	// Returns all of the contained graphs
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	TArray<URigVMGraph*> GetContainedGraphs(bool bRecursive = false) const;

	// Returns the entry node of this graph
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	URigVMFunctionEntryNode* GetEntryNode() const;
	
	// Returns the return node of this graph
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	URigVMFunctionReturnNode* GetReturnNode() const;

	// Returns a list of unique Variable descriptions within this Graph.
	// Multiple Variable Nodes can share the same description.
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	TArray<FRigVMGraphVariableDescription> GetVariableDescriptions() const;

	// Returns a list of unique Parameter descriptions within this Graph.
	// Multiple Parameter Nodes can share the same description.
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	TArray<FRigVMGraphParameterDescription> GetParameterDescriptions() const;

	// Returns the path of this graph as defined by its invoking nodes
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	virtual FString GetNodePath() const;

	// Returns the name of this graph (as defined by the node path)
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	FString GetGraphName() const;

	// Returns a Node given its name (or nullptr).
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	URigVMNode* FindNodeByName(const FName& InNodeName) const;

	// Returns a Node given its path (or nullptr).
	// (for now this is the same as finding a node by its name.)
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	URigVMNode* FindNode(const FString& InNodePath) const;

	// Returns a Pin given its path, for example "Node.Color.R".
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	URigVMPin* FindPin(const FString& InPinPath) const;

	// Returns a link given its string representation,
	// for example "NodeA.Color.R -> NodeB.Translation.X"
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	URigVMLink* FindLink(const FString& InLinkPinPathRepresentation) const;

	// Returns true if a Node with a given name is selected.
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	bool IsNodeSelected(const FName& InNodeName) const;

	// Returns the names of all currently selected Nodes.
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	const TArray<FName>& GetSelectNodes() const;

	// Returns true if this graph is the top level graph
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	bool IsTopLevelGraph() const;

	// Returns the locally available function library
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	virtual URigVMFunctionLibrary* GetDefaultFunctionLibrary() const;

	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	void SetDefaultFunctionLibrary(URigVMFunctionLibrary* InFunctionLibrary);

	// Returns the modified event, which can be used to 
	// subscribe to changes happening within the Graph.
	FRigVMGraphModifiedEvent& OnModified();

	void PrepareCycleChecking(URigVMPin* InPin, bool bAsInput);
	virtual bool CanLink(URigVMPin* InSourcePin, URigVMPin* InTargetPin, FString* OutFailureReason, const FRigVMByteCode* InByteCode);
	
	TSharedPtr<FRigVMParserAST> GetDiagnosticsAST(bool bForceRefresh = false, TArray<URigVMLink*> InLinksToSkip = TArray<URigVMLink*>());
	TSharedPtr<FRigVMParserAST> GetRuntimeAST(const FRigVMParserASTSettings& InSettings = FRigVMParserASTSettings::Optimized(), bool bForceRefresh = false);
	void ClearAST(bool bClearDiagnostics = true, bool bClearRuntime = true);

private:

	void RepopulatePinLinks();

	FRigVMGraphModifiedEvent ModifiedEvent;
	void Notify(ERigVMGraphNotifType InNotifType, UObject* InSubject);

	UPROPERTY()
	TArray<URigVMNode*> Nodes;

	UPROPERTY()
	TArray<URigVMLink*> Links;

	UPROPERTY()
	TArray<FName> SelectedNodes;

	UPROPERTY()
	TWeakObjectPtr<URigVMGraph> DefaultFunctionLibraryPtr;

	TSharedPtr<FRigVMParserAST> DiagnosticsAST;
	TSharedPtr<FRigVMParserAST> RuntimeAST;

#if WITH_EDITOR
	TArray<TSharedPtr<FString>> VariableNames;
	TArray<TSharedPtr<FString>> ParameterNames;
#endif

	bool IsNameAvailable(const FString& InName);

	friend class URigVMController;
	friend class URigVMCompiler;
	friend struct FRigVMControllerObjectFactory;
};

