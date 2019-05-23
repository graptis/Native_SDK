/*!*********************************************************************************************************************
\File         OGLESPVRScopeExample.cpp
\Title        PVRScopeExample
\Author       PowerVR by Imagination, Developer Technology Team
\Copyright    Copyright (c) Imagination Technologies Limited.
\brief  Shows how to use our example PVRScope graph code.
***********************************************************************************************************************/
#include "PVRShell/PVRShell.h"
#include "PVRUtils/PVRUtilsVk.h"
#include "PVRScopeGraph.h"

#if !defined(_WIN32) || defined(__WINSCW__)
#define _stricmp strcasecmp
#endif

// Shader Source
const char FragShaderSrcFile[] = "FragShader_vk.fsh.spv";
const char VertShaderSrcFile[] = "VertShader_vk.vsh.spv";

// PVR texture files
const char TextureFile[] = "Marble.pvr";

// POD _scene files
const char SceneFile[] = "Satyr.pod";
enum
{
	NumModelInstance = 2
};

namespace MaterialUboElements {
enum Enum
{
	ViewLightDirection,
	AlbedoModulation,
	SpecularExponent,
	Metallicity,
	Reflectivity,
	Count
};
std::pair<pvr::StringHash, pvr::GpuDatatypes> Mapping[Count] = {
	{ "ViewLightDirection", pvr::GpuDatatypes::vec3 },
	{ "AlbedoModulation", pvr::GpuDatatypes::vec3 },
	{ "SpecularExponent", pvr::GpuDatatypes::Float },
	{ "Metallicity", pvr::GpuDatatypes::Float },
	{ "Reflectivity", pvr::GpuDatatypes::Float },
};
} // namespace MaterialUboElements

struct DeviceResources
{
	pvrvk::Instance instance;
	pvrvk::DebugReportCallback debugCallbacks[2];
	pvrvk::Device device;
	pvrvk::Swapchain swapchain;
	pvrvk::Queue queue;
	pvrvk::DescriptorPool descriptorPool;
	pvrvk::CommandPool commandPool;
	pvr::utils::vma::Allocator vmaBufferAllocator;
	pvr::utils::vma::Allocator vmaImageAllocator;
	pvr::Multi<pvrvk::Framebuffer> onScreenFramebuffer;
	pvr::Multi<pvrvk::ImageView> depthStencilImages;
	pvr::Multi<pvrvk::Semaphore> semaphoreAcquire;
	pvr::Multi<pvrvk::Semaphore> semaphoreSubmit;
	pvr::Multi<pvrvk::Fence> perFrameFence;
	pvr::Multi<pvrvk::DescriptorSet> mvpDescriptor;
	pvr::Multi<pvrvk::DescriptorSet> materialDescriptor;
	pvr::Multi<pvrvk::CommandBuffer> commandBuffer;
	pvrvk::GraphicsPipeline pipeline;
	pvrvk::ImageView texture;
	std::vector<pvrvk::Buffer> ibos;
	std::vector<pvrvk::Buffer> vbos;
	pvrvk::DescriptorSet texSamplerDescriptor;
	pvrvk::DescriptorSetLayout texSamplerLayout;
	pvrvk::DescriptorSetLayout uboLayoutVert;
	pvrvk::DescriptorSetLayout uboLayoutFrag;
	pvr::utils::StructuredBufferView mvpUboView;
	pvrvk::Buffer mvpUbo;
	pvr::utils::StructuredBufferView materialUboView;
	pvrvk::Buffer materialUbo;
	pvrvk::PipelineCache pipelineCache;

	// UIRenderer used to display text
	pvr::ui::UIRenderer uiRenderer;

	PVRScopeGraph scopeGraph;

	~DeviceResources()
	{
		if (device.isValid())
		{
			device->waitIdle();
			int l = swapchain->getSwapchainLength();
			for (int i = 0; i < l; ++i)
			{
				if (perFrameFence[i].isValid())
					perFrameFence[i]->wait();
				if (perFrameFence[i].isValid())
					perFrameFence[i]->wait();
			}
		}
	}
};

/*!*********************************************************************************************************************
\brief Class implementing the Shell functions.
***********************************************************************************************************************/
class VulkanPVRScopeExample : public pvr::Shell
{
	std::unique_ptr<DeviceResources> _deviceResources;

	// 3D Model
	pvr::assets::ModelHandle _scene;

	// Projection and view matrices
	struct Uniforms
	{
		glm::mat4 projectionMtx;
		glm::mat4 _viewMtx;
		glm::mat4 mvpMatrix1;
		glm::mat4 mvpMatrix2;
		glm::mat4 mvMatrix1;
		glm::mat4 mvMatrix2;
		glm::mat3 mvITMatrix1;
		glm::mat3 mvITMatrix2;
		glm::vec3 lightDirView;
		float specularExponent;
		float metallicity;
		float reflectivity;
		glm::vec3 albedo;
	} _progUniforms;

	struct MaterialData
	{
		glm::vec3 lightDirView;
		glm::vec3 albedoMod;
		float specExponent;
		float metalicity;
		float reflectivity;
	} _materialData;

	// The translation and Rotate parameter of Model
	float _angleY;

