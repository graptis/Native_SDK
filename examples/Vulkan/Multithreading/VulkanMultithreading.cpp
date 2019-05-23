/*!*********************************************************************************************************************
\File         VulkanMultithreading.cpp
\Title        Bump mapping
\Author       PowerVR by Imagination, Developer Technology Team
\Copyright    Copyright (c) Imagination Technologies Limited.
\brief      Shows how to perform tangent space bump mapping
***********************************************************************************************************************/
#include "PVRShell/PVRShell.h"
#include "PVRUtils/Vulkan/AsynchronousVk.h"
#include "PVRUtils/PVRUtilsVk.h"
#include <mutex>

const float RotateY = glm::pi<float>() / 150;
const glm::vec4 LightDir(.24f, .685f, -.685f, 0.0f);
const pvrvk::ClearValue ClearValue(0.00f, 0.70f, 0.67f, 1.f);
/*!*********************************************************************************************************************
 shader attributes
 ***********************************************************************************************************************/
// vertex attributes
namespace VertexAttrib {
enum Enum
{
	VertexArray,
	NormalArray,
	TexCoordArray,
	TangentArray,
	numAttribs
};
}

const pvr::utils::VertexBindings VertexAttribBindings[] = {
	{ "POSITION", 0 },
	{ "NORMAL", 1 },
	{ "UV0", 2 },
	{ "TANGENT", 3 },
};

// shader uniforms
namespace Uniform {
enum Enum
{
	MVPMatrix,
	LightDir,
	NumUniforms
};
}

/*!*********************************************************************************************************************
 Content file names
 ***********************************************************************************************************************/

// Source and binary shaders
const char FragShaderSrcFile[] = "FragShader_vk.fsh.spv";
const char VertShaderSrcFile[] = "VertShader_vk.vsh.spv";

// PVR texture files
const char StatueTexFile[] = "Marble.pvr";
const char StatueNormalMapFile[] = "MarbleNormalMap.pvr";

const char ShadowTexFile[] = "Shadow.pvr";
const char ShadowNormalMapFile[] = "ShadowNormalMap.pvr";

// POD _scene files
const char SceneFile[] = "Satyr.pod";

struct UboPerMeshData
{
	glm::mat4 mvpMtx;
	glm::vec3 lightDirModel;
};

struct DescriptorSetUpdateRequiredInfo
{
	pvr::utils::AsyncApiTexture diffuseTex;
	pvr::utils::AsyncApiTexture bumpTex;
	pvrvk::Sampler trilinearSampler;
	pvrvk::Sampler bilinearSampler;
};

struct DeviceResources
{
	pvrvk::Instance instance;
	pvrvk::DebugReportCallback debugCallbacks[2];
	pvrvk::Surface surface;
	pvrvk::Device device;
	pvrvk::Swapchain swapchain;
	pvrvk::Queue queue;

	pvr::utils::vma::Allocator vmaBufferAllocator;
	pvr::utils::vma::Allocator vmaImageAllocator;

	pvrvk::DescriptorPool descriptorPool;
	pvrvk::CommandPool commandPool;

	pvr::Multi<pvrvk::CommandBuffer> commandBuffers; // per swapchain
	pvr::Multi<pvrvk::CommandBuffer> loadingTextCommandBuffer; // per swapchain

	pvr::Multi<pvrvk::Framebuffer> framebuffer;
	pvr::Multi<pvrvk::ImageView> depthStencilImages;

	pvrvk::Semaphore semaphoreImageAcquired[static_cast<uint32_t>(pvrvk::FrameworkCaps::MaxSwapChains)];
	pvrvk::Fence perFrameAcquireFence[static_cast<uint32_t>(pvrvk::FrameworkCaps::MaxSwapChains)];
	pvrvk::Semaphore semaphorePresent[static_cast<uint32_t>(pvrvk::FrameworkCaps::MaxSwapChains)];
	pvrvk::Fence perFrameCommandBufferFence[static_cast<uint32_t>(pvrvk::FrameworkCaps::MaxSwapChains)];

	pvrvk::GraphicsPipeline pipe;

	pvr::async::TextureAsyncLoader loader;
	pvr::utils::ImageApiAsyncUploader uploader;
	std::vector<pvrvk::Buffer> vbos;
	std::vector<pvrvk::Buffer> ibos;
	pvrvk::DescriptorSetLayout texLayout;
	pvrvk::DescriptorSetLayout uboLayoutDynamic;
	pvrvk::PipelineLayout pipelayout;
	pvrvk::DescriptorSet texDescSet;

	// UIRenderer used to display text
	pvr::ui::UIRenderer uiRenderer;
	pvr::ui::Text loadingText[3];
	pvr::utils::StructuredBufferView structuredMemoryView;
	pvrvk::Buffer ubo;
	pvrvk::DescriptorSet uboDescSet[4];

