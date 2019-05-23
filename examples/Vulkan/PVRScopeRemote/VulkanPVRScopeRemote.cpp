/*!*********************************************************************************************************************
\File         VulkanPVRScopeRemote.cpp
\Title        PVRScopeRemote
\Author       PowerVR by Imagination, Developer Technology Team
\Copyright    Copyright (c) Imagination Technologies Limited.
\brief		  Shows how to use our example PVRScope graph code.
***********************************************************************************************************************/
#include "PVRShell/PVRShell.h"
#include "PVRUtils/PVRUtilsVk.h"
#include "PVRScopeComms.h"

// Source and binary shaders
const char FragShaderSrcFile[] = "FragShader_vk.fsh.spv";
const char VertShaderSrcFile[] = "VertShader_vk.vsh.spv";

// PVR texture files
const char TextureFile[] = "Marble.pvr";

// POD scene files
const char SceneFile[] = "Satyr.pod";
enum
{
	MaxSwapChains = 8
};
namespace CounterDefs {
enum Enum
{
	Counter,
	Counter10,
	NumCounter
};
}

namespace PipelineConfigs {
// Pipeline Descriptor sets
enum DescriptorSetIds
{
	Model,
	Lighting
};
} // namespace PipelineConfigs
namespace BufferEntryNames {
namespace Matrices {
const char* const MVPMatrix = "mVPMatrix";
const char* const MVInverseTransposeMatrix = "mVITMatrix";
} // namespace Matrices
namespace Materials {
const char* const AlbedoModulation = "albedoModulation";
const char* const SpecularExponent = "specularExponent";
const char* const Metallicity = "metallicity";
const char* const Reflectivity = "reflectivity";
} // namespace Materials
namespace Lighting {
const char* const ViewLightDirection = "viewLightDirection";
}
} // namespace BufferEntryNames

const char* FrameDefs[CounterDefs::NumCounter] = { "Frames", "Frames10" };

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
	pvrvk::CommandPool commandPool;
	pvrvk::DescriptorPool descriptorPool;

	pvr::Multi<pvrvk::ImageView> depthStencilImages;

	pvr::Multi<pvrvk::Semaphore> semaphoreAcquire;
	pvr::Multi<pvrvk::Semaphore> semaphoreSubmit;
	pvr::Multi<pvrvk::Fence> perFrameFence;

	pvrvk::GraphicsPipeline pipeline;
	pvrvk::ImageView texture;
	std::vector<pvrvk::Buffer> vbos;
	std::vector<pvrvk::Buffer> ibos;
	std::vector<pvrvk::CommandBuffer> commandBuffer;

	pvr::utils::StructuredBufferView uboMatricesBufferView;
	pvrvk::Buffer uboMatrices;
	pvr::utils::StructuredBufferView uboMaterialBufferView;
	pvrvk::Buffer uboMaterial;
	pvr::utils::StructuredBufferView uboLightingBufferView;
	pvrvk::Buffer uboLighting;

	pvrvk::DescriptorSetLayout modelDescriptorSetLayout;
	pvrvk::DescriptorSetLayout lightingDescriptorSetLayout;

	pvrvk::PipelineLayout pipelineLayout;

	pvrvk::DescriptorSet modelDescriptorSets[MaxSwapChains];
	pvrvk::DescriptorSet lightingDescriptorSet;

	pvrvk::DescriptorSetLayout descriptorSetLayout;
	pvr::Multi<pvrvk::Framebuffer> onScreenFramebuffer;

	pvrvk::PipelineCache pipelineCache;

	// UIRenderer used to display text
	pvr::ui::UIRenderer uiRenderer;
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
\brief Class implementing the PVRShell functions.
***********************************************************************************************************************/
class VulkanPVRScopeRemote : public pvr::Shell
{
	std::unique_ptr<DeviceResources> _deviceResources;

	uint32_t _frameId;
	glm::mat4 _projectionMtx;
	glm::mat4 _viewMtx;

	// 3D Model
	pvr::assets::ModelHandle _scene;

	struct UboMaterialData
	{
		glm::vec3 albedo;
		float specularExponent;
		float metallicity;
		float reflectivity;
		bool isDirty;
	} _uboMatData;

	// The translation and Rotate parameter of Model
	float _angleY;

	// Data connection to PVRPerfServer
	bool _hasCommunicationError;
	SSPSCommsData* _spsCommsData;
	SSPSCommsLibraryTypeFloat _commsLibSpecularExponent;
	SSPSCommsLibraryTypeFloat _commsLibMetallicity;
	SSPSCommsLibraryTypeFloat _commsLibReflectivity;
	SSPSCommsLibraryTypeFloat _commsLibAlbedoR;
	SSPSCommsLibraryTypeFloat _commsLibAlbedoG;
	SSPSCommsLibraryTypeFloat _commsLibAlbedoB;
	uint32_t _frameCounter;
	uint32_t _frame10Counter;
	uint32_t _counterReadings[CounterDefs::NumCounter];

public:
	virtual pvr::Result initApplication();
	virtual pvr::Result initView();
	virtual pvr::Result releaseView();
	virtual pvr::Result quitApplication();
	virtual pvr::Result renderFrame();

	void createBuffers();
	void recordCommandBuffer(uint32_t swapchain);
	void createDescriptorSetLayouts();
	void createPipeline();
	void loadVbos(pvrvk::CommandBuffer& uploadCmd);
	void drawMesh(int i32NodeIndex, pvrvk::CommandBuffer& command);
	void createDescriptorSet();
	void updateUbo(uint32_t swapchain);
};