	// Variables for the graphing code
	int32_t _selectedCounter;
	int32_t _interval;
	glm::mat4 _projMtx;
	glm::mat4 _viewMtx;
	uint32_t _frameId;

public:
	virtual pvr::Result initApplication();
	virtual pvr::Result initView();
	virtual pvr::Result releaseView();
	virtual pvr::Result quitApplication();
	virtual pvr::Result renderFrame();

	void eventMappedInput(pvr::SimplifiedInput key);

	void updateDescription();
	void recordCommandBuffer(uint32_t swapchain);
	void createTexSamplerDescriptorSet(pvrvk::CommandBuffer& imageUploadCmd);
	void createUboDescriptorSet();
	void createPipeline();
	void loadVbos(pvrvk::CommandBuffer& uploadCmd);
	void updateMVPMatrix(uint32_t swapchain);
	void drawMesh(int32_t nodeIndex, pvrvk::CommandBuffer& command);
};

/*!*********************************************************************************************************************
\brief Handle input key events
\param key key event to handle
************************************************************************************************************************/
void VulkanPVRScopeExample::eventMappedInput(pvr::SimplifiedInput key)
{
	// Keyboard input (cursor up/down to cycle through counters)
	switch (key)
	{
	case pvr::SimplifiedInput::Up:
	case pvr::SimplifiedInput::Right:
	{
		_selectedCounter++;
		if (_selectedCounter > (int)_deviceResources->scopeGraph.getCounterNum())
		{
			_selectedCounter = _deviceResources->scopeGraph.getCounterNum();
		}
	}
	break;
	case pvr::SimplifiedInput::Down:
	case pvr::SimplifiedInput::Left:
	{
		_selectedCounter--;
		if (_selectedCounter < 0)
		{
			_selectedCounter = 0;
		}
	}
	break;
	case pvr::SimplifiedInput::Action1:
	{
		_deviceResources->scopeGraph.showCounter(_selectedCounter, !_deviceResources->scopeGraph.isCounterShown(_selectedCounter));
	}
	break;
	// Keyboard input (cursor left/right to change active group)
	case pvr::SimplifiedInput::ActionClose:
		exitShell();
		break;
	default:
		break;
	}

	updateDescription();
}

/*!*********************************************************************************************************************
\brief Loads the textures required for this training course
\return Return true if no error occurred
***********************************************************************************************************************/
void VulkanPVRScopeExample::createTexSamplerDescriptorSet(pvrvk::CommandBuffer& imageUploadCmd)
{
	_deviceResources->texture = pvr::utils::loadAndUploadImageAndView(_deviceResources->device, TextureFile, true, imageUploadCmd, *this, pvrvk::ImageUsageFlags::e_SAMPLED_BIT,
		pvrvk::ImageLayout::e_SHADER_READ_ONLY_OPTIMAL, nullptr, &_deviceResources->vmaBufferAllocator, &_deviceResources->vmaImageAllocator);

	// create the bilinear sampler
	pvrvk::SamplerCreateInfo samplerDesc;
	samplerDesc.minFilter = pvrvk::Filter::e_LINEAR;
	samplerDesc.mipMapMode = pvrvk::SamplerMipmapMode::e_NEAREST;
	samplerDesc.magFilter = pvrvk::Filter::e_LINEAR;
	pvrvk::Sampler bilinearSampler = _deviceResources->device->createSampler(samplerDesc);
	_deviceResources->texSamplerDescriptor = _deviceResources->descriptorPool->allocateDescriptorSet(_deviceResources->texSamplerLayout);

	pvrvk::WriteDescriptorSet writeDescSet(pvrvk::DescriptorType::e_COMBINED_IMAGE_SAMPLER, _deviceResources->texSamplerDescriptor);
	writeDescSet.setImageInfo(0, pvrvk::DescriptorImageInfo(_deviceResources->texture, bilinearSampler, pvrvk::ImageLayout::e_SHADER_READ_ONLY_OPTIMAL));
	_deviceResources->device->updateDescriptorSets(&writeDescSet, 1, nullptr, 0);
}

