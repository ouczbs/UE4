// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "KismetCompiler.h"

#include "BlueprintCompilerExtension.generated.h"

USTRUCT()
struct FBlueprintCompiledData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<UEdGraph>> IntermediateGraphs;
};

UCLASS(Abstract)
class KISMET_API UBlueprintCompilerExtension : public UObject
{
	GENERATED_BODY()
public:
	UBlueprintCompilerExtension(const FObjectInitializer& ObjectInitializer);

	void BlueprintCompiled(const FKismetCompilerContext& CompilationContext, const FBlueprintCompiledData& Data);

protected:
	/** 
	 * Override this if you're interested in running logic after class layout has been
	 * generated, but before bytecode and member variables have been 
	 */
	virtual void ProcessBlueprintCompiled(const FKismetCompilerContext& CompilationContext, const FBlueprintCompiledData& Data) {}
};
