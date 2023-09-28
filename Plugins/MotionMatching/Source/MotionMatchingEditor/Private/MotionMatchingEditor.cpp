// Copyright Terry Meng 2022 All Rights Reserved.
#include "MotionMatchingEditor.h"
static const FName MotionMatchingEditorTabName("MotionMatchingEditor");

#define LOCTEXT_NAMESPACE "FMotionMatchingEditorModule"

void FMotionMatchingEditorModule::StartupModule()
{
}

void FMotionMatchingEditorModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMotionMatchingEditorModule, MotionMatchingEditor)