#include "RayGenTest.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderTargetPool.h"
#include "RHI.h"
#include "Modules/ModuleManager.h"
#include "RayTracingDefinitions.h"
#include "RayTracingPayloadType.h"
#include "../Private/RayTracing/RayTracingScene.h"
#include "../Private/SceneRendering.h"
#include "RenderGraphUtils.h"

FRayGenTest::FRayGenTest()
{
}

void FRayGenTest::BeginRendering()
{
	// If the handle is already initialized and valid, no need to do anything
	if (mPostOpaqueRenderDelegate.IsValid())
	{
		return;
	}

	// Get the Renderer Module and add our entry to the callbacks so it can be executed each frame after
	//  the scene rendering is done

	const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
	if (RendererModule)
	{
		mPostOpaqueRenderDelegate = RendererModule->RegisterPostOpaqueRenderDelegate(FPostOpaqueRenderDelegate::CreateRaw(this, &FRayGenTest::Execute_RenderThread));
	}

	// create output texture
	const FIntPoint TextureSize = { mCachedParams.RenderTarget->SizeX, mCachedParams.RenderTarget->SizeY };
	FRHITextureCreateDesc TextureDesc = FRHITextureCreateDesc::Create2D(TEXT("RaytracingTestOutput"), TextureSize.X, TextureSize.Y, mCachedParams.RenderTarget->GetFormat());
	TextureDesc.AddFlags(TexCreate_ShaderResource | TexCreate_UAV);
	mShaderOutputTexture = RHICreateTexture(TextureDesc);
	mShaderOutputTextureUAV = RHICreateUnorderedAccessView(mShaderOutputTexture);
}

//Stop the compute shader execution
void FRayGenTest::EndRendering()
{
	//If the handle is not valid then there's no cleanup to do
	if (!mPostOpaqueRenderDelegate.IsValid())
	{
		return;
	}
	//Get the Renderer Module and remove our entry from the PostOpaqueRender callback
	const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
	if (RendererModule)
	{
		RendererModule->RemovePostOpaqueRenderDelegate(mPostOpaqueRenderDelegate);
	}

	mPostOpaqueRenderDelegate.Reset();
}

void FRayGenTest::UpdateParameters(FRayGenTestParameters& DrawParameters)
{
	mCachedParams = DrawParameters;
	bCachedParamsAreValid = true;
}

void FRayGenTest::Execute_RenderThread(FPostOpaqueRenderParameters& Parameters)
#if RHI_RAYTRACING
{
	FRDGBuilder* GraphBuilder = Parameters.GraphBuilder;
	FRHICommandListImmediate& RHICmdList = GraphBuilder->RHICmdList;
	//If there's no cached parameters to use, skip
	//If no Render Target is supplied in the cachedParams, skip
	if (!(bCachedParamsAreValid && mCachedParams.RenderTarget))
	{
		return;
	}

	//Render Thread Assertion
	check(IsInRenderingThread());

	// set shader parameters
	FRayGenTestRGS::FParameters *PassParameters = GraphBuilder->AllocParameters<FRayGenTestRGS::FParameters>();
	PassParameters->ViewUniformBuffer = Parameters.View->ViewUniformBuffer;
	PassParameters->TLAS = mCachedParams.Scene->GetLayerSRVChecked(ERayTracingSceneLayer::Base);
	PassParameters->outTex = mShaderOutputTextureUAV;


	// define render pass needed parameters
	TShaderMapRef<FRayGenTestRGS> RayGenTestRGS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FIntPoint TextureSize = { mCachedParams.RenderTarget->SizeX, mCachedParams.RenderTarget->SizeY };
	FRHIRayTracingScene* RHIScene = mCachedParams.Scene->GetRHIRayTracingScene();

	// add the ray trace dispatch pass
	GraphBuilder->AddPass(
		RDG_EVENT_NAME("RayGenTest"),
		PassParameters,
		ERDGPassFlags::Compute,
		[PassParameters, RayGenTestRGS, TextureSize, RHIScene](FRHIRayTracingCommandList& RHICmdList)
		{
			FRayTracingShaderBindingsWriter GlobalResources;
			SetShaderParameters(GlobalResources, RayGenTestRGS, *PassParameters);

			FRayTracingPipelineStateInitializer PSOInitializer;
			PSOInitializer.MaxPayloadSizeInBytes = GetRayTracingPayloadTypeMaxSize(FRayGenTestRGS::GetRayTracingPayloadType(0));;
			PSOInitializer.bAllowHitGroupIndexing = false;

			// Set RayGen shader
			TArray<FRHIRayTracingShader*> RayGenShaderTable;
			RayGenShaderTable.Add(GetGlobalShaderMap(GMaxRHIFeatureLevel)->GetShader<FRayGenTestRGS>().GetRayTracingShader());
			PSOInitializer.SetRayGenShaderTable(RayGenShaderTable);

			// Set ClosestHit shader
			TArray<FRHIRayTracingShader*> RayHitShaderTable;
			RayHitShaderTable.Add(GetGlobalShaderMap(GMaxRHIFeatureLevel)->GetShader<FRayGenTestCHS>().GetRayTracingShader());
			PSOInitializer.SetHitGroupTable(RayHitShaderTable);
			
			// Set Miss shader
			TArray<FRHIRayTracingShader*> RayMissShaderTable;
			RayMissShaderTable.Add(GetGlobalShaderMap(GMaxRHIFeatureLevel)->GetShader<FRayGenTestMS>().GetRayTracingShader());
			PSOInitializer.SetMissShaderTable(RayMissShaderTable);

			// dispatch ray trace shader
			FRayTracingPipelineState* PipeLine = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(RHICmdList, PSOInitializer);
			RHICmdList.SetRayTracingMissShader(RHIScene, 0, PipeLine, 0 /* ShaderIndexInPipeline */, 0, nullptr, 0);
			RHICmdList.RayTraceDispatch(PipeLine, RayGenTestRGS.GetRayTracingShader(), RHIScene, GlobalResources, TextureSize.X, TextureSize.Y);
		}
	);

	// Copy textures from the shader output to our render target
	// this is done as a render pass with the graph builder
	FTexture2DRHIRef OriginalRT = mCachedParams.RenderTarget->GetRenderTargetResource()->GetTexture2DRHI();
	FRDGTexture* OutputRDGTexture = GraphBuilder->RegisterExternalTexture(CreateRenderTarget(mShaderOutputTexture, TEXT("RaytracingTestOutputRT")));
	FRDGTexture* CopyToRDGTexture = GraphBuilder->RegisterExternalTexture(CreateRenderTarget(OriginalRT, TEXT("RaytracingTestCopyToRT")));
	FRHICopyTextureInfo CopyInfo;
	CopyInfo.Size = FIntVector(TextureSize.X, TextureSize.Y, 0);
	AddCopyTexturePass(*GraphBuilder, OutputRDGTexture, CopyToRDGTexture, CopyInfo);

	float FooBarArray[16];
	TRefCountPtr< FRDGPooledBuffer > regBuffer;
	FRDGBufferRef BufferRef = GraphBuilder->RegisterExternalBuffer(regBuffer);
}
#else // !RHI_RAYTRACING
{
	unimplemented();
}
#endif
