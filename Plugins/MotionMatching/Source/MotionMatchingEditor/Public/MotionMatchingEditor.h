// Copyright Terry Meng 2022 All Rights Reserved.
#pragma once

#include "Modules/ModuleManager.h"
#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**/
class FMotionMatchingEditorModule : public IModuleInterface
{
public:
	FMotionMatchingEditorModule()
	{
	}
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};