	pvrvk::PipelineCache pipelineCache;

	DescriptorSetUpdateRequiredInfo asyncUpdateInfo;

	~DeviceResources()
	{
		if (device.isValid())
		{
			device->waitIdle();
		}
		auto items_remaining = loader.getNumQueuedItems();
		if (items_remaining)
		{
			Log(LogLevel::Information,
				"Asynchronous Texture Loader is not done: %d items pending. Before releasing,"
				" will wait until all pending load jobs are done.",
				items_remaining);
		}
		items_remaining = uploader.getNumQueuedItems();
		if (items_remaining)
		{
			Log(LogLevel::Information,
				"Asynchronous Texture Uploader is not done: %d items pending. Before releasing,"
				" will wait until all pending load jobs are done.",
				items_remaining);
		}
		if (device.isValid())
		{
			device->waitIdle();
			int l = swapchain->getSwapchainLength();
			for (int i = 0; i < l; ++i)
			{
				if (perFrameAcquireFence[i].isValid())
					perFrameAcquireFence[i]->wait();
				if (perFrameCommandBufferFence[i].isValid())
					perFrameCommandBufferFence[i]->wait();
			}
		}
	}
};

/*!*********************************************************************************************************************
 Class implementing the Shell functions.
 ***********************************************************************************************************************/
class VulkanMultithreading : public pvr::Shell
{
	pvr::async::Mutex _hostMutex;

	// 3D Model
	pvr::assets::ModelHandle _scene;

	// Projection and view matrix
	glm::mat4 _viewProj;

	bool _loadingDone;
	// The translation and Rotate parameter of Model
	float _angleY;
	uint32_t _frameId;
	std::unique_ptr<DeviceResources> _deviceResources;

public:
	VulkanMultithreading() : _loadingDone(false) {}
	virtual pvr::Result initApplication();
	virtual pvr::Result initView();
	virtual pvr::Result releaseView();
	virtual pvr::Result quitApplication();
	virtual pvr::Result renderFrame();

	void createImageSamplerDescriptorSets();
	void createUbo();
	void loadPipeline();
	void drawMesh(pvrvk::CommandBuffer& commandBuffer, int i32NodeIndex);
	void recordMainCommandBuffer();
	void recordLoadingCommandBuffer();
	void updateTextureDescriptorSet();
};

void VulkanMultithreading::updateTextureDescriptorSet()
{
	// create the descriptor set
	pvrvk::WriteDescriptorSet writeDescInfo[2] = { pvrvk::WriteDescriptorSet(pvrvk::DescriptorType::e_COMBINED_IMAGE_SAMPLER, _deviceResources->texDescSet, 0),
		pvrvk::WriteDescriptorSet(pvrvk::DescriptorType::e_COMBINED_IMAGE_SAMPLER, _deviceResources->texDescSet, 1) };

	writeDescInfo[0].setImageInfo(0,
		pvrvk::DescriptorImageInfo(
			_deviceResources->asyncUpdateInfo.diffuseTex->get(), _deviceResources->asyncUpdateInfo.bilinearSampler, pvrvk::ImageLayout::e_SHADER_READ_ONLY_OPTIMAL));

	writeDescInfo[1].setImageInfo(0,
		pvrvk::DescriptorImageInfo(
			_deviceResources->asyncUpdateInfo.bumpTex->get(), _deviceResources->asyncUpdateInfo.trilinearSampler, pvrvk::ImageLayout::e_SHADER_READ_ONLY_OPTIMAL));

	_deviceResources->device->updateDescriptorSets(writeDescInfo, ARRAY_SIZE(writeDescInfo), nullptr, 0);
}

/*!*********************************************************************************************************************
\return return true if no error occurred
\brief  Loads the textures required for this training course
***********************************************************************************************************************/
void VulkanMultithreading::createImageSamplerDescriptorSets()
{
	_deviceResources->texDescSet = _deviceResources->descriptorPool->allocateDescriptorSet(_deviceResources->texLayout);
	// create the bilinear sampler
	pvrvk::SamplerCreateInfo samplerInfo;
	samplerInfo.magFilter = pvrvk::Filter::e_LINEAR;
	samplerInfo.minFilter = pvrvk::Filter::e_LINEAR;
	samplerInfo.mipMapMode = pvrvk::SamplerMipmapMode::e_NEAREST;
	_deviceResources->asyncUpdateInfo.bilinearSampler = _deviceResources->device->createSampler(samplerInfo);

	samplerInfo.mipMapMode = pvrvk::SamplerMipmapMode::e_NEAREST;
	_deviceResources->asyncUpdateInfo.trilinearSampler = _deviceResources->device->createSampler(samplerInfo);
}

