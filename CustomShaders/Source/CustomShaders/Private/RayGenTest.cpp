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
{}

void FRayGenTest::BeginRendering()
{
	// If the handle is already initialized and valid, no need to do anything
	if (mPostOpaqueRenderDelegate.IsValid()) {
		return;
	}

	// Get the Renderer Module and add our entry to the callbacks so it can be executed each frame after
	//  the scene rendering is done

	const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
	if (RendererModule) {
		mPostOpaqueRenderDelegate = RendererModule->RegisterPostOpaqueRenderDelegate(FPostOpaqueRenderDelegate::CreateRaw(this ,&FRayGenTest::Execute_RenderThread));
	}

	// create output texture
	const FIntPoint TextureSize = { mCachedParams.RenderTarget->SizeX, mCachedParams.RenderTarget->SizeY };

	mTextureSizePixelsX = mCachedParams.RenderTarget->SizeX;
	mTextureSizePixelsY = mCachedParams.RenderTarget->SizeY;
	mTextureSizePixels = mCachedParams.RenderTarget->SizeX * mCachedParams.RenderTarget->SizeY;
	//mTextureSizePixels = 10000000;

	mElementSize = 4;
	mElementTypeSizeBytes = sizeof(float);

	mBufferSizeBytes = mElementTypeSizeBytes * mElementSize * mTextureSizePixels;
	auto BufferSizeElements = mElementSize * mTextureSizePixels;

	auto RenderTargetFormat = mCachedParams.RenderTarget->GetFormat();
	FRHITextureCreateDesc TextureDesc = FRHITextureCreateDesc::Create2D(TEXT("RaytracingTestOutput") ,TextureSize.X ,TextureSize.Y
																		,RenderTargetFormat);
	TextureDesc.AddFlags(TexCreate_ShaderResource | TexCreate_UAV);
	mShaderOutputTexture = RHICreateTexture(TextureDesc);
	mShaderOutputTextureUAV = RHICreateUnorderedAccessView(mShaderOutputTexture);



	mOutValBuf = new float[BufferSizeElements];
	auto sizeofOutBuf = sizeof(mOutValBuf);

	UE_LOG(LogTemp ,Display ,TEXT("sizeofOutBuf:%llu") ,sizeofOutBuf);
	const FDateTime dt = FDateTime::Now();
	mFolderDateTime = dt.ToString();


	//FSceneTextures& SceneTextures = FSceneRenderer::GetActiveSceneTextures();

	//FSceneTextureParameters SceneTextureParameters = GetSceneTextureParameters(GraphBuilder ,SceneTextures.UniformBuffer);
	//FSceneTextureParameters::GetSceneTextureParameters():


	//FRDGBuilder GraphBuilder(RHICmdList);



	// mRDGBufferDesc = FRDGBufferUAVDesc::CreateBufferDesc(4, 10);
	//mRDGBufferDesc = FRDGBufferDesc::CreateBufferDesc(4, 10);
	//FRDGBufferDesc RDGBufferDesc = FRDGBufferDesc::CreateBufferDesc(4, 10);
	//mRDGBuffer = FRDGBuffer(TEXT("debugBuffer"), RDGBufferDesc, ERDGBufferFlags::None);
	//FRDGBuffer RDGBuffer(TEXT("debugBuffer"), RDGBufferDesc, ERDGBufferFlags::None);

	//FRDGBufferRef DebugBuffer = &RDGBuffer;


	//FRDGBufferUAVDesc RDGBufferUAVDesc(DebugBuffer);

	//FRDGBufferUAV
	//mDebugTex = GraphBuilder.CreateUAV(RDGBufferUAVDesc);





	/*

	FRDGTextureUAVDesc DebugTextureDesc = FRDGTextureUAVDesc::(
		TEXT("RaytracingTestOutputDebug")
		, 10
		, 10
		, PF_R32_FLOAT
	);
	mDebugTex = GraphBuilder.CreateUAV(DebugTextureDesc);

	GraphBuilder->CreateUAV

	*/
}

//Stop the compute shader execution
void FRayGenTest::EndRendering()
{
	//If the handle is not valid then there's no cleanup to do
	if (!mPostOpaqueRenderDelegate.IsValid()) {
		return;
	}
	//Get the Renderer Module and remove our entry from the PostOpaqueRender callback
	const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
	if (RendererModule) {
		RendererModule->RemovePostOpaqueRenderDelegate(mPostOpaqueRenderDelegate);
	}

	mPostOpaqueRenderDelegate.Reset();
}

