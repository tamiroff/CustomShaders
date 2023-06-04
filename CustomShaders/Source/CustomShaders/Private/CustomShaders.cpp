// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomShaders.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FCustomShadersModule"

void FCustomShadersModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module


	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("CustomShaders"))->GetBaseDir() ,TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/CustomShaders") ,PluginShaderDir);
}

void FCustomShadersModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FCustomShadersModule, CustomShaders)