void VulkanMultithreading::createUbo()
{
	const uint32_t swapchainLength = _deviceResources->swapchain->getSwapchainLength();
	pvrvk::WriteDescriptorSet descUpdate[pvrvk::FrameworkCaps::MaxSwapChains];
	{
		pvr::utils::StructuredMemoryDescription desc;
		desc.addElement("MVPMatrix", pvr::GpuDatatypes::mat4x4);
		desc.addElement("LightDirModel", pvr::GpuDatatypes::vec3);

		_deviceResources->structuredMemoryView.initDynamic(desc, swapchainLength, pvr::BufferUsageFlags::UniformBuffer,
			static_cast<uint32_t>(_deviceResources->device->getPhysicalDevice()->getProperties().getLimits().getMinUniformBufferOffsetAlignment()));
		_deviceResources->ubo = pvr::utils::createBuffer(_deviceResources->device, _deviceResources->structuredMemoryView.getSize(), pvrvk::BufferUsageFlags::e_UNIFORM_BUFFER_BIT,
			pvrvk::MemoryPropertyFlags::e_HOST_VISIBLE_BIT,
			pvrvk::MemoryPropertyFlags::e_DEVICE_LOCAL_BIT | pvrvk::MemoryPropertyFlags::e_HOST_VISIBLE_BIT | pvrvk::MemoryPropertyFlags::e_HOST_COHERENT_BIT,
			&_deviceResources->vmaBufferAllocator, pvr::utils::vma::AllocationCreateFlags::e_MAPPED_BIT);

		_deviceResources->structuredMemoryView.pointToMappedMemory(_deviceResources->ubo->getDeviceMemory()->getMappedData());
	}

	for (uint32_t i = 0; i < swapchainLength; ++i)
	{
		_deviceResources->uboDescSet[i] = _deviceResources->descriptorPool->allocateDescriptorSet(_deviceResources->uboLayoutDynamic);
		descUpdate[i]
			.set(pvrvk::DescriptorType::e_UNIFORM_BUFFER, _deviceResources->uboDescSet[i])
			.setBufferInfo(0,
				pvrvk::DescriptorBufferInfo(
					_deviceResources->ubo, _deviceResources->structuredMemoryView.getDynamicSliceOffset(i), _deviceResources->structuredMemoryView.getDynamicSliceSize()));
	}

	_deviceResources->device->updateDescriptorSets(descUpdate, swapchainLength, nullptr, 0);
}

/*!*********************************************************************************************************************
\return  Return true if no error occurred
\brief  Loads and compiles the shaders and create a pipeline
***********************************************************************************************************************/
void VulkanMultithreading::loadPipeline()
{
	//--- create the texture-sampler descriptor set layout
	{
		pvrvk::DescriptorSetLayoutCreateInfo descSetLayoutInfo;
		descSetLayoutInfo
			.setBinding(0, pvrvk::DescriptorType::e_COMBINED_IMAGE_SAMPLER, 1, pvrvk::ShaderStageFlags::e_FRAGMENT_BIT) /*binding 0*/
			.setBinding(1, pvrvk::DescriptorType::e_COMBINED_IMAGE_SAMPLER, 1, pvrvk::ShaderStageFlags::e_FRAGMENT_BIT); /*binding 1*/
		_deviceResources->texLayout = _deviceResources->device->createDescriptorSetLayout(descSetLayoutInfo);
	}

	//--- create the ubo descriptorset layout
	{
		pvrvk::DescriptorSetLayoutCreateInfo descSetLayoutInfo;
		descSetLayoutInfo.setBinding(0, pvrvk::DescriptorType::e_UNIFORM_BUFFER, 1, pvrvk::ShaderStageFlags::e_VERTEX_BIT); /*binding 0*/
		_deviceResources->uboLayoutDynamic = _deviceResources->device->createDescriptorSetLayout(descSetLayoutInfo);
	}

	//--- create the pipeline layout
	{
		pvrvk::PipelineLayoutCreateInfo pipeLayoutInfo;
		pipeLayoutInfo
			.addDescSetLayout(_deviceResources->texLayout) /*set 0*/
			.addDescSetLayout(_deviceResources->uboLayoutDynamic); /*set 1*/
		_deviceResources->pipelayout = _deviceResources->device->createPipelineLayout(pipeLayoutInfo);
	}
	pvrvk::GraphicsPipelineCreateInfo pipeInfo;
	pipeInfo.rasterizer.setCullMode(pvrvk::CullModeFlags::e_BACK_BIT);
	pipeInfo.colorBlend.setAttachmentState(0, pvrvk::PipelineColorBlendAttachmentState());

	pvr::Stream::ptr_type vertSource = getAssetStream(VertShaderSrcFile);
	pvr::Stream::ptr_type fragSource = getAssetStream(FragShaderSrcFile);

	pipeInfo.vertexShader.setShader(_deviceResources->device->createShader(vertSource->readToEnd<uint32_t>()));
	pipeInfo.fragmentShader.setShader(_deviceResources->device->createShader(fragSource->readToEnd<uint32_t>()));

	const pvr::assets::Mesh& mesh = _scene->getMesh(0);
	pipeInfo.inputAssembler.setPrimitiveTopology(pvr::utils::convertToPVRVk(mesh.getPrimitiveType()));
	pipeInfo.pipelineLayout = _deviceResources->pipelayout;
	pipeInfo.renderPass = _deviceResources->framebuffer[0]->getRenderPass();
	pipeInfo.subpass = 0;
	// Enable z-buffer test. We are using a projection matrix optimized for a floating point depth buffer,
	// so the depth test and clear value need to be inverted (1 becomes near, 0 becomes far).
	pipeInfo.depthStencil.enableDepthTest(true);
	pipeInfo.depthStencil.setDepthCompareFunc(pvrvk::CompareOp::e_LESS);
	pipeInfo.depthStencil.enableDepthWrite(true);
	pvr::utils::populateInputAssemblyFromMesh(
		mesh, VertexAttribBindings, sizeof(VertexAttribBindings) / sizeof(VertexAttribBindings[0]), pipeInfo.vertexInput, pipeInfo.inputAssembler);

	pvr::utils::populateViewportStateCreateInfo(_deviceResources->framebuffer[0], pipeInfo.viewport);
	_deviceResources->pipe = _deviceResources->device->createGraphicsPipeline(pipeInfo, _deviceResources->pipelineCache);
}

