// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Templates/PimplPtr.h"

#include "MeshDescriptionBaseBulkData.generated.h"


class UMeshDescriptionBase;
struct FMeshDescriptionBulkData;


/**
 * UObject wrapper for FMeshDescriptionBulkData
 */
UCLASS()
class MESHDESCRIPTION_API UMeshDescriptionBaseBulkData : public UObject
{
	GENERATED_BODY()

public:
	UMeshDescriptionBaseBulkData();
	virtual void Serialize(FArchive& Ar) override;
	virtual bool IsEditorOnly() const override;
	virtual bool NeedsLoadForClient() const override;
	virtual bool NeedsLoadForServer() const override;
	virtual bool NeedsLoadForEditorGame() const override;

#if WITH_EDITORONLY_DATA

	void Empty();
	UMeshDescriptionBase* CreateMeshDescription();
	UMeshDescriptionBase* GetMeshDescription() const;
	bool HasCachedMeshDescription() const;
	bool CacheMeshDescription();
	void CommitMeshDescription(bool bUseHashAsGuid);
	void RemoveMeshDescription();
	bool IsBulkDataValid() const;

	const FMeshDescriptionBulkData& GetBulkData() const;
	FMeshDescriptionBulkData& GetBulkData();

protected:
	TPimplPtr<FMeshDescriptionBulkData> BulkData;

	UPROPERTY(Transient, Instanced)
	UMeshDescriptionBase* PreallocatedMeshDescription;

	UPROPERTY(Transient)
	UMeshDescriptionBase* MeshDescription;

#endif //WITH_EDITORONLY_DATA
};
