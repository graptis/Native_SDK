/*!*********************************************************************************************************************
\File         VulkanBumpmap.cpp
\Title        Bump mapping
\Author       PowerVR by Imagination, Developer Technology Team
\Copyright    Copyright (c) Imagination Technologies Limited.
\brief      Shows how to perform tangent space bump mapping
***********************************************************************************************************************/
#include "PVRShell/PVRShell.h"
#include "PVRUtils/PVRUtilsVk.h"

const float RotateY = glm::pi<float>() / 150;
const glm::vec4 LightDir(.24f, .685f, -.685f, 0.0f);

/*!*********************************************************************************************************************
 shader attributes
 ***********************************************************************************************************************/
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

/*!*********************************************************************************************************************
 Class implementing the Shell functions.
 ***********************************************************************************************************************/
class VulkanBumpmap : public pvr::Shell
{
	struct DeviceResources
	{
		pvrvk::Instance instance;
		pvrvk::DebugReportCallback debugCallbacks[2];
		pvrvk::Device device;
		pvrvk::Swapchain swapchain;
		pvrvk::CommandPool commandPool;
		pvrvk::DescriptorPool descriptorPool;
		pvrvk::Queue queue;
		pvr::utils::vma::Allocator vmaBufferAllocator;
		pvr::utils::vma::Allocator vmaImageAllocator;
		pvrvk::Semaphore semaphoreImageAcquired[static_cast<uint32_t>(pvrvk::FrameworkCaps::MaxSwapChains)];
		pvrvk::Fence perFrameAcquireFence[static_cast<uint32_t>(pvrvk::FrameworkCaps::MaxSwapChains)];
		pvrvk::Semaphore semaphorePresent[static_cast<uint32_t>(pvrvk::FrameworkCaps::MaxSwapChains)];
		pvrvk::Fence perFrameCommandBufferFence[static_cast<uint32_t>(pvrvk::FrameworkCaps::MaxSwapChains)];
		std::vector<pvrvk::Buffer> vbos;
		std::vector<pvrvk::Buffer> ibos;
		pvrvk::DescriptorSetLayout texLayout;
		pvrvk::DescriptorSetLayout uboLayoutDynamic;
		pvrvk::PipelineLayout pipelayout;
		pvrvk::DescriptorSet texDescSet;
		pvrvk::GraphicsPipeline pipe;
		pvr::Multi<pvrvk::CommandBuffer> commandBuffers; // per swapchain
		pvr::Multi<pvrvk::Framebuffer> onScreenFramebuffers; // per swapchain
		pvr::Multi<pvrvk::ImageView> depthStencilImages;
		pvr::Multi<pvrvk::DescriptorSet> uboDescSets;
		pvr::utils::StructuredBufferView structuredBufferView;
		pvrvk::Buffer ubo;
		pvrvk::PipelineCache pipelineCache;

		// UIRenderer used to display text
		pvr::ui::UIRenderer uiRenderer;
		~DeviceResources()
		{
			if (device.isValid())
			{
				device->waitIdle();
			}
			int l = swapchain->getSwapchainLength();
			for (int i = 0; i < l; ++i)
			{
				if (perFrameAcquireFence[i].isValid())
					perFrameAcquireFence[i]->wait();
				if (perFrameCommandBufferFence[i].isValid())
					perFrameCommandBufferFence[i]->wait();
			}
		}
	};

	struct UboPerMeshData
	{
		glm::mat4 mvpMtx;
		glm::vec3 lightDirModel;
	};

	// 3D Model
	pvr::assets::ModelHandle _scene;

	// Projection and view matrix
	glm::mat4 _viewProj;

	uint32_t _frameId;
	// The translation and Rotate parameter of Model
	float _angleY;
	std::unique_ptr<DeviceResources> _deviceResources;

public:
	virtual pvr::Result initApplication();
	virtual pvr::Result initView();
	virtual pvr::Result releaseView();
	virtual pvr::Result quitApplication();
	virtual pvr::Result renderFrame();