/*!*********************************************************************************************************************
\return Return pvr::Result::Success if no error occurred
\brief  Code in initApplication() will be called by Shell once per run, before the rendering context is created.
	Used to initialize variables that are not dependent on it (e.g. external modules, loading meshes, etc.)
	If the rendering context is lost, initApplication() will not be called again.
***********************************************************************************************************************/
pvr::Result VulkanMultithreading::initApplication()
{
	// Load the _scene
	pvr::assets::helper::loadModel(*this, SceneFile, _scene);
	_angleY = 0.0f;
	_frameId = 0;
	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\return Return pvr::Result::Success if no error occurred
\brief  Code in quitApplication() will be called by PVRShell once per run, just before exiting the program.
	If the rendering context is lost, quitApplication() will not be called.x
***********************************************************************************************************************/
pvr::Result VulkanMultithreading::quitApplication()
{
	_scene.reset();
	return pvr::Result::Success;
}

void DiffuseTextureDoneCallback(pvr::utils::AsyncApiTexture tex)

{
	// We have set the "callbackBeforeSignal" to "true", which means we should NOT call GET before this function returns!
	if (tex->isSuccessful())
	{
		std::this_thread::sleep_for(std::chrono::seconds(2));
		Log(LogLevel::Information, "ASYNCUPLOADER: Diffuse texture uploading completed successfully.");
	}
	else
	{
		Log(LogLevel::Information, "ASYNCUPLOADER: ERROR uploading normal texture. You can handle this information in your applications.");
	}
}

void NormalTextureDoneCallback(pvr::utils::AsyncApiTexture tex)
{
	// We have set the "callbackBeforeSignal" to "true", which means we should NOT call GET before this function returns!
	if (tex->isSuccessful())
	{
		std::this_thread::sleep_for(std::chrono::seconds(2));
		Log(LogLevel::Information, "ASYNCUPLOADER: Normal texture uploading has been completed.");
	}
	else
	{
		Log(LogLevel::Information,
			"ASYNCUPLOADER: ERROR uploading normal texture. You can handle this "
			"information in your applications.");
	}
}

/*!*********************************************************************************************************************
\return Return pvr::Result::Success if no error occurred
\brief  Code in initView() will be called by Shell upon initialization or after a change in the rendering context.
	Used to initialize variables that are dependent on the rendering context (e.g. textures, vertex buffers, etc.)
***********************************************************************************************************************/
pvr::Result VulkanMultithreading::initView()
{
	_deviceResources = std::unique_ptr<DeviceResources>(new DeviceResources());

	// Create instance and retrieve compatible physical devices
	_deviceResources->instance = pvr::utils::createInstance(this->getApplicationName());

	// Create the surface
	_deviceResources->surface = pvr::utils::createSurface(_deviceResources->instance, _deviceResources->instance->getPhysicalDevice(0), this->getWindow(), this->getDisplay());

	// Add Debug Report Callbacks
	// Add a Debug Report Callback for logging messages for events of all supported types.
	_deviceResources->debugCallbacks[0] = pvr::utils::createDebugReportCallback(_deviceResources->instance);
	// Add a second Debug Report Callback for throwing exceptions for Error events.
	_deviceResources->debugCallbacks[1] =
		pvr::utils::createDebugReportCallback(_deviceResources->instance, pvrvk::DebugReportFlagsEXT::e_ERROR_BIT_EXT, pvr::utils::throwOnErrorDebugReportCallback);

	// look for 2 queues one support Graphics and present operation and the second one with transfer operation
	pvr::utils::QueuePopulateInfo queuePopulateInfo = {
		pvrvk::QueueFlags::e_GRAPHICS_BIT,
		_deviceResources->surface,
	};
	pvr::utils::QueueAccessInfo queueAccessInfo;
	// create the Logical device
	_deviceResources->device = pvr::utils::createDeviceAndQueues(_deviceResources->instance->getPhysicalDevice(0), &queuePopulateInfo, 1, &queueAccessInfo);

	// Get the queues
	_deviceResources->queue = _deviceResources->device->getQueue(queueAccessInfo.familyId, queueAccessInfo.queueId);

	_deviceResources->vmaBufferAllocator = pvr::utils::vma::createAllocator(pvr::utils::vma::AllocatorCreateInfo(_deviceResources->device));
	_deviceResources->vmaImageAllocator = pvr::utils::vma::createAllocator(pvr::utils::vma::AllocatorCreateInfo(_deviceResources->device));

	// Create the commandpool & Descriptorpool
	_deviceResources->commandPool =
		_deviceResources->device->createCommandPool(_deviceResources->queue->getQueueFamilyId(), pvrvk::CommandPoolCreateFlags::e_RESET_COMMAND_BUFFER_BIT);

	_deviceResources->descriptorPool = _deviceResources->device->createDescriptorPool(pvrvk::DescriptorPoolCreateInfo()
																						  .addDescriptorInfo(pvrvk::DescriptorType::e_COMBINED_IMAGE_SAMPLER, 16)
																						  .addDescriptorInfo(pvrvk::DescriptorType::e_UNIFORM_BUFFER_DYNAMIC, 16)
																						  .addDescriptorInfo(pvrvk::DescriptorType::e_UNIFORM_BUFFER, 16)
																						  .setMaxDescriptorSets(16));

	// create a new commandpool for image uploading and upload the images in separate thread
	_deviceResources->uploader.init(_deviceResources->device, _deviceResources->queue, &_hostMutex);

	_deviceResources->asyncUpdateInfo.diffuseTex = _deviceResources->uploader.uploadTextureAsync(
		_deviceResources->loader.loadTextureAsync("Marble.pvr", this, pvr::TextureFileFormat::PVR), true, &DiffuseTextureDoneCallback, true);

	_deviceResources->asyncUpdateInfo.bumpTex = _deviceResources->uploader.uploadTextureAsync(
		_deviceResources->loader.loadTextureAsync("MarbleNormalMap.pvr", this, pvr::TextureFileFormat::PVR), true, &NormalTextureDoneCallback, true);

	pvrvk::SurfaceCapabilitiesKHR surfaceCapabilities = _deviceResources->instance->getPhysicalDevice(0)->getSurfaceCapabilities(_deviceResources->surface);

	// validate the supported swapchain image usage
	pvrvk::ImageUsageFlags swapchainImageUsage = pvrvk::ImageUsageFlags::e_COLOR_ATTACHMENT_BIT;
	if (pvr::utils::isImageUsageSupportedBySurface(surfaceCapabilities, pvrvk::ImageUsageFlags::e_TRANSFER_SRC_BIT))
	{
		swapchainImageUsage |= pvrvk::ImageUsageFlags::e_TRANSFER_SRC_BIT;
	}

	// Create the swapchain image and depthstencil image
	pvr::utils::createSwapchainAndDepthStencilImageAndViews(_deviceResources->device, _deviceResources->surface, getDisplayAttributes(), _deviceResources->swapchain,
		_deviceResources->depthStencilImages, swapchainImageUsage, pvrvk::ImageUsageFlags::e_DEPTH_STENCIL_ATTACHMENT_BIT | pvrvk::ImageUsageFlags::e_TRANSIENT_ATTACHMENT_BIT,
		&_deviceResources->vmaImageAllocator);

	pvr::utils::createOnscreenFramebufferAndRenderpass(_deviceResources->swapchain, &_deviceResources->depthStencilImages[0], _deviceResources->framebuffer);

	// Create the pipeline cache
	_deviceResources->pipelineCache = _deviceResources->device->createPipelineCache();

	// load the pipeline
	loadPipeline();
	createUbo();

	for (uint32_t i = 0; i < _deviceResources->swapchain->getSwapchainLength(); ++i)
	{
		_deviceResources->semaphorePresent[i] = _deviceResources->device->createSemaphore();
		_deviceResources->semaphoreImageAcquired[i] = _deviceResources->device->createSemaphore();
		_deviceResources->perFrameCommandBufferFence[i] = _deviceResources->device->createFence(pvrvk::FenceCreateFlags::e_SIGNALED_BIT);
		_deviceResources->perFrameAcquireFence[i] = _deviceResources->device->createFence(pvrvk::FenceCreateFlags::e_SIGNALED_BIT);

		_deviceResources->loadingTextCommandBuffer[i] = _deviceResources->commandPool->allocateCommandBuffer();
		_deviceResources->commandBuffers[i] = _deviceResources->commandPool->allocateCommandBuffer();
	}

	// load the vbo and ibo data
	_deviceResources->commandBuffers[0]->begin();
	bool requiresCommandBufferSubmission = false;
	pvr::utils::appendSingleBuffersFromModel(_deviceResources->device, *_scene, _deviceResources->vbos, _deviceResources->ibos, _deviceResources->commandBuffers[0],
		requiresCommandBufferSubmission, &_deviceResources->vmaBufferAllocator);

	_deviceResources->commandBuffers[0]->end();

	if (requiresCommandBufferSubmission)
	{
		pvrvk::SubmitInfo submitInfo;
		submitInfo.commandBuffers = &_deviceResources->commandBuffers[0];
		submitInfo.numCommandBuffers = 1;

		// submit the queue and wait for it to become idle
		_deviceResources->queue->submit(&submitInfo, 1);
		_deviceResources->queue->waitIdle();
	}

	//  Initialize UIRenderer
	_deviceResources->uiRenderer.init(
		getWidth(), getHeight(), isFullScreen(), _deviceResources->framebuffer[0]->getRenderPass(), 0, _deviceResources->commandPool, _deviceResources->queue);

	_deviceResources->uiRenderer.getDefaultTitle()->setText("Multithreading");
	_deviceResources->uiRenderer.getDefaultTitle()->commitUpdates();
	glm::vec3 from, to, up;
	float fov;
	_scene->getCameraProperties(0, fov, from, to, up);

	// Is the screen rotated
	bool bRotate = this->isScreenRotated();

	//  Calculate the projection and rotate it by 90 degree if the screen is rotated.
	_viewProj = (bRotate
			? pvr::math::perspectiveFov(
				  pvr::Api::Vulkan, fov, (float)this->getHeight(), (float)this->getWidth(), _scene->getCamera(0).getNear(), _scene->getCamera(0).getFar(), glm::pi<float>() * .5f)
			: pvr::math::perspectiveFov(pvr::Api::Vulkan, fov, (float)this->getWidth(), (float)this->getHeight(), _scene->getCamera(0).getNear(), _scene->getCamera(0).getFar()));

	_viewProj = _viewProj * glm::lookAt(from, to, up);
	recordLoadingCommandBuffer();
	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\brief  Code in releaseView() will be called by PVRShell when theapplication quits or before a change in the rendering context.
\return Return pvr::Result::Success if no error occurred
***********************************************************************************************************************/
pvr::Result VulkanMultithreading::releaseView()
{
	_deviceResources.reset();
	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\return Return pvr::Result::Success if no error occurred
\brief  Main rendering loop function of the program. The shell will call this function every frame.
***********************************************************************************************************************/
pvr::Result VulkanMultithreading::renderFrame()
{
	_deviceResources->perFrameAcquireFence[_frameId]->wait();
	_deviceResources->perFrameAcquireFence[_frameId]->reset();
	_deviceResources->swapchain->acquireNextImage(uint64_t(-1), _deviceResources->semaphoreImageAcquired[_frameId], _deviceResources->perFrameAcquireFence[_frameId]);

	const uint32_t swapchainIndex = _deviceResources->swapchain->getSwapchainIndex();

	_deviceResources->perFrameCommandBufferFence[swapchainIndex]->wait();
	_deviceResources->perFrameCommandBufferFence[swapchainIndex]->reset();

	pvrvk::SubmitInfo submitInfo;
	pvrvk::PipelineStageFlags waitDestStages = pvrvk::PipelineStageFlags::e_COLOR_ATTACHMENT_OUTPUT_BIT;
	submitInfo.waitDestStages = &waitDestStages;
	submitInfo.numCommandBuffers = 1;
	submitInfo.waitSemaphores = &_deviceResources->semaphoreImageAcquired[_frameId];
	submitInfo.numWaitSemaphores = 1;
	submitInfo.signalSemaphores = &_deviceResources->semaphorePresent[_frameId];
	submitInfo.numSignalSemaphores = 1;

	if (!_loadingDone)
	{
		if (_deviceResources->asyncUpdateInfo.bumpTex->isComplete() && _deviceResources->asyncUpdateInfo.diffuseTex->isComplete())
		{
			createImageSamplerDescriptorSets();
			updateTextureDescriptorSet();
			recordMainCommandBuffer();
			_loadingDone = true;
		}
	}
	if (!_loadingDone)
	{
		static float f = 0;
		f += getFrameTime() * .0005f;
		if (f > glm::pi<float>() * .5f)
		{
			f = 0;
		}
		_deviceResources->loadingText[swapchainIndex]->setColor(1.0f, 1.0f, 1.0f, f + .01f);
		_deviceResources->loadingText[swapchainIndex]->setScale(sin(f) * 3.f, sin(f) * 3.f);
		_deviceResources->loadingText[swapchainIndex]->commitUpdates();

		submitInfo.commandBuffers = &_deviceResources->loadingTextCommandBuffer[swapchainIndex];
	}

	if (_loadingDone)
	{
		// Calculate the model matrix
		glm::mat4 mModel = glm::rotate(_angleY, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::scale(glm::vec3(1.8f));
		_angleY += -RotateY * 0.05f * getFrameTime();

		// Set light Direction in model space
		//  The inverse of a rotation matrix is the transposed matrix
		//  Because of v * M = transpose(M) * v, this means:
		//  v * R == inverse(R) * v
		//  So we don't have to actually invert or transpose the matrix
		//  to transform back from world space to model space

		// update the ubo
		{
			UboPerMeshData srcWrite;
			srcWrite.lightDirModel = glm::vec3(LightDir * mModel);
			srcWrite.mvpMtx = _viewProj * mModel * _scene->getWorldMatrix(_scene->getNode(0).getObjectId());

			uint32_t currentDynamicSlice = swapchainIndex * _scene->getNumMeshNodes();
			_deviceResources->structuredMemoryView.getElement(0, 0, currentDynamicSlice).setValue(&srcWrite.mvpMtx);
			_deviceResources->structuredMemoryView.getElement(1, 0, currentDynamicSlice).setValue(&srcWrite.lightDirModel);

			// if the memory property flags used by the buffers' device memory do not contain e_HOST_COHERENT_BIT then we must flush the memory
			if (static_cast<uint32_t>(_deviceResources->ubo->getDeviceMemory()->getMemoryFlags() & pvrvk::MemoryPropertyFlags::e_HOST_COHERENT_BIT) == 0)
			{
				_deviceResources->ubo->getDeviceMemory()->flushRange(_deviceResources->structuredMemoryView.getDynamicSliceOffset(swapchainIndex * _scene->getNumMeshNodes()),
					_deviceResources->structuredMemoryView.getDynamicSliceSize() * _scene->getNumMeshNodes());
			}
		}
		submitInfo.commandBuffers = &_deviceResources->commandBuffers[swapchainIndex];
	}

	{
		std::lock_guard<pvr::async::Mutex> lock(_hostMutex);
		// submit
		_deviceResources->queue->submit(&submitInfo, 1, _deviceResources->perFrameCommandBufferFence[swapchainIndex]);
	}

	if (this->shouldTakeScreenshot())
	{
		pvr::utils::takeScreenshot(_deviceResources->swapchain, swapchainIndex, _deviceResources->commandPool, _deviceResources->queue, getScreenshotFileName(),
			&_deviceResources->vmaBufferAllocator, &_deviceResources->vmaImageAllocator);
	}

	// present
	pvrvk::PresentInfo present;
	present.swapchains = &_deviceResources->swapchain;
	present.imageIndices = &swapchainIndex;
	present.numSwapchains = 1;
	present.waitSemaphores = &_deviceResources->semaphorePresent[_frameId];
	present.numWaitSemaphores = 1;
	_deviceResources->queue->present(present);

	_frameId = (_frameId + 1) % _deviceResources->swapchain->getSwapchainLength();

	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\brief  Draws a assets::Mesh after the model view matrix has been set and the material prepared.
\param  nodeIndex Node index of the mesh to draw
***********************************************************************************************************************/
void VulkanMultithreading::drawMesh(pvrvk::CommandBuffer& commandBuffer, int nodeIndex)
{
	uint32_t meshId = _scene->getNode(nodeIndex).getObjectId();
	const pvr::assets::Mesh& mesh = _scene->getMesh(meshId);

	// bind the VBO for the mesh
	commandBuffer->bindVertexBuffer(_deviceResources->vbos[meshId], 0, 0);

	//  The geometry can be exported in 4 ways:
	//  - Indexed Triangle list
	//  - Non-Indexed Triangle list
	//  - Indexed Triangle strips
	//  - Non-Indexed Triangle strips
	if (mesh.getNumStrips() == 0)
	{
		// Indexed Triangle list
		if (_deviceResources->ibos[meshId].isValid())
		{
			commandBuffer->bindIndexBuffer(_deviceResources->ibos[meshId], 0, pvr::utils::convertToPVRVk(mesh.getFaces().getDataType()));
			commandBuffer->drawIndexed(0, mesh.getNumFaces() * 3, 0, 0, 1);
		}
		else
		{
			// Non-Indexed Triangle list
			commandBuffer->draw(0, mesh.getNumFaces() * 3, 0, 1);
		}
	}
	else
	{
		for (int i = 0; i < (int)mesh.getNumStrips(); ++i)
		{
			int offset = 0;
			if (_deviceResources->ibos[meshId].isValid())
			{
				// Indexed Triangle strips
				commandBuffer->bindIndexBuffer(_deviceResources->ibos[meshId], 0, pvr::utils::convertToPVRVk(mesh.getFaces().getDataType()));
				commandBuffer->drawIndexed(0, mesh.getStripLength(i) + 2, offset * 2, 0, 1);
			}
			else
			{
				// Non-Indexed Triangle strips
				commandBuffer->draw(0, mesh.getStripLength(i) + 2, 0, 1);
			}
			offset += mesh.getStripLength(i) + 2;
		}
	}
}

/*!*********************************************************************************************************************
\brief  Pre record the commands
***********************************************************************************************************************/
void VulkanMultithreading::recordMainCommandBuffer()
{
	const pvrvk::ClearValue clearValues[] = { pvrvk::ClearValue(0.00f, 0.70f, 0.67f, 1.f), pvrvk::ClearValue(1.f, 0u) };
	for (uint32_t i = 0; i < _deviceResources->swapchain->getSwapchainLength(); ++i)
	{
		pvrvk::CommandBuffer& commandBuffer = _deviceResources->commandBuffers[i];
		commandBuffer->begin();
		commandBuffer->beginRenderPass(_deviceResources->framebuffer[i], pvrvk::Rect2D(0, 0, getWidth(), getHeight()), true, clearValues, ARRAY_SIZE(clearValues));
		// enqueue the static states which wont be changed through out the frame
		commandBuffer->bindPipeline(_deviceResources->pipe);
		commandBuffer->bindDescriptorSet(pvrvk::PipelineBindPoint::e_GRAPHICS, _deviceResources->pipelayout, 0, _deviceResources->texDescSet, 0);
		commandBuffer->bindDescriptorSet(pvrvk::PipelineBindPoint::e_GRAPHICS, _deviceResources->pipelayout, 1, _deviceResources->uboDescSet[i]);
		drawMesh(commandBuffer, 0);

		// record the uirenderer commands
		_deviceResources->uiRenderer.beginRendering(commandBuffer);
		_deviceResources->uiRenderer.getDefaultTitle()->render();
		_deviceResources->uiRenderer.getSdkLogo()->render();
		_deviceResources->uiRenderer.endRendering();
		commandBuffer->endRenderPass();
		commandBuffer->end();
	}
}

/*!*********************************************************************************************************************
\brief  Pre record the commands
***********************************************************************************************************************/
void VulkanMultithreading::recordLoadingCommandBuffer()
{
	const pvrvk::ClearValue clearColor[2] = { pvrvk::ClearValue(0.00f, 0.70f, 0.67f, 1.f), pvrvk::ClearValue(1.f, 0u) };

	for (uint32_t i = 0; i < _deviceResources->swapchain->getSwapchainLength(); ++i)
	{
		pvrvk::CommandBuffer& commandBuffer = _deviceResources->loadingTextCommandBuffer[i];
		commandBuffer->begin();

		commandBuffer->beginRenderPass(_deviceResources->framebuffer[i], true, clearColor, ARRAY_SIZE(clearColor));

		_deviceResources->loadingText[i] = _deviceResources->uiRenderer.createText("Loading...");
		_deviceResources->loadingText[i]->commitUpdates();

		// record the uirenderer commands
		_deviceResources->uiRenderer.beginRendering(commandBuffer);
		_deviceResources->uiRenderer.getDefaultTitle()->render();
		_deviceResources->uiRenderer.getSdkLogo()->render();
		_deviceResources->loadingText[i]->render();
		_deviceResources->uiRenderer.endRendering();

		commandBuffer->endRenderPass();
		commandBuffer->end();
	}
}

/*!*********************************************************************************************************************
\return Return an auto ptr to the demo supplied by the user
\brief  This function must be implemented by the user of the shell. The user should return its
	Shell object defining the behavior of the application.
***********************************************************************************************************************/
std::unique_ptr<pvr::Shell> pvr::newDemo()
{
	return std::unique_ptr<pvr::Shell>(new VulkanMultithreading());
}