/*!*********************************************************************************************************************
\return	Return Result::Success if no error occurred
\brief	Code in initApplication() will be called by Shell once per run, before the rendering context is created.
		Used to initialize variables that are not dependent on it (e.g. external modules, loading meshes, etc.)
		If the rendering context is lost, initApplication() will not be called again.
***********************************************************************************************************************/
pvr::Result VulkanPVRScopeRemote::initApplication()
{
	_frameId = 0;

	// Load the scene
	pvr::assets::helper::loadModel(*this, SceneFile, _scene);

	// We want a data connection to PVRPerfServer
	{
		_spsCommsData = pplInitialise("PVRScopeRemote", 14);
		_hasCommunicationError = false;
		if (_spsCommsData)
		{
			// Demonstrate that there is a good chance of the initial data being
			// lost - the connection is normally completed asynchronously.
			pplSendMark(_spsCommsData, "lost", static_cast<uint32_t>(strlen("lost")));

			// This is entirely optional. Wait for the connection to succeed, it will
			// timeout if e.g. PVRPerfServer is not running.
			int isConnected;
			pplWaitForConnection(_spsCommsData, &isConnected, 1, 200);
		}
	}
	CPPLProcessingScoped PPLProcessingScoped(_spsCommsData, __FUNCTION__, static_cast<uint32_t>(strlen(__FUNCTION__)), _frameCounter);

	_uboMatData.specularExponent = 5.f; // Width of the specular highlights (using low exponent for a brushed metal look)
	_uboMatData.albedo = glm::vec3(1.f, .77f, .33f); // Overall color
	_uboMatData.metallicity = 1.f; // Is the color of the specular white (nonmetallic), or coloured by the object(metallic)
	_uboMatData.reflectivity = .8f; // Percentage of contribution of diffuse / specular
	_uboMatData.isDirty = true;
	_frameCounter = 0;
	_frame10Counter = 0;

	// set angle of rotation
	_angleY = 0.0f;

	//	Remotely editable library items
	if (_spsCommsData)
	{
		std::vector<SSPSCommsLibraryItem> communicableItems;

		// Editable: Specular Exponent
		communicableItems.push_back(SSPSCommsLibraryItem());
		_commsLibSpecularExponent.fCurrent = _uboMatData.specularExponent;
		_commsLibSpecularExponent.fMin = 1.1f;
		_commsLibSpecularExponent.fMax = 300.0f;
		communicableItems.back().pszName = "Specular Exponent";
		communicableItems.back().nNameLength = static_cast<uint32_t>(strlen(communicableItems.back().pszName));
		communicableItems.back().eType = eSPSCommsLibTypeFloat;
		communicableItems.back().pData = (const char*)&_commsLibSpecularExponent;
		communicableItems.back().nDataLength = sizeof(_commsLibSpecularExponent);

		communicableItems.push_back(SSPSCommsLibraryItem());
		// Editable: Metallicity
		_commsLibMetallicity.fCurrent = _uboMatData.metallicity;
		_commsLibMetallicity.fMin = 0.0f;
		_commsLibMetallicity.fMax = 1.0f;
		communicableItems.back().pszName = "Metallicity";
		communicableItems.back().nNameLength = static_cast<uint32_t>(strlen(communicableItems.back().pszName));
		communicableItems.back().eType = eSPSCommsLibTypeFloat;
		communicableItems.back().pData = (const char*)&_commsLibMetallicity;
		communicableItems.back().nDataLength = sizeof(_commsLibMetallicity);

		// Editable: Reflectivity
		communicableItems.push_back(SSPSCommsLibraryItem());
		_commsLibReflectivity.fCurrent = _uboMatData.reflectivity;
		_commsLibReflectivity.fMin = 0.;
		_commsLibReflectivity.fMax = 1.;
		communicableItems.back().pszName = "Reflectivity";
		communicableItems.back().nNameLength = static_cast<uint32_t>(strlen(communicableItems.back().pszName));
		communicableItems.back().eType = eSPSCommsLibTypeFloat;
		communicableItems.back().pData = (const char*)&_commsLibReflectivity;
		communicableItems.back().nDataLength = sizeof(_commsLibReflectivity);

		// Editable: Albedo R channel
		communicableItems.push_back(SSPSCommsLibraryItem());
		_commsLibAlbedoR.fCurrent = _uboMatData.albedo.r;
		_commsLibAlbedoR.fMin = 0.0f;
		_commsLibAlbedoR.fMax = 1.0f;
		communicableItems.back().pszName = "Albedo R";
		communicableItems.back().nNameLength = static_cast<uint32_t>(strlen(communicableItems.back().pszName));
		communicableItems.back().eType = eSPSCommsLibTypeFloat;
		communicableItems.back().pData = (const char*)&_commsLibAlbedoR;
		communicableItems.back().nDataLength = sizeof(_commsLibAlbedoR);

		// Editable: Albedo R channel
		communicableItems.push_back(SSPSCommsLibraryItem());
		_commsLibAlbedoG.fCurrent = _uboMatData.albedo.g;
		_commsLibAlbedoG.fMin = 0.0f;
		_commsLibAlbedoG.fMax = 1.0f;
		communicableItems.back().pszName = "Albedo G";
		communicableItems.back().nNameLength = static_cast<uint32_t>(strlen(communicableItems.back().pszName));
		communicableItems.back().eType = eSPSCommsLibTypeFloat;
		communicableItems.back().pData = (const char*)&_commsLibAlbedoG;
		communicableItems.back().nDataLength = sizeof(_commsLibAlbedoG);

		// Editable: Albedo R channel
		communicableItems.push_back(SSPSCommsLibraryItem());
		_commsLibAlbedoB.fCurrent = _uboMatData.albedo.b;
		_commsLibAlbedoB.fMin = 0.0f;
		_commsLibAlbedoB.fMax = 1.0f;
		communicableItems.back().pszName = "Albedo B";
		communicableItems.back().nNameLength = static_cast<uint32_t>(strlen(communicableItems.back().pszName));
		communicableItems.back().eType = eSPSCommsLibTypeFloat;
		communicableItems.back().pData = (const char*)&_commsLibAlbedoB;
		communicableItems.back().nDataLength = sizeof(_commsLibAlbedoB);

		// Ok, submit our library
		if (!pplLibraryCreate(_spsCommsData, communicableItems.data(), static_cast<uint32_t>(communicableItems.size())))
		{
			Log(LogLevel::Debug, "PVRScopeRemote: pplLibraryCreate() failed\n");
		}

		// User defined counters
		SSPSCommsCounterDef counterDefines[CounterDefs::NumCounter];
		for (uint32_t i = 0; i < CounterDefs::NumCounter; ++i)
		{
			counterDefines[i].pszName = FrameDefs[i];
			counterDefines[i].nNameLength = static_cast<uint32_t>(strlen(FrameDefs[i]));
		}

		if (!pplCountersCreate(_spsCommsData, counterDefines, CounterDefs::NumCounter))
		{
			Log(LogLevel::Debug, "PVRScopeRemote: pplCountersCreate() failed\n");
		}
	}
	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\return	Return Result::Success if no error occurred
\brief	Code in initView() will be called by Shell upon initialization or after a change in the rendering context.
		Used to initialize variables that are dependent on the rendering context (e.g. textures, vertex buffers, etc.)
***********************************************************************************************************************/
pvr::Result VulkanPVRScopeRemote::initView()
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

	pvr::utils::QueuePopulateInfo queuePopulateInfo = { pvrvk::QueueFlags::e_GRAPHICS_BIT, _deviceResources->surface };
	pvr::utils::QueueAccessInfo queueAccessInfo;

	_deviceResources->device = pvr::utils::createDeviceAndQueues(_deviceResources->instance->getPhysicalDevice(0), &queuePopulateInfo, 1, &queueAccessInfo);

	_deviceResources->queue = _deviceResources->device->getQueue(queueAccessInfo.familyId, queueAccessInfo.queueId);

	_deviceResources->vmaBufferAllocator = pvr::utils::vma::createAllocator(pvr::utils::vma::AllocatorCreateInfo(_deviceResources->device));
	_deviceResources->vmaImageAllocator = pvr::utils::vma::createAllocator(pvr::utils::vma::AllocatorCreateInfo(_deviceResources->device));

	pvrvk::SurfaceCapabilitiesKHR surfaceCapabilities = _deviceResources->instance->getPhysicalDevice(0)->getSurfaceCapabilities(_deviceResources->surface);

	// validate the supported swapchain image usage
	pvrvk::ImageUsageFlags swapchainImageUsage = pvrvk::ImageUsageFlags::e_COLOR_ATTACHMENT_BIT;
	if (pvr::utils::isImageUsageSupportedBySurface(surfaceCapabilities, pvrvk::ImageUsageFlags::e_TRANSFER_SRC_BIT))
	{
		swapchainImageUsage |= pvrvk::ImageUsageFlags::e_TRANSFER_SRC_BIT;
	}

	// create the swapchain
	pvr::utils::createSwapchainAndDepthStencilImageAndViews(_deviceResources->device, _deviceResources->surface, getDisplayAttributes(), _deviceResources->swapchain,
		_deviceResources->depthStencilImages, swapchainImageUsage, pvrvk::ImageUsageFlags::e_DEPTH_STENCIL_ATTACHMENT_BIT | pvrvk::ImageUsageFlags::e_TRANSIENT_ATTACHMENT_BIT,
		&_deviceResources->vmaImageAllocator);

	// Create the Commandpool and Descriptorpool
	_deviceResources->commandPool =
		_deviceResources->device->createCommandPool(_deviceResources->queue->getQueueFamilyId(), pvrvk::CommandPoolCreateFlags::e_RESET_COMMAND_BUFFER_BIT);

	_deviceResources->descriptorPool = _deviceResources->device->createDescriptorPool(pvrvk::DescriptorPoolCreateInfo()
																						  .addDescriptorInfo(pvrvk::DescriptorType::e_COMBINED_IMAGE_SAMPLER, 16)
																						  .addDescriptorInfo(pvrvk::DescriptorType::e_UNIFORM_BUFFER_DYNAMIC, 16)
																						  .addDescriptorInfo(pvrvk::DescriptorType::e_UNIFORM_BUFFER, 16));
	const uint32_t swapchainLength = _deviceResources->swapchain->getSwapchainLength();
	_deviceResources->commandBuffer.resize(swapchainLength);
	_deviceResources->semaphoreAcquire.resize(swapchainLength);
	_deviceResources->semaphoreSubmit.resize(swapchainLength);
	_deviceResources->perFrameFence.resize(swapchainLength);
	for (uint32_t i = 0; i < swapchainLength; ++i)
	{
		_deviceResources->commandBuffer[i] = _deviceResources->commandPool->allocateCommandBuffer();
		_deviceResources->semaphoreAcquire[i] = _deviceResources->device->createSemaphore();
		_deviceResources->semaphoreSubmit[i] = _deviceResources->device->createSemaphore();
		_deviceResources->perFrameFence[i] = _deviceResources->device->createFence(pvrvk::FenceCreateFlags::e_SIGNALED_BIT);
	}

	pvr::utils::createOnscreenFramebufferAndRenderpass(_deviceResources->swapchain, &_deviceResources->depthStencilImages[0], _deviceResources->onScreenFramebuffer);

	CPPLProcessingScoped PPLProcessingScoped(_spsCommsData, __FUNCTION__, static_cast<uint32_t>(strlen(__FUNCTION__)), _frameCounter);

	_deviceResources->commandBuffer[0]->begin();

	//	Initialize VBO data
	loadVbos(_deviceResources->commandBuffer[0]);

	_deviceResources->texture = pvr::utils::loadAndUploadImageAndView(_deviceResources->device, TextureFile, true, _deviceResources->commandBuffer[0], *this,
		pvrvk::ImageUsageFlags::e_SAMPLED_BIT, pvrvk::ImageLayout::e_SHADER_READ_ONLY_OPTIMAL, nullptr, &_deviceResources->vmaBufferAllocator, &_deviceResources->vmaImageAllocator);
	_deviceResources->commandBuffer[0]->end();
	// submit the texture upload commands
	pvrvk::SubmitInfo submitInfo;
	submitInfo.commandBuffers = &_deviceResources->commandBuffer[0];
	submitInfo.numCommandBuffers = 1;
	_deviceResources->queue->submit(&submitInfo, 1);
	_deviceResources->queue->waitIdle();

	createDescriptorSetLayouts();

	// Create the pipeline cache
	_deviceResources->pipelineCache = _deviceResources->device->createPipelineCache();

	createPipeline();

	createBuffers();

	// create the pipeline
	createDescriptorSet();

	_deviceResources->uboLightingBufferView.getElementByName("viewLightDirection").setValue(glm::normalize(glm::vec3(1., 1., -1.)));

	// if the memory property flags used by the buffers' device memory do not contain e_HOST_COHERENT_BIT then we must flush the memory
	if (static_cast<uint32_t>(_deviceResources->uboLighting->getDeviceMemory()->getMemoryFlags() & pvrvk::MemoryPropertyFlags::e_HOST_COHERENT_BIT) == 0)
	{
		_deviceResources->uboLighting->getDeviceMemory()->flushRange(0, _deviceResources->uboLightingBufferView.getSize());
	}

	//	Initialize the UI Renderer
	_deviceResources->uiRenderer.init(
		getWidth(), getHeight(), isFullScreen(), _deviceResources->onScreenFramebuffer[0]->getRenderPass(), 0, _deviceResources->commandPool, _deviceResources->queue);

	// create the pvrscope connection pass and fail text
	_deviceResources->uiRenderer.getDefaultTitle()->setText("PVRScopeRemote");
	_deviceResources->uiRenderer.getDefaultTitle()->commitUpdates();

	_deviceResources->uiRenderer.getDefaultDescription()->setScale(glm::vec2(.5, .5));
	_deviceResources->uiRenderer.getDefaultDescription()->setText("Use PVRTune to remotely control the parameters of this application.");
	_deviceResources->uiRenderer.getDefaultDescription()->commitUpdates();

	// Calculate the projection and view matrices
	// Is the screen rotated?
	bool isRotated = this->isScreenRotated();
	_viewMtx = glm::lookAt(glm::vec3(0.f, 0.f, 75.f), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
	_projectionMtx = pvr::math::perspectiveFov(pvr::Api::Vulkan, glm::pi<float>() / 6, (float)getWidth(), (float)getHeight(), _scene->getCamera(0).getNear(),
		_scene->getCamera(0).getFar(), isRotated ? glm::pi<float>() * .5f : 0.0f);

	for (uint32_t i = 0; i < _deviceResources->swapchain->getSwapchainLength(); ++i)
	{
		recordCommandBuffer(i);
	}
	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\return	Return Result::Success if no error occurred
\brief	Code in releaseView() will be called by Shell when the application quits or before a change in the rendering context.
***********************************************************************************************************************/
pvr::Result VulkanPVRScopeRemote::releaseView()
{
	CPPLProcessingScoped PPLProcessingScoped(_spsCommsData, __FUNCTION__, static_cast<uint32_t>(strlen(__FUNCTION__)), _frameCounter);
	// Release UIRenderer
	_deviceResources.reset();
	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\return	Return Result::Success if no error occurred
\brief	Code in quitApplication() will be called by Shell once per run, just before exiting the program.
If the rendering context is lost, QuitApplication() will not be called.
***********************************************************************************************************************/
pvr::Result VulkanPVRScopeRemote::quitApplication()
{
	if (_spsCommsData)
	{
		_hasCommunicationError |= !pplSendProcessingBegin(_spsCommsData, __FUNCTION__, static_cast<uint32_t>(strlen(__FUNCTION__)), _frameCounter);

		// Close the data connection to PVRPerfServer
		for (uint32_t i = 0; i < 40; ++i)
		{
			char buf[128];
			const int nLen = sprintf(buf, "test %u", i);
			_hasCommunicationError |= !pplSendMark(_spsCommsData, buf, nLen);
		}
		_hasCommunicationError |= !pplSendProcessingEnd(_spsCommsData);
		pplShutdown(_spsCommsData);
	}
	_scene.reset();
	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\return	Return Result::Success if no error occurred
\brief Main rendering loop function of the program. The shell will call this function every frame.
***********************************************************************************************************************/
pvr::Result VulkanPVRScopeRemote::renderFrame()
{
	_deviceResources->perFrameFence[_frameId]->wait();
	_deviceResources->perFrameFence[_frameId]->reset();

	pvrvk::Semaphore& semaphoreAcquire = _deviceResources->semaphoreAcquire[_frameId];
	pvrvk::Semaphore& semaphoreSubmit = _deviceResources->semaphoreSubmit[_frameId];

	_deviceResources->swapchain->acquireNextImage(uint64_t(-1), semaphoreAcquire);
	const uint32_t swapchainIndex = _deviceResources->swapchain->getSwapchainIndex();

	CPPLProcessingScoped PPLProcessingScoped(_spsCommsData, __FUNCTION__, static_cast<uint32_t>(strlen(__FUNCTION__)), _frameCounter);
	if (_spsCommsData)
	{
		// mark every N frames
		if (!(_frameCounter % 100))
		{
			char buf[128];
			const int nLen = sprintf(buf, "frame %u", _frameCounter);
			_hasCommunicationError |= !pplSendMark(_spsCommsData, buf, nLen);
		}

		// Check for dirty items
		_hasCommunicationError |= !pplSendProcessingBegin(_spsCommsData, "dirty", static_cast<uint32_t>(strlen("dirty")), _frameCounter);
		{
			uint32_t nItem, nNewDataLen;
			const char* pData;
			bool recompile = false;
			while (pplLibraryDirtyGetFirst(_spsCommsData, &nItem, &nNewDataLen, &pData))
			{
				Log(LogLevel::Debug, "dirty item %u %u 0x%08x\n", nItem, nNewDataLen, pData);
				switch (nItem)
				{
				case 0:
					if (nNewDataLen == sizeof(SSPSCommsLibraryTypeFloat))
					{
						const SSPSCommsLibraryTypeFloat* const psData = (SSPSCommsLibraryTypeFloat*)pData;
						_uboMatData.specularExponent = psData->fCurrent;
						_uboMatData.isDirty = true;
						Log(LogLevel::Information, "Setting Specular Exponent to value [%6.2f]", _uboMatData.specularExponent);
					}
					break;
				case 1:
					if (nNewDataLen == sizeof(SSPSCommsLibraryTypeFloat))
					{
						const SSPSCommsLibraryTypeFloat* const psData = (SSPSCommsLibraryTypeFloat*)pData;
						_uboMatData.metallicity = psData->fCurrent;
						_uboMatData.isDirty = true;
						Log(LogLevel::Information, "Setting Metallicity to value [%3.2f]", _uboMatData.metallicity);
					}
					break;
				case 2:
					if (nNewDataLen == sizeof(SSPSCommsLibraryTypeFloat))
					{
						const SSPSCommsLibraryTypeFloat* const psData = (SSPSCommsLibraryTypeFloat*)pData;
						_uboMatData.reflectivity = psData->fCurrent;
						_uboMatData.isDirty = true;
						Log(LogLevel::Information, "Setting Reflectivity to value [%3.2f]", _uboMatData.reflectivity);
					}
					break;
				case 3:
					if (nNewDataLen == sizeof(SSPSCommsLibraryTypeFloat))
					{
						const SSPSCommsLibraryTypeFloat* const psData = (SSPSCommsLibraryTypeFloat*)pData;
						_uboMatData.albedo.r = psData->fCurrent;
						_uboMatData.isDirty = true;
						Log(LogLevel::Information, "Setting Albedo Red channel to value [%3.2f]", _uboMatData.albedo.r);
					}
					break;
				case 4:
					if (nNewDataLen == sizeof(SSPSCommsLibraryTypeFloat))
					{
						const SSPSCommsLibraryTypeFloat* const psData = (SSPSCommsLibraryTypeFloat*)pData;
						_uboMatData.albedo.g = psData->fCurrent;
						_uboMatData.isDirty = true;
						Log(LogLevel::Information, "Setting Albedo Green channel to value [%3.2f]", _uboMatData.albedo.g);
					}
					break;
				case 5:
					if (nNewDataLen == sizeof(SSPSCommsLibraryTypeFloat))
					{
						const SSPSCommsLibraryTypeFloat* const psData = (SSPSCommsLibraryTypeFloat*)pData;
						_uboMatData.albedo.b = psData->fCurrent;
						_uboMatData.isDirty = true;
						Log(LogLevel::Information, "Setting Albedo Blue channel to value [%3.2f]", _uboMatData.albedo.b);
					}
					break;
				}
			}

			if (recompile)
			{
				Log(LogLevel::Error, "*** Could not recompile the shaders passed from PVRScopeComms ****");
			}
		}
		_hasCommunicationError |= !pplSendProcessingEnd(_spsCommsData);
	}

	if (_spsCommsData)
	{
		_hasCommunicationError |= !pplSendProcessingBegin(_spsCommsData, "draw", static_cast<uint32_t>(strlen("draw")), _frameCounter);
	}

	updateUbo(swapchainIndex);

	// Set eye position in model space
	// Now that the uniforms are set, call another function to actually draw the mesh.
	if (_spsCommsData)
	{
		_hasCommunicationError |= !pplSendProcessingEnd(_spsCommsData);
		_hasCommunicationError |= !pplSendProcessingBegin(_spsCommsData, "UIRenderer", static_cast<uint32_t>(strlen("UIRenderer")), _frameCounter);
	}

	if (_hasCommunicationError)
	{
		_deviceResources->uiRenderer.getDefaultControls()->setText("Communication Error:\nPVRScopeComms failed\n"
																   "Is PVRPerfServer connected?");
		_deviceResources->uiRenderer.getDefaultControls()->setColor(glm::vec4(.8f, .3f, .3f, 1.0f));
		_deviceResources->uiRenderer.getDefaultControls()->commitUpdates();
		_hasCommunicationError = false;
	}
	else
	{
		_deviceResources->uiRenderer.getDefaultControls()->setText("PVRScope Communication established.");
		_deviceResources->uiRenderer.getDefaultControls()->setColor(glm::vec4(1.f));
		_deviceResources->uiRenderer.getDefaultControls()->commitUpdates();
	}

	if (_spsCommsData)
	{
		_hasCommunicationError |= !pplSendProcessingEnd(_spsCommsData);
	}

	// send counters
	_counterReadings[CounterDefs::Counter] = _frameCounter;
	_counterReadings[CounterDefs::Counter10] = _frame10Counter;
	if (_spsCommsData)
	{
		_hasCommunicationError |= !pplCountersUpdate(_spsCommsData, _counterReadings);
	}

	// update some counters
	++_frameCounter;
	if (0 == (_frameCounter / 10) % 10)
	{
		_frame10Counter += 10;
	}

	// SUBMIT
	pvrvk::SubmitInfo submitInfo;
	submitInfo.commandBuffers = &_deviceResources->commandBuffer[swapchainIndex];
	submitInfo.numCommandBuffers = 1;
	submitInfo.waitSemaphores = &semaphoreAcquire;
	submitInfo.numWaitSemaphores = 1;
	submitInfo.signalSemaphores = &semaphoreSubmit;
	submitInfo.numSignalSemaphores = 1;
	pvrvk::PipelineStageFlags waitStages = pvrvk::PipelineStageFlags::e_COLOR_ATTACHMENT_OUTPUT_BIT;
	submitInfo.waitDestStages = &waitStages;

	_deviceResources->queue->submit(&submitInfo, 1, _deviceResources->perFrameFence[_frameId]);

	if (this->shouldTakeScreenshot())
	{
		pvr::utils::takeScreenshot(_deviceResources->swapchain, swapchainIndex, _deviceResources->commandPool, _deviceResources->queue, this->getScreenshotFileName(),
			&_deviceResources->vmaBufferAllocator, &_deviceResources->vmaImageAllocator);
	}

	// PRESENT
	pvrvk::PresentInfo presentInfo;
	presentInfo.swapchains = &_deviceResources->swapchain;
	presentInfo.imageIndices = &swapchainIndex;
	presentInfo.numSwapchains = 1;
	presentInfo.numWaitSemaphores = 1;
	presentInfo.waitSemaphores = &semaphoreSubmit;
	_deviceResources->queue->present(presentInfo);
	return pvr::Result::Success;
}

void VulkanPVRScopeRemote::createDescriptorSetLayouts()
{
	{
		pvrvk::DescriptorSetLayoutCreateInfo descSetInfo;
		descSetInfo.setBinding(0, pvrvk::DescriptorType::e_UNIFORM_BUFFER, 1u, pvrvk::ShaderStageFlags::e_VERTEX_BIT);
		descSetInfo.setBinding(1, pvrvk::DescriptorType::e_COMBINED_IMAGE_SAMPLER, 1u, pvrvk::ShaderStageFlags::e_FRAGMENT_BIT);
		descSetInfo.setBinding(2, pvrvk::DescriptorType::e_UNIFORM_BUFFER, 1u, pvrvk::ShaderStageFlags::e_FRAGMENT_BIT);
		_deviceResources->modelDescriptorSetLayout = _deviceResources->device->createDescriptorSetLayout(descSetInfo);
	}

	{
		pvrvk::DescriptorSetLayoutCreateInfo descSetInfo;
		descSetInfo.setBinding(0, pvrvk::DescriptorType::e_UNIFORM_BUFFER, 1u, pvrvk::ShaderStageFlags::e_FRAGMENT_BIT);
		_deviceResources->lightingDescriptorSetLayout = _deviceResources->device->createDescriptorSetLayout(descSetInfo);
	}
}

/*!*********************************************************************************************************************
\return	Return true if no error occurred
\brief	Loads and compiles the shaders and links the shader programs required for this training course
***********************************************************************************************************************/
void VulkanPVRScopeRemote::createPipeline()
{
	// Mapping of mesh semantic names to shader variables
	pvr::utils::VertexBindings_Name vertexBindings[] = { { "POSITION", "inVertex" }, { "NORMAL", "inNormal" }, { "UV0", "inTexCoord" } };

	CPPLProcessingScoped PPLProcessingScoped(_spsCommsData, __FUNCTION__, static_cast<uint32_t>(strlen(__FUNCTION__)), _frameCounter);

	pvrvk::PipelineLayoutCreateInfo pipeLayoutInfo;
	pipeLayoutInfo.setDescSetLayout(0, _deviceResources->modelDescriptorSetLayout);
	pipeLayoutInfo.setDescSetLayout(1, _deviceResources->lightingDescriptorSetLayout);
	_deviceResources->pipelineLayout = _deviceResources->device->createPipelineLayout(pipeLayoutInfo);

	pvrvk::GraphicsPipelineCreateInfo pipeDesc;
	pipeDesc.pipelineLayout = _deviceResources->pipelineLayout;

	/* Load and compile the shaders from files. */
	pipeDesc.vertexShader.setShader(_deviceResources->device->createShader(getAssetStream(VertShaderSrcFile)->readToEnd<uint32_t>()));
	pipeDesc.fragmentShader.setShader(_deviceResources->device->createShader(getAssetStream(FragShaderSrcFile)->readToEnd<uint32_t>()));

	pvr::utils::populateViewportStateCreateInfo(_deviceResources->onScreenFramebuffer[0], pipeDesc.viewport);
	pipeDesc.rasterizer.setCullMode(pvrvk::CullModeFlags::e_BACK_BIT);
	pipeDesc.depthStencil.enableDepthTest(true);
	pipeDesc.depthStencil.setDepthCompareFunc(pvrvk::CompareOp::e_LESS);
	pipeDesc.depthStencil.enableDepthWrite(true);
	pipeDesc.colorBlend.setAttachmentState(0, pvrvk::PipelineColorBlendAttachmentState());
	pipeDesc.renderPass = _deviceResources->onScreenFramebuffer[0]->getRenderPass();
	pvr::utils::populateInputAssemblyFromMesh(_scene->getMesh(0), vertexBindings, 3, pipeDesc.vertexInput, pipeDesc.inputAssembler);

	_deviceResources->pipeline = _deviceResources->device->createGraphicsPipeline(pipeDesc, _deviceResources->pipelineCache);
}

/*!*********************************************************************************************************************
\brief	Loads the mesh data required for this training course into vertex buffer objects
***********************************************************************************************************************/
void VulkanPVRScopeRemote::loadVbos(pvrvk::CommandBuffer& uploadCmd)
{
	CPPLProcessingScoped PPLProcessingScoped(_spsCommsData, __FUNCTION__, static_cast<uint32_t>(strlen(__FUNCTION__)), _frameCounter);

	//	Load vertex data of all meshes in the scene into VBOs
	//	The meshes have been exported with the "Interleave Vectors" option,
	//	so all data is interleaved in the buffer at pMesh->pInterleaved.
	//	Interleaving data improves the memory access pattern and cache efficiency,
	//	thus it can be read faster by the hardware.
	bool requiresCommandBufferSubmission = false;
	pvr::utils::appendSingleBuffersFromModel(
		_deviceResources->device, *_scene, _deviceResources->vbos, _deviceResources->ibos, uploadCmd, requiresCommandBufferSubmission, &_deviceResources->vmaBufferAllocator);
}

/*!*********************************************************************************************************************
\brief	Draws a assets::Mesh after the model view matrix has been set and the material prepared.
\param	nodeIndex Node index of the mesh to draw
***********************************************************************************************************************/
void VulkanPVRScopeRemote::drawMesh(int nodeIndex, pvrvk::CommandBuffer& command)
{
	CPPLProcessingScoped PPLProcessingScoped(_spsCommsData, __FUNCTION__, static_cast<uint32_t>(strlen(__FUNCTION__)), _frameCounter);

	const int32_t meshIndex = _scene->getNode(nodeIndex).getObjectId();
	const pvr::assets::Mesh& mesh = _scene->getMesh(meshIndex);
	// bind the VBO for the mesh
	command->bindVertexBuffer(_deviceResources->vbos[meshIndex], 0, 0);

	//	The geometry can be exported in 4 ways:
	//	- Indexed Triangle list
	//	- Non-Indexed Triangle list
	//	- Indexed Triangle strips
	//	- Non-Indexed Triangle strips
	if (mesh.getNumStrips() == 0)
	{
		if (_deviceResources->ibos[meshIndex].isValid())
		{
			// Indexed Triangle list
			command->bindIndexBuffer(_deviceResources->ibos[meshIndex], 0, pvrvk::IndexType::e_UINT16);
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
			if (_deviceResources->ibos[meshIndex].isValid())
			{
				command->bindIndexBuffer(_deviceResources->ibos[meshIndex], 0, pvrvk::IndexType::e_UINT16);

				// Indexed Triangle strips
				command->drawIndexed(0, mesh.getStripLength(i) + 2, offset * 2, 0, 1);
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

void VulkanPVRScopeRemote::createDescriptorSet()
{
	const uint32_t swapchainLength = _deviceResources->swapchain->getSwapchainLength();
	CPPLProcessingScoped PPLProcessingScoped(_spsCommsData, __FUNCTION__, static_cast<uint32_t>(strlen(__FUNCTION__)), _frameCounter);

	// create the MVP ubo
	{
		pvr::utils::StructuredMemoryDescription desc;
		desc.addElement(BufferEntryNames::Matrices::MVPMatrix, pvr::GpuDatatypes::mat4x4);
		desc.addElement(BufferEntryNames::Matrices::MVInverseTransposeMatrix, pvr::GpuDatatypes::mat3x3);

		_deviceResources->uboMatricesBufferView.initDynamic(desc, swapchainLength, pvr::BufferUsageFlags::UniformBuffer,
			static_cast<uint32_t>(_deviceResources->device->getPhysicalDevice()->getProperties().getLimits().getMinUniformBufferOffsetAlignment()));

		_deviceResources->uboMatrices = pvr::utils::createBuffer(_deviceResources->device, _deviceResources->uboMatricesBufferView.getSize(),
			pvrvk::BufferUsageFlags::e_UNIFORM_BUFFER_BIT, pvrvk::MemoryPropertyFlags::e_HOST_VISIBLE_BIT,
			pvrvk::MemoryPropertyFlags::e_DEVICE_LOCAL_BIT | pvrvk::MemoryPropertyFlags::e_HOST_VISIBLE_BIT | pvrvk::MemoryPropertyFlags::e_HOST_COHERENT_BIT,
			&_deviceResources->vmaBufferAllocator, pvr::utils::vma::AllocationCreateFlags::e_MAPPED_BIT);

		_deviceResources->uboMatricesBufferView.pointToMappedMemory(_deviceResources->uboMatrices->getDeviceMemory()->getMappedData());
	}

	pvrvk::SamplerCreateInfo samplerInfo;
	samplerInfo.minFilter = pvrvk::Filter::e_LINEAR;
	samplerInfo.mipMapMode = pvrvk::SamplerMipmapMode::e_LINEAR;
	samplerInfo.magFilter = pvrvk::Filter::e_LINEAR;
	pvrvk::Sampler trilinearSampler = _deviceResources->device->createSampler(samplerInfo);

	std::vector<pvrvk::WriteDescriptorSet> descSetWrites;

	for (uint32_t i = 0; i < swapchainLength; ++i)
	{
		_deviceResources->modelDescriptorSets[i] = _deviceResources->descriptorPool->allocateDescriptorSet(_deviceResources->modelDescriptorSetLayout);

		descSetWrites.push_back(pvrvk::WriteDescriptorSet(pvrvk::DescriptorType::e_UNIFORM_BUFFER, _deviceResources->modelDescriptorSets[i], 0)
									.setBufferInfo(0, pvrvk::DescriptorBufferInfo(_deviceResources->uboMatrices, 0, _deviceResources->uboMatricesBufferView.getDynamicSliceSize())));

		descSetWrites.push_back(pvrvk::WriteDescriptorSet(pvrvk::DescriptorType::e_COMBINED_IMAGE_SAMPLER, _deviceResources->modelDescriptorSets[i], 1)
									.setImageInfo(0, pvrvk::DescriptorImageInfo(_deviceResources->texture, trilinearSampler, pvrvk::ImageLayout::e_SHADER_READ_ONLY_OPTIMAL)));

		descSetWrites.push_back(pvrvk::WriteDescriptorSet(pvrvk::DescriptorType::e_UNIFORM_BUFFER, _deviceResources->modelDescriptorSets[i], 2)
									.setBufferInfo(0, pvrvk::DescriptorBufferInfo(_deviceResources->uboMaterial, 0, _deviceResources->uboMaterialBufferView.getDynamicSliceSize())));
	}

	_deviceResources->lightingDescriptorSet = _deviceResources->descriptorPool->allocateDescriptorSet(_deviceResources->lightingDescriptorSetLayout);
	descSetWrites.push_back(pvrvk::WriteDescriptorSet(pvrvk::DescriptorType::e_UNIFORM_BUFFER, _deviceResources->lightingDescriptorSet, 0)
								.setBufferInfo(0, pvrvk::DescriptorBufferInfo(_deviceResources->uboLighting, 0, _deviceResources->uboLightingBufferView.getSize())));

	_deviceResources->device->updateDescriptorSets(descSetWrites.data(), static_cast<uint32_t>(descSetWrites.size()), nullptr, 0);
}

void VulkanPVRScopeRemote::updateUbo(uint32_t swapchain)
{
	// Rotate and Translation the model matrix
	const glm::mat4 modelMtx = glm::rotate(_angleY, glm::vec3(0, 1, 0)) * glm::scale(glm::vec3(0.6f)) * _scene->getWorldMatrix(0);
	_angleY += (2 * glm::pi<float>() * getFrameTime() / 1000) / 10;

	// Set model view projection matrix
	const glm::mat4 mvMatrix = _viewMtx * modelMtx;

	{
		_deviceResources->uboMatricesBufferView.getElementByName(BufferEntryNames::Matrices::MVPMatrix, 0, swapchain).setValue(_projectionMtx * mvMatrix);
		_deviceResources->uboMatricesBufferView.getElementByName(BufferEntryNames::Matrices::MVInverseTransposeMatrix, 0, swapchain)
			.setValue(glm::mat3x4(glm::inverseTranspose(glm::mat3(mvMatrix))));

		// if the memory property flags used by the buffers' device memory do not contain e_HOST_COHERENT_BIT then we must flush the memory
		if (static_cast<uint32_t>(_deviceResources->uboMatrices->getDeviceMemory()->getMemoryFlags() & pvrvk::MemoryPropertyFlags::e_HOST_COHERENT_BIT) == 0)
		{
			_deviceResources->uboMatrices->getDeviceMemory()->flushRange(
				_deviceResources->uboMatricesBufferView.getDynamicSliceOffset(swapchain), _deviceResources->uboMatricesBufferView.getDynamicSliceSize());
		}
	}

	if (_uboMatData.isDirty)
	{
		_deviceResources->device->waitIdle();
		_deviceResources->uboMaterialBufferView.getElementByName(BufferEntryNames::Materials::AlbedoModulation).setValue(glm::vec4(_uboMatData.albedo, 0.0f));
		_deviceResources->uboMaterialBufferView.getElementByName(BufferEntryNames::Materials::SpecularExponent).setValue(_uboMatData.specularExponent);
		_deviceResources->uboMaterialBufferView.getElementByName(BufferEntryNames::Materials::Metallicity).setValue(_uboMatData.metallicity);
		_deviceResources->uboMaterialBufferView.getElementByName(BufferEntryNames::Materials::Reflectivity).setValue(_uboMatData.reflectivity);
		_uboMatData.isDirty = false;

		// if the memory property flags used by the buffers' device memory do not contain e_HOST_COHERENT_BIT then we must flush the memory
		if (static_cast<uint32_t>(_deviceResources->uboMaterial->getDeviceMemory()->getMemoryFlags() & pvrvk::MemoryPropertyFlags::e_HOST_COHERENT_BIT) == 0)
		{
			_deviceResources->uboMaterial->getDeviceMemory()->flushRange(0, _deviceResources->uboMaterialBufferView.getSize());
		}
	}
}

void VulkanPVRScopeRemote::createBuffers()
{
	{
		pvr::utils::StructuredMemoryDescription desc;
		desc.addElement(BufferEntryNames::Materials::AlbedoModulation, pvr::GpuDatatypes::vec3);
		desc.addElement(BufferEntryNames::Materials::SpecularExponent, pvr::GpuDatatypes::Float);
		desc.addElement(BufferEntryNames::Materials::Metallicity, pvr::GpuDatatypes::Float);
		desc.addElement(BufferEntryNames::Materials::Reflectivity, pvr::GpuDatatypes::Float);

		_deviceResources->uboMaterialBufferView.init(desc);
		_deviceResources->uboMaterial = pvr::utils::createBuffer(_deviceResources->device, _deviceResources->uboMaterialBufferView.getSize(),
			pvrvk::BufferUsageFlags::e_UNIFORM_BUFFER_BIT, pvrvk::MemoryPropertyFlags::e_HOST_VISIBLE_BIT,
			pvrvk::MemoryPropertyFlags::e_DEVICE_LOCAL_BIT | pvrvk::MemoryPropertyFlags::e_HOST_VISIBLE_BIT | pvrvk::MemoryPropertyFlags::e_HOST_COHERENT_BIT,
			&_deviceResources->vmaBufferAllocator, pvr::utils::vma::AllocationCreateFlags::e_MAPPED_BIT);

		_deviceResources->uboMaterialBufferView.pointToMappedMemory(_deviceResources->uboMaterial->getDeviceMemory()->getMappedData());
	}

	{
		pvr::utils::StructuredMemoryDescription desc;
		desc.addElement("viewLightDirection", pvr::GpuDatatypes::vec3);

		_deviceResources->uboLightingBufferView.init(desc);
		_deviceResources->uboLighting = pvr::utils::createBuffer(_deviceResources->device, _deviceResources->uboLightingBufferView.getSize(),
			pvrvk::BufferUsageFlags::e_UNIFORM_BUFFER_BIT, pvrvk::MemoryPropertyFlags::e_HOST_VISIBLE_BIT,
			pvrvk::MemoryPropertyFlags::e_DEVICE_LOCAL_BIT | pvrvk::MemoryPropertyFlags::e_HOST_VISIBLE_BIT | pvrvk::MemoryPropertyFlags::e_HOST_COHERENT_BIT,
			&_deviceResources->vmaBufferAllocator, pvr::utils::vma::AllocationCreateFlags::e_MAPPED_BIT);

		_deviceResources->uboLightingBufferView.pointToMappedMemory(_deviceResources->uboLighting->getDeviceMemory()->getMappedData());
	}
}

/*!*********************************************************************************************************************
\brief	pre-record the rendering the commands
***********************************************************************************************************************/
void VulkanPVRScopeRemote::recordCommandBuffer(uint32_t swapchain)
{
	CPPLProcessingScoped PPLProcessingScoped(_spsCommsData, __FUNCTION__, static_cast<uint32_t>(strlen(__FUNCTION__)), _frameCounter);

	_deviceResources->commandBuffer[swapchain]->begin();
	const pvrvk::ClearValue clearValues[2] = { pvrvk::ClearValue(0.00f, 0.70f, 0.67f, 1.0f), pvrvk::ClearValue(1.f, 0u) };
	_deviceResources->commandBuffer[swapchain]->beginRenderPass(
		_deviceResources->onScreenFramebuffer[swapchain], pvrvk::Rect2D(0, 0, getWidth(), getHeight()), true, clearValues, ARRAY_SIZE(clearValues));

	// Use shader program
	_deviceResources->commandBuffer[swapchain]->bindPipeline(_deviceResources->pipeline);

	// Bind texture
	_deviceResources->commandBuffer[swapchain]->bindDescriptorSet(
		pvrvk::PipelineBindPoint::e_GRAPHICS, _deviceResources->pipeline->getPipelineLayout(), 0, _deviceResources->modelDescriptorSets[swapchain], 0);
	_deviceResources->commandBuffer[swapchain]->bindDescriptorSet(
		pvrvk::PipelineBindPoint::e_GRAPHICS, _deviceResources->pipeline->getPipelineLayout(), 1, _deviceResources->lightingDescriptorSet, 0);

	drawMesh(0, _deviceResources->commandBuffer[swapchain]);

	_deviceResources->uiRenderer.beginRendering(_deviceResources->commandBuffer[swapchain]);
	// Displays the demo name using the tools. For a detailed explanation, see the example
	// IntroUIRenderer
	_deviceResources->uiRenderer.getDefaultTitle()->render();
	_deviceResources->uiRenderer.getDefaultDescription()->render();
	_deviceResources->uiRenderer.getSdkLogo()->render();
	_deviceResources->uiRenderer.getDefaultControls()->render();
	_deviceResources->uiRenderer.endRendering();
	_deviceResources->commandBuffer[swapchain]->endRenderPass();
	_deviceResources->commandBuffer[swapchain]->end();
}

/*!*********************************************************************************************************************
\return	Return auto ptr to the demo supplied by the user
\brief	This function must be implemented by the user of the shell. The user should return its Shell object defining the behavior of the application.
***********************************************************************************************************************/
std::unique_ptr<pvr::Shell> pvr::newDemo()
{
	return std::unique_ptr<pvr::Shell>(new VulkanPVRScopeRemote());
}