void FRayGenTest::UpdateParameters(FRayGenTestParameters& DrawParameters)
{
	mCachedParams = DrawParameters;
	bCachedParamsAreValid = true;
}


void AsyncCallbackImp(const float* a ,FRayGenTest* RayGenTest);

void FRayGenTest::Execute_RenderThread(FPostOpaqueRenderParameters& Parameters)
#if RHI_RAYTRACING
{
	if (!mShaderOutputTextureUAV)
		return;


	FRDGBuilder* pGraphBuilder = Parameters.GraphBuilder;
	if (!pGraphBuilder)
		return;

	FRDGBuilder& GraphBuilder = *pGraphBuilder;

	FRHICommandListImmediate& RHICmdList = GraphBuilder.RHICmdList;
	//If there's no cached parameters to use, skip
	//If no Render Target is supplied in the cachedParams, skip
	if (!(bCachedParamsAreValid && mCachedParams.RenderTarget)) {
		return;
	}

	//Render Thread Assertion
	check(IsInRenderingThread());



	FRDGBufferRef OutputBuffer = GraphBuilder.CreateBuffer( 
		FRDGBufferDesc::CreateBufferDesc(mElementSize * mElementTypeSizeBytes ,mTextureSizePixels)
		,TEXT("OutputBuffer")
	);

	//mDebugTexRef = GraphBuilder->CreateUAV(FRDGBufferUAVDesc(OutputBuffer, PF_R32_SINT));
	//mDebugTexRef = GraphBuilder->CreateUAV(mRDGBufferDesc);

	if (!Parameters.View)
		return;

	const class FViewInfo& View = *(Parameters.View);

	FSceneTextureParameters SceneTextureParameters = GetSceneTextureParameters(GraphBuilder ,View);



	// set shader parameters
	FRayGenTestRGS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRayGenTestRGS::FParameters>();
	PassParameters->ViewUniformBuffer = Parameters.View->ViewUniformBuffer;
	PassParameters->TLAS = mCachedParams.Scene->GetLayerSRVChecked(ERayTracingSceneLayer::Base);
	PassParameters->outTex = mShaderOutputTextureUAV;
	PassParameters->MyValue = .70369;
	//PassParameters->DebugTex = mDebugTexRef;
	PassParameters->DebugTex = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutputBuffer ,PF_R32_FLOAT));;

	PassParameters->SceneTextures = SceneTextureParameters;

	// define render pass needed parameters
	TShaderMapRef<FRayGenTestRGS> RayGenTestRGS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FIntPoint TextureSize = { mCachedParams.RenderTarget->SizeX, mCachedParams.RenderTarget->SizeY };
	FRHIRayTracingScene* RHIScene = mCachedParams.Scene->GetRHIRayTracingScene();

	// add the ray trace dispatch pass
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("RayGenTest") ,
		PassParameters ,
		ERDGPassFlags::Compute ,
		[PassParameters ,RayGenTestRGS ,TextureSize ,RHIScene] (FRHIRayTracingCommandList& RHICmdList) {
		FRayTracingShaderBindingsWriter GlobalResources;
		SetShaderParameters(GlobalResources ,RayGenTestRGS ,*PassParameters);

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
		FRayTracingPipelineState* PipeLine = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(RHICmdList ,PSOInitializer);
		RHICmdList.SetRayTracingMissShader(RHIScene ,0 ,PipeLine ,0 /* ShaderIndexInPipeline */ ,0 ,nullptr ,0);
		RHICmdList.RayTraceDispatch(PipeLine ,RayGenTestRGS.GetRayTracingShader() ,RHIScene ,GlobalResources ,TextureSize.X ,TextureSize.Y);
	}
	);


	static bool first = true;
	if (first) {
		first = false;
		FWindowsPlatformProcess::Sleep(0.03);
	}

	FWindowsPlatformProcess::Sleep(0.0030001);

	// Copy textures from the shader output to our render target
	// this is done as a render pass with the graph builder
	FTexture2DRHIRef OriginalRT = mCachedParams.RenderTarget->GetRenderTargetResource()->GetTexture2DRHI();
	FRDGTexture* OutputRDGTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(mShaderOutputTexture ,TEXT("RaytracingTestOutputRT")));
	FRDGTexture* CopyToRDGTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(OriginalRT ,TEXT("RaytracingTestCopyToRT")));
	FRHICopyTextureInfo CopyInfo;
	CopyInfo.Size = FIntVector(TextureSize.X ,TextureSize.Y ,0);
	AddCopyTexturePass(GraphBuilder ,OutputRDGTexture ,CopyToRDGTexture ,CopyInfo);




	//GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteMySimpleComputeShaderOutput"));
	FRHIGPUBufferReadback* GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteMySimpleComputeShaderOutput"));

	//static FRHIGPUBufferReadback* GPUBufferReadback = nullptr;

	//if (!GPUBufferReadback)
	//	GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteMySimpleComputeShaderOutput"));

	AddEnqueueCopyPass(GraphBuilder ,GPUBufferReadback ,OutputBuffer ,mBufferSizeBytes);

	TFunction<void(const float* OutputVal ,FRayGenTest* RayGenTest)> AsyncCallback;
	AsyncCallback = AsyncCallbackImp;

	float* OutVal = mOutValBuf;
	//int OutVal =  0 ;
	auto BufferSizeBytes = mBufferSizeBytes;
	FRayGenTest* pRayGenTest = this;

	auto RunnerFunc = [GPUBufferReadback ,AsyncCallback ,OutVal ,BufferSizeBytes ,pRayGenTest] (auto&& RunnerFunc) -> void {

		if (GPUBufferReadback->IsReady()) {

			float* Buffer = (float*)GPUBufferReadback->Lock(1);
			//int OutVal = (int)Buffer[0];

			//int OutVal[10];
			//OutVal = Buffer;
			FMemory::Memcpy(OutVal ,Buffer ,BufferSizeBytes);
			//OutVal[0] = Buffer[0];
			//OutVal[1] = Buffer[1];

			GPUBufferReadback->Unlock();

			AsyncTask(ENamedThreads::GameThread ,[AsyncCallback ,OutVal ,pRayGenTest] () {
				AsyncCallback(OutVal ,pRayGenTest);
			});

			delete GPUBufferReadback;
		} else {
			AsyncTask(ENamedThreads::ActualRenderingThread ,[RunnerFunc] () {
				RunnerFunc(RunnerFunc);
			});
		}
	};

	AsyncTask(ENamedThreads::ActualRenderingThread ,[RunnerFunc] () {
		RunnerFunc(RunnerFunc);
	});





}

