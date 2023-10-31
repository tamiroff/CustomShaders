#pragma once

#include "CoreMinimal.h"

#include "RayTracingPayloadType.h"
#include "ShaderParameterStruct.h"
#include "Runtime/Renderer/Private/SceneTextureParameters.h"
//D:\Unreal\Engines\UE_5_2_1\Engine\Source\Runtime\Renderer\Private\RayTracing\RayTracingAmbientOcclusion.cpp

#include "RenderGraphUtils.h"
#include "Engine/TextureRenderTargetVolume.h"
#include "Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h"

class FRayTracingScene;

struct  FRayGenTestParameters
{

	FIntPoint GetRenderTargetSize() const
	{
		return CachedRenderTargetSize;
	}

	FRayGenTestParameters() {}; // consider delete this, otherwise the target size will not be set, or just add setter
	FRayGenTestParameters(UTextureRenderTarget2D* IORenderTarget)
		: RenderTarget(IORenderTarget)
	{
		CachedRenderTargetSize = RenderTarget ? FIntPoint(RenderTarget->SizeX ,RenderTarget->SizeY) : FIntPoint::ZeroValue;
	}

	UTextureRenderTarget2D* RenderTarget;
	FRayTracingScene* Scene;
	FIntPoint CachedRenderTargetSize;
};

class CUSTOMSHADERS_API FRayGenTest
{
public:
	FRayGenTest();

	void BeginRendering();
	void EndRendering();
	void UpdateParameters(FRayGenTestParameters& DrawParameters);

	float* mOutValBuf = nullptr;
	float* mOutValFloat = nullptr;
	size_t mTextureSizePixelsX = 0;
	size_t mTextureSizePixelsY = 0;
	size_t mTextureSizePixels = 0;
	size_t mElementSize = 0;
	size_t mElementTypeSizeBytes = 0;
	size_t mBufferSizeBytes = 0;
	FString mFolderDateTime;

	void SaveBuffer(const float* a);
private:
	void Execute_RenderThread(FPostOpaqueRenderParameters& Parameters);



	/// The delegate handle to our function that will be executed each frame by the renderer
	FDelegateHandle mPostOpaqueRenderDelegate;

	/// Cached Shader Manager Parameters
	FRayGenTestParameters mCachedParams;

	/// Whether we have cached parameters to pass to the shader or not
	volatile bool bCachedParamsAreValid;

	/// We create the shader's output texture and UAV and save to avoid reallocation
	FTexture2DRHIRef mShaderOutputTexture;
	FUnorderedAccessViewRHIRef mShaderOutputTextureUAV;

	//FRDGBufferUAVRef mDebugTexRef;
	//FRDGBufferUAV mDebugTex;
	//FRDGBuffer mRDGBuffer;
	//FRDGBufferUAVDesc mRDGBufferDesc;
	//FRDGBufferRef mInBufferRef;
	//FRDGBuffer mInBuffer;
	//FRDGBufferDesc mRDGBufferDesc;

	//FRHIGPUBufferReadback* GPUBufferReadback = nullptr;
};


#define NUM_THREADS_PER_GROUP_DIMENSION 8

class FRayGenTestRGS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayGenTestRGS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayGenTestRGS, FGlobalShader)
	
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER(float, MyValue)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, outTex)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, DebugTex)
		//SHADER_PARAMETER_RDG_BUFFER_UAV(RWTexture2D<float3>, DebugTexFloat)
		//SHADER_PARAMETER_UAV(RWTexture2D<int>, DebugTex)
		//SHADER_PARAMETER_UAV(RWTexture2D<float>, DebugTex)
		//SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, DebugBuffer)
		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters ,SceneTextures)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
	END_SHADER_PARAMETER_STRUCT()


	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 /*PermutationId*/)
	{
		return ERayTracingPayloadType::Minimal;
	}

	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		//We're using it here to add some preprocessor defines. That way we don't have to change both C++ and HLSL code when we change the value for NUM_THREADS_PER_GROUP_DIMENSION
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), NUM_THREADS_PER_GROUP_DIMENSION);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), NUM_THREADS_PER_GROUP_DIMENSION);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Z"), NUM_THREADS_PER_GROUP_DIMENSION);
	}
};
IMPLEMENT_GLOBAL_SHADER(FRayGenTestRGS, "/Plugin/CustomShaders/private/MyRayTraceTest.usf", "RayTraceTestRGS", SF_RayGen);

class FRayGenTestCHS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayGenTestCHS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayGenTestCHS, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return FRayGenTestRGS::GetRayTracingPayloadType(PermutationId);
	}

	using FParameters = FEmptyShaderParameters;
};
IMPLEMENT_GLOBAL_SHADER(FRayGenTestCHS, "/Plugin/CustomShaders/private/MyRayTraceTest.usf", "closestHit=RayTraceTestCHS", SF_RayHitGroup);

class FRayGenTestMS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayGenTestMS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayGenTestMS, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return FRayGenTestRGS::GetRayTracingPayloadType(PermutationId);
	}

	using FParameters = FEmptyShaderParameters;
};
IMPLEMENT_GLOBAL_SHADER(FRayGenTestMS, "/Plugin/CustomShaders/private/MyRayTraceTest.usf", "RayTraceTestMS", SF_RayMiss);