	void createImageSamplerDescriptor(pvrvk::CommandBuffer& imageUploadCmd);
	void createUbo();
	void createPipeline();
	void drawMesh(pvrvk::CommandBuffer& commandBuffer, int i32NodeIndex);
	void recordCommandBuffer();
};

/*!*********************************************************************************************************************
\return return true if no error occurred
\brief  Loads the textures required for this training course
***********************************************************************************************************************/
void VulkanBumpmap::createImageSamplerDescriptor(pvrvk::CommandBuffer& imageUploadCmd)
{
	pvrvk::Device& device = _deviceResources->device;
	pvrvk::ImageView texBase;
	pvrvk::ImageView texNormalMap;

	// create the bilinear sampler
	pvrvk::SamplerCreateInfo samplerInfo;
	samplerInfo.magFilter = pvrvk::Filter::e_LINEAR;
	samplerInfo.minFilter = pvrvk::Filter::e_LINEAR;
	samplerInfo.mipMapMode = pvrvk::SamplerMipmapMode::e_NEAREST;
	pvrvk::Sampler samplerMipBilinear = device->createSampler(samplerInfo);

	samplerInfo.mipMapMode = pvrvk::SamplerMipmapMode::e_LINEAR;
	pvrvk::Sampler samplerTrilinear = device->createSampler(samplerInfo);

	texBase = pvr::utils::loadAndUploadImageAndView(_deviceResources->device, StatueTexFile, true, imageUploadCmd, *this, pvrvk::ImageUsageFlags::e_SAMPLED_BIT,
		pvrvk::ImageLayout::e_SHADER_READ_ONLY_OPTIMAL, nullptr, &_deviceResources->vmaBufferAllocator, &_deviceResources->vmaImageAllocator);
	texNormalMap = pvr::utils::loadAndUploadImageAndView(_deviceResources->device, StatueNormalMapFile, true, imageUploadCmd, *this, pvrvk::ImageUsageFlags::e_SAMPLED_BIT,
		pvrvk::ImageLayout::e_SHADER_READ_ONLY_OPTIMAL, nullptr, &_deviceResources->vmaBufferAllocator, &_deviceResources->vmaImageAllocator);

	texBase->setObjectName("Base diffuse ImageView");
	texNormalMap->setObjectName("Normal map ImgaeView");

	// create the descriptor set
	_deviceResources->texDescSet = _deviceResources->descriptorPool->allocateDescriptorSet(_deviceResources->texLayout);
	_deviceResources->texDescSet->setObjectName("Texture DescriptorSet");

	pvrvk::WriteDescriptorSet writeDescSets[2] = { pvrvk::WriteDescriptorSet(pvrvk::DescriptorType::e_COMBINED_IMAGE_SAMPLER, _deviceResources->texDescSet, 0),
		pvrvk::WriteDescriptorSet(pvrvk::DescriptorType::e_COMBINED_IMAGE_SAMPLER, _deviceResources->texDescSet, 1) };
	writeDescSets[0].setImageInfo(0, pvrvk::DescriptorImageInfo(texBase, samplerMipBilinear));
	writeDescSets[1].setImageInfo(0, pvrvk::DescriptorImageInfo(texNormalMap, samplerTrilinear));

	_deviceResources->device->updateDescriptorSets(writeDescSets, ARRAY_SIZE(writeDescSets), nullptr, 0);
}