void VulkanPVRScopeExample::createUboDescriptorSet()
{
	// create the mvp ubo
	const uint32_t swapchainLength = _deviceResources->swapchain->getSwapchainLength();
	pvrvk::WriteDescriptorSet writeDescSet[pvrvk::FrameworkCaps::MaxSwapChains * 2];

	pvr::utils::StructuredMemoryDescription desc;
	desc.addElement("MVPMatrix", pvr::GpuDatatypes::mat4x4);
	desc.addElement("MVITMatrix", pvr::GpuDatatypes::mat3x3);

	_deviceResources->mvpUboView.initDynamic(desc, NumModelInstance * _deviceResources->swapchain->getSwapchainLength(), pvr::BufferUsageFlags::UniformBuffer,
		static_cast<uint32_t>(_deviceResources->device->getPhysicalDevice()->getProperties().getLimits().getMinUniformBufferOffsetAlignment()));
	_deviceResources->mvpUbo = pvr::utils::createBuffer(_deviceResources->device, _deviceResources->mvpUboView.getSize(), pvrvk::BufferUsageFlags::e_UNIFORM_BUFFER_BIT,
		pvrvk::MemoryPropertyFlags::e_HOST_VISIBLE_BIT,
		pvrvk::MemoryPropertyFlags::e_DEVICE_LOCAL_BIT | pvrvk::MemoryPropertyFlags::e_HOST_VISIBLE_BIT | pvrvk::MemoryPropertyFlags::e_HOST_COHERENT_BIT,
		&_deviceResources->vmaBufferAllocator, pvr::utils::vma::AllocationCreateFlags::e_MAPPED_BIT);

	_deviceResources->mvpUboView.pointToMappedMemory(_deviceResources->mvpUbo->getDeviceMemory()->getMappedData());

	uint32_t writeIndex = 0;
	for (uint32_t i = 0; i < swapchainLength; ++i, ++writeIndex)
	{
		pvrvk::DescriptorSet& matDescSet = _deviceResources->mvpDescriptor[i];
		matDescSet = _deviceResources->descriptorPool->allocateDescriptorSet(_deviceResources->uboLayoutVert);
		writeDescSet[writeIndex]
			.set(pvrvk::DescriptorType::e_UNIFORM_BUFFER_DYNAMIC, matDescSet)
			.setBufferInfo(0, pvrvk::DescriptorBufferInfo(_deviceResources->mvpUbo, 0, _deviceResources->mvpUboView.getDynamicSliceSize()));
	}

	{
		// create the material ubo
		pvr::utils::StructuredMemoryDescription desc;
		desc.addElement(MaterialUboElements::Mapping[MaterialUboElements::ViewLightDirection].first, MaterialUboElements::Mapping[MaterialUboElements::ViewLightDirection].second);
		desc.addElement(MaterialUboElements::Mapping[MaterialUboElements::AlbedoModulation].first, MaterialUboElements::Mapping[MaterialUboElements::AlbedoModulation].second);
		desc.addElement(MaterialUboElements::Mapping[MaterialUboElements::SpecularExponent].first, MaterialUboElements::Mapping[MaterialUboElements::SpecularExponent].second);
		desc.addElement(MaterialUboElements::Mapping[MaterialUboElements::Metallicity].first, MaterialUboElements::Mapping[MaterialUboElements::Metallicity].second);
		desc.addElement(MaterialUboElements::Mapping[MaterialUboElements::Reflectivity].first, MaterialUboElements::Mapping[MaterialUboElements::Reflectivity].second);

		_deviceResources->materialUboView.initDynamic(desc, _deviceResources->swapchain->getSwapchainLength(), pvr::BufferUsageFlags::UniformBuffer,
			static_cast<uint32_t>(_deviceResources->device->getPhysicalDevice()->getProperties().getLimits().getMinUniformBufferOffsetAlignment()));
		_deviceResources->materialUbo = pvr::utils::createBuffer(_deviceResources->device, _deviceResources->materialUboView.getSize(),
			pvrvk::BufferUsageFlags::e_UNIFORM_BUFFER_BIT, pvrvk::MemoryPropertyFlags::e_HOST_VISIBLE_BIT,
			pvrvk::MemoryPropertyFlags::e_DEVICE_LOCAL_BIT | pvrvk::MemoryPropertyFlags::e_HOST_VISIBLE_BIT | pvrvk::MemoryPropertyFlags::e_HOST_COHERENT_BIT,
			&_deviceResources->vmaBufferAllocator, pvr::utils::vma::AllocationCreateFlags::e_MAPPED_BIT);

		_deviceResources->materialUboView.pointToMappedMemory(_deviceResources->materialUbo->getDeviceMemory()->getMappedData());
	}

	for (uint32_t i = 0; i < swapchainLength; ++i, ++writeIndex)
	{
		pvrvk::DescriptorSet& matDescSet = _deviceResources->materialDescriptor[i];
		matDescSet = _deviceResources->descriptorPool->allocateDescriptorSet(_deviceResources->uboLayoutFrag);
		writeDescSet[writeIndex]
			.set(pvrvk::DescriptorType::e_UNIFORM_BUFFER, matDescSet)
			.setBufferInfo(0, pvrvk::DescriptorBufferInfo(_deviceResources->materialUbo, 0, _deviceResources->materialUboView.getDynamicSliceSize()));

		// fill the buffer with initial values
		_deviceResources->materialUboView.getElement(MaterialUboElements::ViewLightDirection, 0, i).setValue(glm::vec4(_materialData.lightDirView, 0));
		_deviceResources->materialUboView.getElement(MaterialUboElements::AlbedoModulation, 0, i).setValue(glm::vec4(_materialData.albedoMod, 0));
		_deviceResources->materialUboView.getElement(MaterialUboElements::SpecularExponent, 0, i).setValue(_materialData.specExponent);
		_deviceResources->materialUboView.getElement(MaterialUboElements::Metallicity, 0, i).setValue(_materialData.metalicity);
		_deviceResources->materialUboView.getElement(MaterialUboElements::Reflectivity, 0, i).setValue(_materialData.reflectivity);
	}

	// if the memory property flags used by the buffers' device memory do not contain e_HOST_COHERENT_BIT then we must flush the memory
	if (static_cast<uint32_t>(_deviceResources->materialUbo->getDeviceMemory()->getMemoryFlags() & pvrvk::MemoryPropertyFlags::e_HOST_COHERENT_BIT) == 0)
	{
		_deviceResources->materialUbo->getDeviceMemory()->flushRange(0, _deviceResources->materialUboView.getSize());
	}

	_deviceResources->device->updateDescriptorSets(writeDescSet, writeIndex, nullptr, 0);
}