void FRayGenTest::SaveBuffer(const float* a)
{
	const FDateTime dt = FDateTime::Now();
	FString FileDateTime = dt.ToString();

	const FString FileName = TEXT("BufferData-") + FileDateTime + TEXT(".csv");
	const FString FolderPath = FPaths::Combine(FPaths::ProjectSavedDir() ,TEXT("RTX") ,mFolderDateTime);

	if (!FPaths::DirectoryExists(FolderPath)) {
		IPlatformFile& FileManager = FPlatformFileManager::Get().GetPlatformFile();
		if (FileManager.CreateDirectoryTree(*FolderPath)) {
		}
	}

	IFileManager* FileManager = &IFileManager::Get();

	FString OutData = TEXT("PosX , PosY , PosZ , X , Y , Dist\n");


	for (int rowY = 0; rowY < mTextureSizePixelsY; rowY++) {

		for (int colX = 0; colX < mTextureSizePixelsX; colX++) {

			int BufferPos = (rowY * mTextureSizePixelsX + colX) * 4;

			OutData += FString::Printf(TEXT(" %f , %f , %f , %d,%d , %f\n")
									   ,a[BufferPos + 0]
									   ,a[BufferPos + 1]
									   ,a[BufferPos + 2]
									   ,colX
									   ,rowY
									   ,a[BufferPos + 3]);

		}

		FFileHelper::SaveStringToFile(OutData ,*FPaths::Combine(FolderPath ,FileName)
									  ,FFileHelper::EEncodingOptions::AutoDetect
									  ,FileManager
									  ,FILEWRITE_Append);

		OutData.Empty();
	}
}

void AsyncCallbackImp(const float* a ,FRayGenTest* pRayGenTest)
{
	static float b = 0;
	b += a[0];
	b += a[1];
	b += a[2];
	b += a[3];
	b += a[4];
	b += a[5];

	static bool cond = false;

	if (cond)
		pRayGenTest->SaveBuffer(a);

	cond = false;
}



#else // !RHI_RAYTRACING
{
	unimplemented();
}
#endif