void VulkanBumpmap::createUbo()
{
	pvrvk::WriteDescriptorSet descUpdate[pvrvk::FrameworkCaps::MaxSwapChains];
	{
		pvr::utils::StructuredMemoryDescription desc;
		desc.addElement("MVPMatrix", pvr::GpuDatatypes::mat4x4);
		desc.addElement("LightDirModel", pvr::GpuDatatypes::vec3);

		_deviceResources->structuredBufferView.initDynamic(desc, _scene->getNumMeshNodes() * _deviceResources->swapchain->getSwapchainLength(), pvr::BufferUsageFlags::UniformBuffer,
			static_cast<uint32_t>(_deviceResources->device->getPhysicalDevice()->getProperties().getLimits().getMinUniformBufferOffsetAlignment()));
		_deviceResources->ubo = pvr::utils::createBuffer(_deviceResources->device, _deviceResources->structuredBufferView.getSize(), pvrvk::BufferUsageFlags::e_UNIFORM_BUFFER_BIT,
			pvrvk::MemoryPropertyFlags::e_HOST_VISIBLE_BIT,
			pvrvk::MemoryPropertyFlags::e_DEVICE_LOCAL_BIT | pvrvk::MemoryPropertyFlags::e_HOST_VISIBLE_BIT | pvrvk::MemoryPropertyFlags::e_HOST_COHERENT_BIT,
			&_deviceResources->vmaBufferAllocator, pvr::utils::vma::AllocationCreateFlags::e_MAPPED_BIT);
		_deviceResources->structuredBufferView.pointToMappedMemory(_deviceResources->ubo->getDeviceMemory()->getMappedData());
		_deviceResources->ubo->setObjectName("Object Ubo");
	}

	for (uint32_t i = 0; i < _deviceResources->swapchain->getSwapchainLength(); ++i)
	{
		_deviceResources->uboDescSets.add(_deviceResources->descriptorPool->allocateDescriptorSet(_deviceResources->uboLayoutDynamic));
		_deviceResources->uboDescSets[i]->setObjectName(std::string("Ubo DescriptorSet [") + std::to_string(i) + "]");

		descUpdate[i]
			.set(pvrvk::DescriptorType::e_UNIFORM_BUFFER_DYNAMIC, _deviceResources->uboDescSets[i])
			.setBufferInfo(0, pvrvk::DescriptorBufferInfo(_deviceResources->ubo, 0, _deviceResources->structuredBufferView.getDynamicSliceSize()));
	}
	_deviceResources->device->updateDescriptorSets(descUpdate, _deviceResources->swapchain->getSwapchainLength(), nullptr, 0);
}