/*!*********************************************************************************************************************
\brief  Create a graphics pipeline required for this training course
\return Return true if no error occurred
***********************************************************************************************************************/
void VulkanPVRScopeExample::createPipeline()
{
	pvr::utils::VertexBindings_Name vertexBindings[] = { { "POSITION", "inVertex" }, { "NORMAL", "inNormal" }, { "UV0", "inTexCoord" } };

	//--- create the descriptor set Layout
	_deviceResources->texSamplerLayout = _deviceResources->device->createDescriptorSetLayout(
		pvrvk::DescriptorSetLayoutCreateInfo().setBinding(0, pvrvk::DescriptorType::e_COMBINED_IMAGE_SAMPLER, 1, pvrvk::ShaderStageFlags::e_FRAGMENT_BIT));

	_deviceResources->uboLayoutVert = _deviceResources->device->createDescriptorSetLayout(
		pvrvk::DescriptorSetLayoutCreateInfo().setBinding(0, pvrvk::DescriptorType::e_UNIFORM_BUFFER_DYNAMIC, 1, pvrvk::ShaderStageFlags::e_VERTEX_BIT));

	_deviceResources->uboLayoutFrag = _deviceResources->device->createDescriptorSetLayout(
		pvrvk::DescriptorSetLayoutCreateInfo().setBinding(0, pvrvk::DescriptorType::e_UNIFORM_BUFFER, 1, pvrvk::ShaderStageFlags::e_FRAGMENT_BIT));

	//--- create the pipeline layout
	pvrvk::PipelineLayoutCreateInfo pipeLayoutInfo;
	pipeLayoutInfo
		.setDescSetLayout(0, _deviceResources->uboLayoutVert) // mvp
		.setDescSetLayout(1, _deviceResources->texSamplerLayout) // albedo
		.setDescSetLayout(2, _deviceResources->uboLayoutFrag); // material

	pvrvk::GraphicsPipelineCreateInfo pipelineInfo;
	pipelineInfo.viewport.setViewportAndScissor(0,
		pvrvk::Viewport(0, 0, (float)_deviceResources->swapchain->getDimension().getWidth(), (float)_deviceResources->swapchain->getDimension().getHeight()),
		pvrvk::Rect2D(0, 0, _deviceResources->swapchain->getDimension().getWidth(), _deviceResources->swapchain->getDimension().getHeight()));

	pipelineInfo.vertexShader.setShader(_deviceResources->device->createShader(getAssetStream(VertShaderSrcFile)->readToEnd<uint32_t>()));

	pipelineInfo.fragmentShader.setShader(_deviceResources->device->createShader(getAssetStream(FragShaderSrcFile)->readToEnd<uint32_t>()));

	pipelineInfo.rasterizer.setCullMode(pvrvk::CullModeFlags::e_BACK_BIT);
	pipelineInfo.depthStencil.enableDepthTest(true);
	pipelineInfo.depthStencil.setDepthCompareFunc(pvrvk::CompareOp::e_LESS);
	pipelineInfo.depthStencil.enableDepthWrite(true);
	pipelineInfo.pipelineLayout = _deviceResources->device->createPipelineLayout(pipeLayoutInfo);
	pipelineInfo.colorBlend.setAttachmentState(0, pvrvk::PipelineColorBlendAttachmentState());

	pvr::utils::populateInputAssemblyFromMesh(_scene->getMesh(0), vertexBindings, 3, pipelineInfo.vertexInput, pipelineInfo.inputAssembler);

	pipelineInfo.renderPass = _deviceResources->onScreenFramebuffer[0]->getRenderPass();
	_deviceResources->pipeline = _deviceResources->device->createGraphicsPipeline(pipelineInfo, _deviceResources->pipelineCache);
}

/*!*********************************************************************************************************************
\brief Loads the mesh data required for this training course into vertex buffer objects
***********************************************************************************************************************/
void VulkanPVRScopeExample::loadVbos(pvrvk::CommandBuffer& uploadCmd)
{
	// load the vbo and ibo data
	bool requiresCommandBufferSubmission = false;
	pvr::utils::appendSingleBuffersFromModel(
		_deviceResources->device, *_scene, _deviceResources->vbos, _deviceResources->ibos, uploadCmd, requiresCommandBufferSubmission, &_deviceResources->vmaBufferAllocator);
}

void VulkanPVRScopeExample::updateMVPMatrix(uint32_t swapchain)
{
	glm::mat4 instance1, instance2;
	instance1 = glm::translate(glm::vec3(0.0f, -1.0f, 0.0f)) * glm::rotate((_angleY), glm::vec3(0.f, 1.f, 0.f)) * glm::translate(glm::vec3(.5f, 0.f, -1.0f)) *
		glm::scale(glm::vec3(0.5f, 0.5f, 0.5f)) * _scene->getWorldMatrix(0);

	// Create two instances of the mesh, offset to the sides.
	instance2 = _viewMtx * instance1 * glm::translate(glm::vec3(0, 0, -2000));
	instance1 = _viewMtx * instance1 * glm::translate(glm::vec3(0, 0, 2000));

	// update the angle for the next frame.
	_angleY += (2 * glm::pi<float>() * getFrameTime() / 1000) / 10;

	_deviceResources->mvpUboView.getElementByName("MVPMatrix", 0, 0 + swapchain * 2).setValue(_projMtx * instance1);
	_deviceResources->mvpUboView.getElementByName("MVITMatrix", 0, 0 + swapchain * 2).setValue(glm::mat3x4(glm::inverseTranspose(glm::mat3(instance1))));

	_deviceResources->mvpUboView.getElementByName("MVPMatrix", 0, 1 + swapchain * 2).setValue(_projMtx * instance2);
	_deviceResources->mvpUboView.getElementByName("MVITMatrix", 0, 1 + swapchain * 2).setValue(glm::mat3x4(glm::inverseTranspose(glm::mat3(instance2))));

	// if the memory property flags used by the buffers' device memory do not contain e_HOST_COHERENT_BIT then we must flush the memory
	if (static_cast<uint32_t>(_deviceResources->mvpUbo->getDeviceMemory()->getMemoryFlags() & pvrvk::MemoryPropertyFlags::e_HOST_COHERENT_BIT) == 0)
	{
		_deviceResources->mvpUbo->getDeviceMemory()->flushRange(
			_deviceResources->mvpUboView.getDynamicSliceOffset(swapchain * 2), _deviceResources->mvpUboView.getDynamicSliceSize() * 2);
	}
}

/*!*********************************************************************************************************************
\return Result::Success if no error occurred
\brief  Code in initApplication() will be called by Shell once per run, before the rendering context is created.
	  Used to initialize variables that are not dependent on it (e.g. external modules, loading meshes,etc.)
	  If the rendering context is lost, initApplication() will not be called again.
***********************************************************************************************************************/
pvr::Result VulkanPVRScopeExample::initApplication()
{
	_frameId = 0;
	// Blue-ish marble
	_progUniforms.specularExponent = 100.f; // Width of the specular highlights (High exponent for small shiny highlights)
	_progUniforms.albedo = glm::vec3(.78f, .82f, 1.f); // Overall color
	_progUniforms.metallicity = 1.f; // Doesn't make much of a difference in this material.
	_progUniforms.reflectivity = .2f; // Low reflectivity - color mostly diffuse.

	// At the time of writing, this counter is the USSE load for vertex + pixel processing
	_selectedCounter = 0;
	_interval = 0;
	_angleY = 0.0f;

	// Load the _scene
	pvr::assets::helper::loadModel(*this, SceneFile, _scene);

	// Process the command line
	{
		const pvr::CommandLine commandline = getCommandLine();
		commandline.getIntOption("-counter", _selectedCounter);
		commandline.getIntOption("-_interval", _interval);
	}
	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\return Return Result::Success if no error occurred
\brief  Code in quitApplication() will be called by Shell once per run, just before exiting
	  the program. If the rendering context is lost, quitApplication() will not be called.x
***********************************************************************************************************************/
pvr::Result VulkanPVRScopeExample::quitApplication()
{
	_scene.reset();
	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\return Return Result::Success if no error occurred
\brief Code in initView() will be called by Shell upon initialization or after a change in the rendering context.
	 Used to initialize variables that are dependent on the rendering context (e.g. textures, vertex buffers, etc.)
***********************************************************************************************************************/
pvr::Result VulkanPVRScopeExample::initView()
{
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

	//--------------------
	// Create the logical device and the queues
	const pvr::utils::QueuePopulateInfo queuePopulateInfo = { pvrvk::QueueFlags::e_GRAPHICS_BIT | pvrvk::QueueFlags::e_TRANSFER_BIT, surface };
	pvr::utils::QueueAccessInfo queueAccessInfo;
	_deviceResources->device = pvr::utils::createDeviceAndQueues(_deviceResources->instance->getPhysicalDevice(0), &queuePopulateInfo, 1, &queueAccessInfo);

	//--------------------
	// Get the queues
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

	//--------------------
	// Create the swapchain
	pvr::utils::createSwapchainAndDepthStencilImageAndViews(_deviceResources->device, surface, getDisplayAttributes(), _deviceResources->swapchain,
		_deviceResources->depthStencilImages, swapchainImageUsage, pvrvk::ImageUsageFlags::e_DEPTH_STENCIL_ATTACHMENT_BIT | pvrvk::ImageUsageFlags::e_TRANSIENT_ATTACHMENT_BIT,
		&_deviceResources->vmaImageAllocator);

	//--------------------
	// Create the framebuffer
	pvrvk::RenderPass rp;
	pvr::utils::createOnscreenFramebufferAndRenderpass(_deviceResources->swapchain, &_deviceResources->depthStencilImages[0], _deviceResources->onScreenFramebuffer, rp);

	//--------------------
	// Create the pools
	_deviceResources->commandPool =
		_deviceResources->device->createCommandPool(_deviceResources->queue->getQueueFamilyId(), pvrvk::CommandPoolCreateFlags::e_RESET_COMMAND_BUFFER_BIT);

	_deviceResources->descriptorPool = _deviceResources->device->createDescriptorPool(pvrvk::DescriptorPoolCreateInfo()
																						  .addDescriptorInfo(pvrvk::DescriptorType::e_COMBINED_IMAGE_SAMPLER, 16)
																						  .addDescriptorInfo(pvrvk::DescriptorType::e_UNIFORM_BUFFER_DYNAMIC, 16)
																						  .addDescriptorInfo(pvrvk::DescriptorType::e_UNIFORM_BUFFER, 16)
																						  .setMaxDescriptorSets(16));

	// set up the material
	_materialData.specExponent = 100.f; // Width of the specular highlights (High exponent for small shiny highlights)
	_materialData.albedoMod = glm::vec3(.78f, .82f, 1.f); // Overall color
	_materialData.metalicity = 1.f; // Doesn't make much of a difference in this material.
	_materialData.reflectivity = .2f; // Low reflectivity - color mostly diffuse.
	_materialData.lightDirView = glm::normalize(glm::vec3(1.f, 1.f, -1.f)); // Set light direction in model space

	// Create the pipeline cache
	_deviceResources->pipelineCache = _deviceResources->device->createPipelineCache();

	createPipeline();
	createUboDescriptorSet();

	// Prepare per swachain resources and set the acttachments inital layouts
	for (uint32_t i = 0; i < _deviceResources->swapchain->getSwapchainLength(); ++i)
	{
		_deviceResources->semaphoreAcquire[i] = _deviceResources->device->createSemaphore();
		_deviceResources->semaphoreSubmit[i] = _deviceResources->device->createSemaphore();
		_deviceResources->perFrameFence[i] = _deviceResources->device->createFence(pvrvk::FenceCreateFlags::e_SIGNALED_BIT);
		_deviceResources->commandBuffer[i] = _deviceResources->commandPool->allocateCommandBuffer();
		if (i == 0)
		{
			_deviceResources->commandBuffer[0]->begin();
		}
	}

	// Load textures
	createTexSamplerDescriptorSet(_deviceResources->commandBuffer[0]);

	// Initialize VBO data
	loadVbos(_deviceResources->commandBuffer[0]);

	_deviceResources->commandBuffer[0]->end();
	pvrvk::SubmitInfo submitInfo;
	submitInfo.commandBuffers = &_deviceResources->commandBuffer[0];
	submitInfo.numCommandBuffers = 1;
	_deviceResources->queue->submit(&submitInfo, 1);
	_deviceResources->queue->waitIdle();

	// Initialize UIRenderer
	_deviceResources->uiRenderer.init(
		getWidth(), getHeight(), isFullScreen(), _deviceResources->onScreenFramebuffer[0]->getRenderPass(), 0, _deviceResources->commandPool, _deviceResources->queue);

	// Calculate the projection and view matrices
	// Is the screen rotated?
	const bool isRotate = this->isScreenRotated();
	_projMtx = pvr::math::perspectiveFov(pvr::Api::Vulkan, glm::pi<float>() / 6, (float)getWidth(), (float)this->getHeight(), _scene->getCamera(0).getNear(),
		_scene->getCamera(0).getFar(), (isRotate ? glm::pi<float>() * .5f : 0.0f));

	_viewMtx = glm::lookAt(glm::vec3(0, 0, 75), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

	// Initialize the graphing code
	std::string errorStr;
	if (!_deviceResources->scopeGraph.init(_deviceResources->device, _deviceResources->swapchain->getDimension(), _deviceResources->descriptorPool, *this,
			_deviceResources->uiRenderer, _deviceResources->onScreenFramebuffer[0]->getRenderPass(), _deviceResources->vmaBufferAllocator, errorStr))
	{
		setExitMessage(errorStr.c_str());
		return pvr::Result::NotInitialized;
	}

	if (_deviceResources->scopeGraph.isInitialized())
	{
		// Position the graph
		_deviceResources->scopeGraph.position(getWidth(), getHeight(),
			pvrvk::Rect2D((static_cast<uint32_t>(getWidth() * 0.02f)), (static_cast<uint32_t>(getHeight() * 0.02f)), (static_cast<uint32_t>(getWidth() * 0.96f)),
				(static_cast<uint32_t>(getHeight() * 0.96f) / 3)));

		// Output the current active group and a list of all the counters
		Log(LogLevel::Information, "PVRScope Number of Hardware Counters: %i\n", _deviceResources->scopeGraph.getCounterNum());
		Log(LogLevel::Information, "Counters\n-ID---Name-------------------------------------------\n");

		for (uint32_t i = 0; i < _deviceResources->scopeGraph.getCounterNum(); ++i)
		{
			Log(LogLevel::Information, "[%2i] %s %s\n", i, _deviceResources->scopeGraph.getCounterName(i),
				_deviceResources->scopeGraph.isCounterPercentage(i) ? "percentage" : "absolute");

			_deviceResources->scopeGraph.showCounter(i, false);
		}

		_deviceResources->scopeGraph.ping(1);
		// Tell the graph to show initial counters
		_deviceResources->scopeGraph.showCounter(_deviceResources->scopeGraph.getStandard3DIndex(), true);
		_deviceResources->scopeGraph.showCounter(_deviceResources->scopeGraph.getStandardTAIndex(), true);
		_deviceResources->scopeGraph.showCounter(_deviceResources->scopeGraph.getStandardShaderPixelIndex(), true);
		_deviceResources->scopeGraph.showCounter(_deviceResources->scopeGraph.getStandardShaderVertexIndex(), true);
		for (uint32_t i = 0; i < _deviceResources->scopeGraph.getCounterNum(); ++i)
		{
			std::string s(std::string(_deviceResources->scopeGraph.getCounterName(i))); // Better safe than sorry - get a copy...
			pvr::strings::toLower(s);
			if (pvr::strings::startsWith(s, "hsr efficiency"))
			{
				_deviceResources->scopeGraph.showCounter(i, true);
			}
			if (pvr::strings::startsWith(s, "shaded pixels per second"))
			{
				_deviceResources->scopeGraph.showCounter(i, true);
			}
		}
		// Set the update _interval: number of updates [frames] before updating the graph
		_deviceResources->scopeGraph.setUpdateInterval(_interval);
	}

	updateDescription();

	_deviceResources->uiRenderer.getDefaultTitle()->setText("PVRScopeExample");
	_deviceResources->uiRenderer.getDefaultTitle()->commitUpdates();
	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\return Return Result::Success if no error occurred
\brief Code in releaseView() will be called by Shell when the application quits or before a change in the rendering context.
***********************************************************************************************************************/
pvr::Result VulkanPVRScopeExample::releaseView()
{
	_deviceResources.reset();
	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\return Return Result::Success if no error occurred
\brief Main rendering loop function of the program. The shell will call this function every frame.
***********************************************************************************************************************/
pvr::Result VulkanPVRScopeExample::renderFrame()
{
	//--------------------
	// MAKE SURE THE COMMANDBUFFER AND THE SEMAPHORE ARE FREE TO USE
	_deviceResources->perFrameFence[_frameId]->wait();
	_deviceResources->perFrameFence[_frameId]->reset();

	_deviceResources->swapchain->acquireNextImage(uint64_t(-1), _deviceResources->semaphoreAcquire[_frameId]);
	const uint32_t swapchainIndex = _deviceResources->swapchain->getSwapchainIndex();
	updateMVPMatrix(swapchainIndex);
	_deviceResources->scopeGraph.ping(static_cast<float>(getFrameTime()));
	recordCommandBuffer(swapchainIndex);

	pvrvk::SubmitInfo submitInfo;
	pvrvk::PipelineStageFlags waitStages = pvrvk::PipelineStageFlags::e_COLOR_ATTACHMENT_OUTPUT_BIT;
	submitInfo.commandBuffers = &_deviceResources->commandBuffer[swapchainIndex];
	submitInfo.numCommandBuffers = 1;
	submitInfo.numSignalSemaphores = 1;
	submitInfo.numWaitSemaphores = 1;
	submitInfo.waitSemaphores = &_deviceResources->semaphoreAcquire[_frameId];
	submitInfo.signalSemaphores = &_deviceResources->semaphoreSubmit[_frameId];
	submitInfo.waitDestStages = &waitStages;
	_deviceResources->queue->submit(&submitInfo, 1, _deviceResources->perFrameFence[_frameId]);

	if (this->shouldTakeScreenshot())
	{
		pvr::utils::takeScreenshot(_deviceResources->swapchain, swapchainIndex, _deviceResources->commandPool, _deviceResources->queue, this->getScreenshotFileName(),
			&_deviceResources->vmaBufferAllocator, &_deviceResources->vmaImageAllocator);
	}

	//--------------------
	// Presents
	pvrvk::PresentInfo presentInfo;
	presentInfo.waitSemaphores = &_deviceResources->semaphoreSubmit[_frameId];
	presentInfo.numWaitSemaphores = 1;
	presentInfo.swapchains = &_deviceResources->swapchain;
	presentInfo.numSwapchains = 1;
	presentInfo.imageIndices = &swapchainIndex;
	_deviceResources->queue->present(presentInfo);

	_frameId = (_frameId + 1) % _deviceResources->swapchain->getSwapchainLength();
	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\param nodeIndex Node index of the mesh to draw
\brief Draws a Model::Mesh after the model view matrix has been set and the material prepared.
***********************************************************************************************************************/
void VulkanPVRScopeExample::drawMesh(int32_t nodeIndex, pvrvk::CommandBuffer& command)
{
	const pvr::assets::Model::Node& node = _scene->getNode(nodeIndex);
	const pvr::assets::Mesh& mesh = _scene->getMesh(node.getObjectId());

	// bind the VBO for the mesh
	command->bindVertexBuffer(_deviceResources->vbos[node.getObjectId()], 0, 0);

	// The geometry can be exported in 4 ways:
	// - Indexed Triangle list
	// - Non-Indexed Triangle list
	// - Indexed Triangle strips
	// - Non-Indexed Triangle strips
	if (mesh.getNumStrips() == 0)
	{
		if (_deviceResources->ibos[node.getObjectId()].isValid())
		{
			// Indexed Triangle list
			command->bindIndexBuffer(_deviceResources->ibos[node.getObjectId()], 0, pvr::utils::convertToPVRVk(mesh.getFaces().getDataType()));
			command->drawIndexed(0, mesh.getNumFaces() * 3, 0, 0, 1);
		}
		else
		{
			// Non-Indexed Triangle list
			command->draw(0, mesh.getNumFaces(), 0, 1);
		}
	}
	else
	{
		for (int32_t i = 0; i < (int32_t)mesh.getNumStrips(); ++i)
		{
			int offset = 0;
			if (_deviceResources->ibos[node.getObjectId()].isValid())
			{
				// Indexed Triangle strips
				command->bindIndexBuffer(_deviceResources->ibos[node.getObjectId()], 0, pvr::utils::convertToPVRVk(mesh.getFaces().getDataType()));
				command->drawIndexed(0, mesh.getStripLength(i) + 2, 0, 0, 1);
			}
			else
			{
				// Non-Indexed Triangle strips
				command->draw(0, mesh.getStripLength(i) + 2, 0, 1);
			}
			offset += mesh.getStripLength(i) + 2;
		}
	}
}

/*!*********************************************************************************************************************
\brief  Pre-record the rendering commands
***********************************************************************************************************************/
void VulkanPVRScopeExample::recordCommandBuffer(uint32_t swapchain)
{
	const pvrvk::ClearValue clearValues[] = { pvrvk::ClearValue(0.00f, 0.70f, 0.67f, 1.0f), pvrvk::ClearValue::createDefaultDepthStencilClearValue() };
	pvrvk::CommandBuffer& command = _deviceResources->commandBuffer[swapchain];
	command->begin();
	command->beginRenderPass(_deviceResources->onScreenFramebuffer[swapchain], pvrvk::Rect2D(0, 0, getWidth(), getHeight()), true, clearValues, ARRAY_SIZE(clearValues));

	// Use shader program
	command->bindPipeline(_deviceResources->pipeline);

	// Bind the descriptors
	command->bindDescriptorSet(pvrvk::PipelineBindPoint::e_GRAPHICS, _deviceResources->pipeline->getPipelineLayout(), 1, _deviceResources->texSamplerDescriptor, 0);

	command->bindDescriptorSet(pvrvk::PipelineBindPoint::e_GRAPHICS, _deviceResources->pipeline->getPipelineLayout(), 2, _deviceResources->materialDescriptor[swapchain], 0);

	// draw the first instance
	uint32_t offset = _deviceResources->mvpUboView.getDynamicSliceOffset(0 + swapchain * 2);

	command->bindDescriptorSet(pvrvk::PipelineBindPoint::e_GRAPHICS, _deviceResources->pipeline->getPipelineLayout(), 0, _deviceResources->mvpDescriptor[swapchain], &offset, 1);

	drawMesh(0, command);

	// draw the second instance
	offset = _deviceResources->mvpUboView.getDynamicSliceOffset(1 + swapchain * 2);

	command->bindDescriptorSet(pvrvk::PipelineBindPoint::e_GRAPHICS, _deviceResources->pipeline->getPipelineLayout(), 0, _deviceResources->mvpDescriptor[swapchain], &offset, 1);

	drawMesh(0, command);

	//--------------------
	// Record the Scope graph
	_deviceResources->scopeGraph.recordCommandBuffer(command);

	//--------------------
	// Record the UIRenderer
	_deviceResources->uiRenderer.beginRendering(command);
	_deviceResources->uiRenderer.getDefaultTitle()->render();
	_deviceResources->uiRenderer.getDefaultDescription()->render();
	_deviceResources->uiRenderer.getSdkLogo()->render();
	_deviceResources->scopeGraph.recordUIElements();
	_deviceResources->uiRenderer.endRendering();
	command->endRenderPass();
	command->end();
}

/*!*********************************************************************************************************************
\brief  Update the description
***********************************************************************************************************************/
void VulkanPVRScopeExample::updateDescription()
{
	static char description[256];
	if (_deviceResources->scopeGraph.getCounterNum())
	{
		float maximum = _deviceResources->scopeGraph.getMaximumOfData(_selectedCounter);
		float userY = _deviceResources->scopeGraph.getMaximum(_selectedCounter);
		bool isKilos = false;
		if (maximum > 10000)
		{
			maximum /= 1000;
			userY /= 1000;
			isKilos = true;
		}
		bool isPercentage = _deviceResources->scopeGraph.isCounterPercentage(_selectedCounter);

		const char* standard = "Use up-down to select a counter, click to enable/disable it\n"
							   "Counter [%i]\n"
							   "Name: %s\n"
							   "Shown: %s\n"
							   "user y-axis: %.2f  max: %.2f\n";
		const char* percentage = "Use up-down to select a counter, click to enable/disable it\n"
								 "Counter [%i]\n"
								 "Name: %s\n"
								 "Shown: %s\n"
								 "user y-axis: %.2f%%  max: %.2f%%\n";
		const char* kilo = "Use up-down to select a counter, click to enable/disable it\n"
						   "Counter [%i]\n"
						   "Name: %s\n"
						   "Shown: %s\n"
						   "user y-axis: %.0fK  max: %.0fK\n";

		sprintf(description, isKilos ? kilo : isPercentage ? percentage : standard, _selectedCounter, _deviceResources->scopeGraph.getCounterName(_selectedCounter),
			_deviceResources->scopeGraph.isCounterShown(_selectedCounter) ? "Yes" : "No", userY, maximum);
		_deviceResources->uiRenderer.getDefaultDescription()->setColor(glm::vec4(1.f));
	}
	else
	{
		sprintf(description, "No counters present");
		_deviceResources->uiRenderer.getDefaultDescription()->setColor(glm::vec4(.8f, 0.0f, 0.0f, 1.0f));
	}
	// Displays the demo name using the tools. For a detailed explanation, see the training course IntroUIRenderer
	_deviceResources->uiRenderer.getDefaultDescription()->setText(description);
	_deviceResources->uiRenderer.getDefaultDescription()->commitUpdates();
}

/*!*********************************************************************************************************************
\return auto ptr to the demo supplied by the user
\brief  This function must be implemented by the user of the shell. The user should return its Shell object defining the
	behavior of the application.
***********************************************************************************************************************/
std::unique_ptr<pvr::Shell> pvr::newDemo()
{
	return std::unique_ptr<pvr::Shell>(new VulkanPVRScopeExample());
}