/*!*********************************************************************************************************************
\return  Return true if no error occurred
\brief  Loads and compiles the shaders and create a pipeline
***********************************************************************************************************************/
void VulkanBumpmap::createPipeline()
{
	pvrvk::PipelineColorBlendAttachmentState colorAttachemtState;
	pvrvk::GraphicsPipelineCreateInfo pipeInfo;
	colorAttachemtState.setBlendEnable(false);

	//--- create the texture-sampler descriptor set layout
	{
		pvrvk::DescriptorSetLayoutCreateInfo descSetLayoutInfo;
		descSetLayoutInfo.setBinding(0, pvrvk::DescriptorType::e_COMBINED_IMAGE_SAMPLER, 1, pvrvk::ShaderStageFlags::e_FRAGMENT_BIT); /*binding 0*/
		descSetLayoutInfo.setBinding(1, pvrvk::DescriptorType::e_COMBINED_IMAGE_SAMPLER, 1, pvrvk::ShaderStageFlags::e_FRAGMENT_BIT); /*binding 1*/
		_deviceResources->texLayout = _deviceResources->device->createDescriptorSetLayout(descSetLayoutInfo);
	}

	//--- create the ubo descriptorset layout
	{
		pvrvk::DescriptorSetLayoutCreateInfo descSetLayoutInfo;
		descSetLayoutInfo.setBinding(0, pvrvk::DescriptorType::e_UNIFORM_BUFFER_DYNAMIC, 1, pvrvk::ShaderStageFlags::e_VERTEX_BIT); /*binding 0*/
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

	const pvrvk::Rect2D rect(0, 0, _deviceResources->swapchain->getDimension().getWidth(), _deviceResources->swapchain->getDimension().getHeight());
	pipeInfo.viewport.setViewportAndScissor(
		0, pvrvk::Viewport((float)rect.getOffset().getX(), (float)rect.getOffset().getY(), (float)rect.getExtent().getWidth(), (float)rect.getExtent().getHeight()), rect);
	pipeInfo.rasterizer.setCullMode(pvrvk::CullModeFlags::e_BACK_BIT);
	pipeInfo.colorBlend.setAttachmentState(0, colorAttachemtState);

	pvr::Stream::ptr_type vertSource = getAssetStream(VertShaderSrcFile);
	pvr::Stream::ptr_type fragSource = getAssetStream(FragShaderSrcFile);

	pipeInfo.vertexShader.setShader(_deviceResources->device->createShader(vertSource->readToEnd<uint32_t>()));
	pipeInfo.fragmentShader.setShader(_deviceResources->device->createShader(fragSource->readToEnd<uint32_t>()));

	const pvr::assets::Mesh& mesh = _scene->getMesh(0);
	pipeInfo.inputAssembler.setPrimitiveTopology(pvr::utils::convertToPVRVk(mesh.getPrimitiveType()));
	pipeInfo.pipelineLayout = _deviceResources->pipelayout;
	pipeInfo.renderPass = _deviceResources->onScreenFramebuffers[0]->getRenderPass();
	pipeInfo.subpass = 0;
	// Enable z-buffer test. We are using a projection matrix optimized for a floating point depth buffer,
	// so the depth test and clear value need to be inverted (1 becomes near, 0 becomes far).
	pipeInfo.depthStencil.enableDepthTest(true);
	pipeInfo.depthStencil.setDepthCompareFunc(pvrvk::CompareOp::e_LESS);
	pipeInfo.depthStencil.enableDepthWrite(true);
	pvr::utils::populateInputAssemblyFromMesh(
		mesh, VertexAttribBindings, sizeof(VertexAttribBindings) / sizeof(VertexAttribBindings[0]), pipeInfo.vertexInput, pipeInfo.inputAssembler);
	_deviceResources->pipe = _deviceResources->device->createGraphicsPipeline(pipeInfo, _deviceResources->pipelineCache);
	_deviceResources->pipe->setObjectName("Bumpmap GraphicsPipeline");
}

/*!*********************************************************************************************************************
\return Return Result::Success if no error occurred
\brief  Code in initApplication() will be called by Shell once per run, before the rendering context is created.
	Used to initialize variables that are not dependent on it (e.g. external modules, loading meshes, etc.)
	If the rendering context is lost, initApplication() will not be called again.
***********************************************************************************************************************/
pvr::Result VulkanBumpmap::initApplication()
{
	// Load the _scene
	pvr::assets::helper::loadModel(*this, SceneFile, _scene);
	_angleY = 0.0f;
	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\return Return Result::Success if no error occurred
\brief  Code in quitApplication() will be called by PVRShell once per run, just before exiting the program.
	If the rendering context is lost, quitApplication() will not be called.x
***********************************************************************************************************************/
pvr::Result VulkanBumpmap::quitApplication()
{
	_scene.reset();
	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\return Return Result::Success if no error occurred
\brief  Code in initView() will be called by Shell upon initialization or after a change in the rendering context.
	Used to initialize variables that are dependent on the rendering context (e.g. textures, vertex buffers, etc.)
***********************************************************************************************************************/
pvr::Result VulkanBumpmap::initView()
{
	_frameId = 0;
	_deviceResources = std::unique_ptr<DeviceResources>(new DeviceResources());

	// Create instance and retrieve compatible physical devices
	_deviceResources->instance = pvr::utils::createInstance(this->getApplicationName());

	// Create the surface
	pvrvk::Surface surface = pvr::utils::createSurface(_deviceResources->instance, _deviceResources->instance->getPhysicalDevice(0), this->getWindow(), this->getDisplay());

	// Add Debug Report Callbacks
	// Add a Debug Report Callback for logging messages for events of all supported types.
	_deviceResources->debugCallbacks[0] = pvr::utils::createDebugReportCallback(_deviceResources->instance);
	// Add a second Debug Report Callback for throwing exceptions for Error events.
	_deviceResources->debugCallbacks[1] =
		pvr::utils::createDebugReportCallback(_deviceResources->instance, pvrvk::DebugReportFlagsEXT::e_ERROR_BIT_EXT, pvr::utils::throwOnErrorDebugReportCallback);

	const pvr::utils::QueuePopulateInfo queuePopulateInfo = { pvrvk::QueueFlags::e_GRAPHICS_BIT, surface };
	pvr::utils::QueueAccessInfo queueAccessInfo;
	_deviceResources->device = pvr::utils::createDeviceAndQueues(_deviceResources->instance->getPhysicalDevice(0), &queuePopulateInfo, 1, &queueAccessInfo);
	_deviceResources->queue = _deviceResources->device->getQueue(queueAccessInfo.familyId, queueAccessInfo.queueId);

	_deviceResources->vmaBufferAllocator = pvr::utils::vma::createAllocator(pvr::utils::vma::AllocatorCreateInfo(_deviceResources->device));
	_deviceResources->vmaImageAllocator = pvr::utils::vma::createAllocator(pvr::utils::vma::AllocatorCreateInfo(_deviceResources->device));

	pvrvk::SurfaceCapabilitiesKHR surfaceCapabilities = _deviceResources->instance->getPhysicalDevice(0)->getSurfaceCapabilities(surface);

	// validate the supported swapchain image usage
	pvrvk::ImageUsageFlags swapchainImageUsage = pvrvk::ImageUsageFlags::e_COLOR_ATTACHMENT_BIT;
	if (pvr::utils::isImageUsageSupportedBySurface(surfaceCapabilities, pvrvk::ImageUsageFlags::e_TRANSFER_SRC_BIT))
	{
		swapchainImageUsage |= pvrvk::ImageUsageFlags::e_TRANSFER_SRC_BIT;
	}

	//---------------
	// Create the swapchain
	pvr::utils::createSwapchainAndDepthStencilImageAndViews(_deviceResources->device, surface, getDisplayAttributes(), _deviceResources->swapchain,
		_deviceResources->depthStencilImages, swapchainImageUsage, pvrvk::ImageUsageFlags::e_DEPTH_STENCIL_ATTACHMENT_BIT | pvrvk::ImageUsageFlags::e_TRANSIENT_ATTACHMENT_BIT,
		&_deviceResources->vmaImageAllocator);

	//---------------
	// Create the commandpool and descriptorset pool
	_deviceResources->commandPool =
		_deviceResources->device->createCommandPool(_deviceResources->queue->getQueueFamilyId(), pvrvk::CommandPoolCreateFlags::e_RESET_COMMAND_BUFFER_BIT);
	_deviceResources->commandPool->setObjectName("Main Command Pool");

	_deviceResources->descriptorPool = _deviceResources->device->createDescriptorPool(pvrvk::DescriptorPoolCreateInfo()
																						  .addDescriptorInfo(pvrvk::DescriptorType::e_COMBINED_IMAGE_SAMPLER, 16)
																						  .addDescriptorInfo(pvrvk::DescriptorType::e_UNIFORM_BUFFER_DYNAMIC, 16)
																						  .addDescriptorInfo(pvrvk::DescriptorType::e_UNIFORM_BUFFER, 16)
																						  .setMaxDescriptorSets(16));
	_deviceResources->descriptorPool->setObjectName("Main Descriptor Pool");

	// create an onscreen framebuffer per swap chain
	pvr::utils::createOnscreenFramebufferAndRenderpass(_deviceResources->swapchain, &_deviceResources->depthStencilImages[0], _deviceResources->onScreenFramebuffers);

	for (uint32_t i = 0; i < _deviceResources->swapchain->getSwapchainLength(); ++i)
	{
		_deviceResources->onScreenFramebuffers[i]->setObjectName(std::string("Main Framebuffer [") + std::to_string(i) + "]");
		_deviceResources->swapchain->getImageView(i)->setObjectName(std::string("Swapchain Image View [") + std::to_string(i) + "]");
		_deviceResources->depthStencilImages[i]->setObjectName(std::string("Depth Stencil Image View [") + std::to_string(i) + "]");
	}

	// Create the pipeline cache
	_deviceResources->pipelineCache = _deviceResources->device->createPipelineCache();

	//---------------
	// load the pipeline
	createPipeline();

	for (uint32_t i = 0; i < _deviceResources->swapchain->getSwapchainLength(); ++i)
	{
		// create the per swapchain command buffers
		_deviceResources->commandBuffers[i] = _deviceResources->commandPool->allocateCommandBuffer();
		_deviceResources->commandBuffers[i]->setObjectName(std::string("Main CommandBuffer [") + std::to_string(i) + "]");
		if (i == 0)
		{
			_deviceResources->commandBuffers[0]->begin();
		}

		_deviceResources->semaphorePresent[i] = _deviceResources->device->createSemaphore();
		_deviceResources->semaphorePresent[i]->setObjectName(std::string("Presentation Semaphore [") + std::to_string(i) + "]");
		_deviceResources->semaphoreImageAcquired[i] = _deviceResources->device->createSemaphore();
		_deviceResources->semaphoreImageAcquired[i]->setObjectName(std::string("Image Acquisition Semaphore [") + std::to_string(i) + "]");
		_deviceResources->perFrameCommandBufferFence[i] = _deviceResources->device->createFence(pvrvk::FenceCreateFlags::e_SIGNALED_BIT);
		_deviceResources->perFrameCommandBufferFence[i]->setObjectName(std::string("Per Frame Command Buffer Fence [") + std::to_string(i) + "]");
		_deviceResources->perFrameAcquireFence[i] = _deviceResources->device->createFence(pvrvk::FenceCreateFlags::e_SIGNALED_BIT);
		_deviceResources->perFrameAcquireFence[i]->setObjectName(std::string("Per Frame Image Acquisition Fence [") + std::to_string(i) + "]");
	}

	// load the vbo and ibo data
	bool requiresCommandBufferSubmission = false;
	pvr::utils::appendSingleBuffersFromModel(_deviceResources->device, *_scene, _deviceResources->vbos, _deviceResources->ibos, _deviceResources->commandBuffers[0],
		requiresCommandBufferSubmission, &_deviceResources->vmaBufferAllocator);

	// create the image samplers
	createImageSamplerDescriptor(_deviceResources->commandBuffers[0]);
	_deviceResources->commandBuffers[0]->end();

	pvrvk::SubmitInfo submitInfo;
	submitInfo.commandBuffers = &_deviceResources->commandBuffers[0];
	submitInfo.numCommandBuffers = 1;
	_deviceResources->queue->submit(&submitInfo, 1);
	_deviceResources->queue->waitIdle();

	//  Initialize UIRenderer
	_deviceResources->uiRenderer.init(
		getWidth(), getHeight(), isFullScreen(), _deviceResources->onScreenFramebuffers[0]->getRenderPass(), 0, _deviceResources->commandPool, _deviceResources->queue);

	_deviceResources->uiRenderer.getDefaultTitle()->setText("Bumpmap");
	_deviceResources->uiRenderer.getDefaultTitle()->commitUpdates();

	// create the uniform buffers
	createUbo();

	glm::vec3 from, to, up;
	float fov;
	_scene->getCameraProperties(0, fov, from, to, up);

	// Is the screen rotated
	const bool bRotate = this->isScreenRotated();

	//  Calculate the projection and rotate it by 90 degree if the screen is rotated.
	_viewProj = (bRotate
			? pvr::math::perspectiveFov(
				  pvr::Api::Vulkan, fov, (float)this->getHeight(), (float)this->getWidth(), _scene->getCamera(0).getNear(), _scene->getCamera(0).getFar(), glm::pi<float>() * .5f)
			: pvr::math::perspectiveFov(pvr::Api::Vulkan, fov, (float)this->getWidth(), (float)this->getHeight(), _scene->getCamera(0).getNear(), _scene->getCamera(0).getFar()));

	_viewProj = _viewProj * glm::lookAt(from, to, up);

	// record the command buffers
	recordCommandBuffer();
	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\brief  Code in releaseView() will be called by PVRShell when theapplication quits or before a change in the rendering context.
\return Return Result::Success if no error occurred
***********************************************************************************************************************/
pvr::Result VulkanBumpmap::releaseView()
{
	_deviceResources.reset();
	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\return Return Result::Success if no error occurred
\brief  Main rendering loop function of the program. The shell will call this function every frame.
***********************************************************************************************************************/
pvr::Result VulkanBumpmap::renderFrame()
{
	_deviceResources->perFrameAcquireFence[_frameId]->wait();
	_deviceResources->perFrameAcquireFence[_frameId]->reset();
	_deviceResources->swapchain->acquireNextImage(uint64_t(-1), _deviceResources->semaphoreImageAcquired[_frameId], _deviceResources->perFrameAcquireFence[_frameId]);

	const uint32_t swapchainIndex = _deviceResources->swapchain->getSwapchainIndex();

	_deviceResources->perFrameCommandBufferFence[swapchainIndex]->wait();
	_deviceResources->perFrameCommandBufferFence[swapchainIndex]->reset();

	// Calculate the model matrix
	const glm::mat4 mModel = glm::rotate(_angleY, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::scale(glm::vec3(1.8f));
	_angleY += -RotateY * 0.05f * getFrameTime();

	// Set light Direction in model space
	//  The inverse of a rotation matrix is the transposed matrix
	//  Because of v * M = transpose(M) * v, this means:
	//  v * R == inverse(R) * v
	//  So we don't have to actually invert or transpose the matrix
	//  to transform back from world space to model space

	//---------------
	// update the ubo
	UboPerMeshData srcWrite;
	srcWrite.lightDirModel = glm::vec3(LightDir * mModel);
	srcWrite.mvpMtx = _viewProj * mModel * _scene->getWorldMatrix(_scene->getNode(0).getObjectId());

	_deviceResources->structuredBufferView.getElementByName("MVPMatrix", 0, swapchainIndex).setValue(&srcWrite.mvpMtx);
	_deviceResources->structuredBufferView.getElementByName("LightDirModel", 0, swapchainIndex).setValue(&srcWrite.lightDirModel);

	// if the memory property flags used by the buffers' device memory do not contain e_HOST_COHERENT_BIT then we must flush the memory
	if (static_cast<uint32_t>(_deviceResources->ubo->getDeviceMemory()->getMemoryFlags() & pvrvk::MemoryPropertyFlags::e_HOST_COHERENT_BIT) == 0)
	{
		_deviceResources->ubo->getDeviceMemory()->flushRange(
			_deviceResources->structuredBufferView.getDynamicSliceOffset(swapchainIndex), _deviceResources->structuredBufferView.getDynamicSliceSize());
	}

	//---------------
	// SUBMIT
	pvrvk::SubmitInfo submitInfo;
	pvrvk::PipelineStageFlags pipeWaitStageFlags = pvrvk::PipelineStageFlags::e_COLOR_ATTACHMENT_OUTPUT_BIT;
	submitInfo.commandBuffers = &_deviceResources->commandBuffers[swapchainIndex];
	submitInfo.numCommandBuffers = 1;
	submitInfo.waitSemaphores = &_deviceResources->semaphoreImageAcquired[_frameId];
	submitInfo.numWaitSemaphores = 1;
	submitInfo.signalSemaphores = &_deviceResources->semaphorePresent[_frameId];
	submitInfo.numSignalSemaphores = 1;
	submitInfo.waitDestStages = &pipeWaitStageFlags;
	_deviceResources->queue->submit(&submitInfo, 1, _deviceResources->perFrameCommandBufferFence[swapchainIndex]);

	if (this->shouldTakeScreenshot())
	{
		pvr::utils::takeScreenshot(_deviceResources->swapchain, swapchainIndex, _deviceResources->commandPool, _deviceResources->queue, this->getScreenshotFileName(),
			&_deviceResources->vmaBufferAllocator, &_deviceResources->vmaImageAllocator);
	}

	//---------------
	// PRESENT
	pvrvk::PresentInfo presentInfo;
	presentInfo.swapchains = &_deviceResources->swapchain;
	presentInfo.numSwapchains = 1;
	presentInfo.waitSemaphores = &_deviceResources->semaphorePresent[_frameId];
	presentInfo.numWaitSemaphores = 1;
	presentInfo.imageIndices = &swapchainIndex;
	_deviceResources->queue->present(presentInfo);

	_frameId = (_frameId + 1) % _deviceResources->swapchain->getSwapchainLength();

	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\brief  Draws a assets::Mesh after the model view matrix has been set and the material prepared.
\param  nodeIndex Node index of the mesh to draw
***********************************************************************************************************************/
void VulkanBumpmap::drawMesh(pvrvk::CommandBuffer& commandBuffer, int nodeIndex)
{
	const uint32_t meshId = _scene->getNode(nodeIndex).getObjectId();
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
		uint32_t offset = 0;
		for (uint32_t i = 0; i < mesh.getNumStrips(); ++i)
		{
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
void VulkanBumpmap::recordCommandBuffer()
{
	const uint32_t numSwapchains = _deviceResources->swapchain->getSwapchainLength();
	pvrvk::ClearValue clearValues[2] = { pvrvk::ClearValue(0.00f, 0.70f, 0.67f, 1.f), pvrvk::ClearValue(1.f, 0u) };
	for (uint32_t i = 0; i < numSwapchains; ++i)
	{
		// begin recording commands for the current swap chain command buffer
		_deviceResources->commandBuffers[i]->begin();
		_deviceResources->commandBuffers[i]->debugMarkerBeginEXT("VulkanBumpmap: Main CommandBuffer");

		// begin the render pass
		_deviceResources->commandBuffers[i]->beginRenderPass(
			_deviceResources->onScreenFramebuffers[i], pvrvk::Rect2D(0, 0, getWidth(), getHeight()), true, clearValues, ARRAY_SIZE(clearValues));

		// calculate the dynamic offset to use
		const uint32_t dynamicOffset = _deviceResources->structuredBufferView.getDynamicSliceOffset(i);
		// enqueue the static states which wont be changed through out the frame
		_deviceResources->commandBuffers[i]->bindPipeline(_deviceResources->pipe);
		_deviceResources->commandBuffers[i]->bindDescriptorSet(pvrvk::PipelineBindPoint::e_GRAPHICS, _deviceResources->pipelayout, 0, _deviceResources->texDescSet, nullptr);

		_deviceResources->commandBuffers[i]->bindDescriptorSet(
			pvrvk::PipelineBindPoint::e_GRAPHICS, _deviceResources->pipelayout, 1, _deviceResources->uboDescSets[i], &dynamicOffset, 1);

		_deviceResources->commandBuffers[i]->debugMarkerBeginEXT("VulkanBumpmap: Rendering VulkanBumpmap Mesh");
		drawMesh(_deviceResources->commandBuffers[i], 0);
		_deviceResources->commandBuffers[i]->debugMarkerEndEXT();

		// record the ui renderer commands
		_deviceResources->uiRenderer.beginRendering(_deviceResources->commandBuffers[i]);
		_deviceResources->uiRenderer.getDefaultTitle()->render();
		_deviceResources->uiRenderer.getSdkLogo()->render();
		_deviceResources->uiRenderer.endRendering();

		// end the renderpass
		_deviceResources->commandBuffers[i]->endRenderPass();

		_deviceResources->commandBuffers[i]->debugMarkerEndEXT();

		// end recording commands for the current command buffer
		_deviceResources->commandBuffers[i]->end();
	}
}

/*!*********************************************************************************************************************
\return Return an auto ptr to the demo supplied by the user
\brief  This function must be implemented by the user of the shell. The user should return its
	Shell object defining the behavior of the application.
***********************************************************************************************************************/
std::unique_ptr<pvr::Shell> pvr::newDemo()
{
	return std::unique_ptr<pvr::Shell>(new VulkanBumpmap());
}
