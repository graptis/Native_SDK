/*!*********************************************************************************************************************
\file         VulkanGlass.cpp
\Title        Glass
\Author       PowerVR by Imagination, Developer Technology Team
\Copyright    Copyright (c) Imagination Technologies Limited.
\brief		  Demonstrates dynamic reflection and refraction by rendering two halves of the scene to a single rectangular texture.
***********************************************************************************************************************/
#include "PVRCore/PVRCore.h"
#include "PVRShell/PVRShell.h"
#include "PVRUtils/PVRUtilsVk.h"

// vertex bindings
const pvr::utils::VertexBindings_Name VertexBindings[] =
{
	{ "POSITION", "inVertex" }, { "NORMAL", "inNormal" }, { "UV0", "inTexCoords" }
};

// Shader uniforms
namespace ShaderUniforms {
enum Enum {	MVPMatrix, MVMatrix, MMatrix, InvVPMatrix, LightDir, EyePos, NumUniforms };
const char* names[] = {"MVPMatrix", "MVMatrix", "MMatrix", "InvVPMatrix", "LightDir", "EyePos"};
}

enum
{
	MaxSwapChain = 4
};

// paraboloid texture size
const uint32_t ParaboloidTexSize = 1024;

// camera constants
const float CamNear = 1.0f;
const float CamFar = 5000.0f;
const float CamFov = glm::pi<float>() * 0.41f;

// textures
const char* BalloonTexFile[2] = { "BalloonTex.pvr", "BalloonTex2.pvr" };
const char CubeTexFile[] = "SkyboxTex.pvr";

// model files
const char StatueFile[] = "scene.pod";
const char BalloonFile[] = "Balloon.pod";

// shaders
namespace Shaders {
const char* Names[] =
{
	"DefaultVertShader_vk.vsh.spv",
	"DefaultFragShader_vk.fsh.spv",
	"ParaboloidVertShader_vk.vsh.spv",
	"SkyboxVertShader_vk.vsh.spv",
	"SkyboxFragShader_vk.fsh.spv",
	"EffectReflectVertShader_vk.vsh.spv",
	"EffectReflectFragShader_vk.fsh.spv",
	"EffectRefractVertShader_vk.vsh.spv",
	"EffectRefractFragShader_vk.fsh.spv",
	"EffectChromaticDispersion_vk.vsh.spv",
	"EffectChromaticDispersion_vk.fsh.spv",
	"EffectReflectionRefraction_vk.vsh.spv",
	"EffectReflectionRefraction_vk.fsh.spv",
	"EffectReflectChromDispersion_vk.vsh.spv",
	"EffectReflectChromDispersion_vk.fsh.spv",
};

enum Enum
{
	DefaultVS,
	DefaultFS,
	ParaboloidVS,
	SkyboxVS,
	SkyboxFS,
	EffectReflectVS,
	EffectReflectFS,
	EffectRefractionVS,
	EffectRefractionFS,
	EffectChromaticDispersionVS,
	EffectChromaticDispersionFS,
	EffectReflectionRefractionVS,
	EffectReflectionRefractionFS,
	EffectReflectChromDispersionVS,
	EffectReflectChromDispersionFS,
	NumShaders
};
}

// effect mappings
namespace Effects {
enum Enum { ReflectChromDispersion, ReflectRefraction, Reflection, ChromaticDispersion, Refraction, NumEffects };
const char* Names[Effects::NumEffects] =
{
	"Reflection + Chromatic Dispersion", "Reflection + Refraction", "Reflection", "Chromatic Dispersion", "Refraction"
};
}

// clear color for the sky
const glm::vec4 ClearSkyColor(glm::vec4(.6f, 0.8f, 1.0f, 0.0f));

struct Model
{
	pvr::assets::ModelHandle handle;
	std::vector<pvrvk::Buffer> vbos;
	std::vector<pvrvk::Buffer> ibos;
};

static inline pvrvk::Sampler createTrilinearImageSampler(pvrvk::Device& device)
{
	pvrvk::SamplerCreateInfo samplerInfo;
	samplerInfo.wrapModeU = VkSamplerAddressMode::e_CLAMP_TO_EDGE;
	samplerInfo.wrapModeV = VkSamplerAddressMode::e_CLAMP_TO_EDGE;
	samplerInfo.wrapModeW = VkSamplerAddressMode::e_CLAMP_TO_EDGE;
	samplerInfo.minFilter = VkFilter::e_LINEAR;
	samplerInfo.magFilter = VkFilter::e_LINEAR;
	samplerInfo.mipMapMode = VkSamplerMipmapMode::e_LINEAR;
	return device->createSampler(samplerInfo);
}

// an abstract base for a rendering pass - handles the drawing of different types of meshes
struct IModelPass
{
private:
	void drawMesh(pvrvk::CommandBufferBase& command, const Model& model, uint32_t nodeIndex)
	{
		int32_t meshId = model.handle->getNode(nodeIndex).getObjectId();
		const pvr::assets::Mesh& mesh = model.handle->getMesh(meshId);

		// bind the VBO for the mesh
		command->bindVertexBuffer(model.vbos[meshId], 0, 0);
		if (mesh.getFaces().getDataSize() != 0)
		{
			// Indexed Triangle list
			command->bindIndexBuffer(model.ibos[meshId], 0, pvr::utils::convertToVk(mesh.getFaces().getDataType()));
			command->drawIndexed(0, mesh.getNumFaces() * 3, 0, 0, 1);
		}
		else
		{
			// Non-Indexed Triangle list
			command->draw(0, mesh.getNumFaces() * 3, 0, 1);
		}
	}

protected:

	void drawMesh(pvrvk::SecondaryCommandBuffer& command, const Model& model, uint32_t nodeIndex)
	{
		drawMesh((pvrvk::CommandBufferBase&)command, model, nodeIndex);
	}

	void drawMesh(pvrvk::CommandBuffer& command, const Model& model, uint32_t nodeIndex)
	{
		drawMesh((pvrvk::CommandBufferBase&)command, model, nodeIndex);
	}
};

// skybox pass
struct PassSkyBox
{
	pvr::utils::StructuredBufferView _bufferMemoryView;
	pvrvk::Buffer _buffer;
	pvrvk::GraphicsPipeline _pipeline;
	pvrvk::Buffer _vbo;
	pvrvk::DescriptorSetLayout _descriptorSetLayout;
	pvr::Multi<pvrvk::DescriptorSet> _descriptorSets;
	pvrvk::ImageView _skyboxTex;
	pvrvk::Sampler _trilinearSampler;
	pvr::Multi<pvrvk::SecondaryCommandBuffer> secondaryCommandBuffers;

	enum {UboInvViewProj, UboEyePos, UboElementCount};

	void update(uint32_t swapchain, const glm::mat4& invViewProj, const glm::vec3& eyePos)
	{
		void* memory;
		_buffer->getDeviceMemory()->map(&memory, _bufferMemoryView.getDynamicSliceOffset(swapchain), _bufferMemoryView.getDynamicSliceSize());
		_bufferMemoryView.pointToMappedMemory(memory, swapchain);
		_bufferMemoryView.getElement(UboInvViewProj, 0, swapchain).setValue(invViewProj);
		_bufferMemoryView.getElement(UboEyePos, 0, swapchain).setValue(glm::vec4(eyePos, 0.0f));
		_buffer->getDeviceMemory()->unmap();
	}

	pvrvk::ImageView getSkyBox()
	{
		return _skyboxTex;
	}

	pvrvk::GraphicsPipeline getPipeline() { return _pipeline; }

	bool initDescriptorSetLayout(pvrvk::Device& device)
	{
		// create skybox descriptor set layout
		pvrvk::DescriptorSetLayoutCreateInfo descSetLayout;

		// combined image sampler descriptor
		descSetLayout.setBinding(0, VkDescriptorType::e_COMBINED_IMAGE_SAMPLER, 1,
		                         VkShaderStageFlags::e_FRAGMENT_BIT);
		// uniform buffer
		descSetLayout.setBinding(1, VkDescriptorType::e_UNIFORM_BUFFER, 1,
		                         VkShaderStageFlags::e_VERTEX_BIT);

		_descriptorSetLayout = device->createDescriptorSetLayout(descSetLayout);

		return true;
	}

	bool initPipeline(pvr::Shell& shell,
	                  pvrvk::Device& device,
	                  const pvrvk::RenderPass& renderpass,
	                  const pvrvk::Extent2D& viewportDim, pvrvk::PipelineCache& pipeCache)
	{
		pvrvk::GraphicsPipelineCreateInfo pipeInfo;

		// on screen renderpass
		pipeInfo.renderPass = renderpass;

		// load, create and set the shaders for rendering the skybox
		auto& vertexShader = Shaders::Names[Shaders::SkyboxVS];
		auto& fragmentShader = Shaders::Names[Shaders::SkyboxFS];
		pvr::Stream::ptr_type vertexShaderSource = shell.getAssetStream(vertexShader);
		pvr::Stream::ptr_type fragmentShaderSource = shell.getAssetStream(fragmentShader);

		pipeInfo.vertexShader.setShader(device->createShader(vertexShaderSource->readToEnd<uint32_t>()));
		pipeInfo.fragmentShader.setShader(device->createShader(fragmentShaderSource->readToEnd<uint32_t>()));

		// create the pipeline layout
		pvrvk::PipelineLayoutCreateInfo pipelineLayout;
		pipelineLayout.setDescSetLayout(0, _descriptorSetLayout);

		pipeInfo.pipelineLayout = device->createPipelineLayout(pipelineLayout);

		// depth stencil state
		pipeInfo.depthStencil.enableDepthWrite(false);
		pipeInfo.depthStencil.enableDepthTest(false);

		// rasterizer state
		pipeInfo.rasterizer.setCullMode(VkCullModeFlags::e_FRONT_BIT);

		// blend state
		pipeInfo.colorBlend.setAttachmentState(0, pvrvk::PipelineColorBlendAttachmentState());

		// input assembler
		pipeInfo.inputAssembler.setPrimitiveTopology(VkPrimitiveTopology::e_TRIANGLE_LIST);

		// vertex attributes and bindings
		pipeInfo.vertexInput.clear();
		pipeInfo.vertexInput.addInputBinding(pvrvk::VertexInputBindingDescription(0, sizeof(float) * 3));
		pipeInfo.vertexInput.addInputAttribute(pvrvk::VertexInputAttributeDescription(0, 0, VkFormat::e_R32G32B32_SFLOAT, 0));

		pipeInfo.viewport.setViewportAndScissor(0, pvrvk::Viewport(0.0f, 0.0f, static_cast<float>(viewportDim.width), static_cast<float>(viewportDim.height)),
		                                        pvrvk::Rect2Di(0, 0, viewportDim.width, viewportDim.height));

		_pipeline = device->createGraphicsPipeline(pipeInfo, pipeCache);
		if (!_pipeline.isValid())
		{
			Log("Failed to create the skybox pipeline");
			return false;
		}
		return true;
	}

	void createBuffers(pvrvk::Device& device, uint32_t numSwapchain)
	{
		{
			// create the sky box vbo
			static float quadVertices[] =
			{
				-1,  1, 0.9999f,// upper left
				-1, -1, 0.9999f,// lower left
				1,  1, 0.9999f,// upper right
				1,  1, 0.9999f,// upper right
				-1, -1, 0.9999f,// lower left
				1, -1, 0.9999f// lower right
			};

			_vbo = pvr::utils::createBuffer(device, sizeof(quadVertices),
			                            VkBufferUsageFlags::e_VERTEX_BUFFER_BIT, VkMemoryPropertyFlags::e_HOST_VISIBLE_BIT);
			pvr::utils::updateBuffer(device, _vbo, quadVertices, 0, sizeof(quadVertices), true);
		}

		{
			// create the structured memory view
			pvr::utils::StructuredMemoryDescription desc;
			desc.addElement("InvVPMatrix", pvr::GpuDatatypes::mat4x4);
			desc.addElement("EyePos", pvr::GpuDatatypes::vec4);

			_bufferMemoryView.initDynamic(desc, numSwapchain, pvr::BufferUsageFlags::UniformBuffer,
			                              static_cast<uint32_t>(device->getPhysicalDevice()->getProperties().limits.minUniformBufferOffsetAlignment));

			_buffer = pvr::utils::createBuffer(device, _bufferMemoryView.getSize(), VkBufferUsageFlags::e_UNIFORM_BUFFER_BIT,
			                               VkMemoryPropertyFlags::e_HOST_VISIBLE_BIT | VkMemoryPropertyFlags::e_HOST_COHERENT_BIT);
		}
	}

	bool createDescriptorSets(pvrvk::Device& device,
	                          pvrvk::DescriptorPool& descriptorPool,
	                          pvrvk::Sampler& sampler, uint32_t numSwapchain)
	{
		pvrvk::WriteDescriptorSet writeDescSets[pvrvk::FrameworkCaps::MaxSwapChains * 2];
		// create a descriptor set per swapchain
		for (uint32_t i = 0; i < numSwapchain; ++i)
		{
			_descriptorSets.add(descriptorPool->allocateDescriptorSet(_descriptorSetLayout));
			writeDescSets[i * numSwapchain]
			.set(VkDescriptorType::e_COMBINED_IMAGE_SAMPLER, _descriptorSets[i], 0)
			.setImageInfo(0, pvrvk::DescriptorImageInfo(_skyboxTex, sampler,
			              VkImageLayout::e_SHADER_READ_ONLY_OPTIMAL));

			writeDescSets[i * numSwapchain + 1]
			.set(VkDescriptorType::e_UNIFORM_BUFFER, _descriptorSets[i], 1)
			.setBufferInfo(0, pvrvk::DescriptorBufferInfo(_buffer,
			    _bufferMemoryView.getDynamicSliceOffset(i),
			    _bufferMemoryView.getDynamicSliceSize()));
		}
		device->updateDescriptorSets(writeDescSets, numSwapchain * 2, nullptr, 0);
		return true;
	}

	bool init(pvr::Shell& shell, pvrvk::Device& device,
	          pvr::Multi<pvrvk::Framebuffer>& framebuffers, const pvrvk::RenderPass& renderpass,
	          pvrvk::CommandBuffer setupCommandBuffer, pvrvk::DescriptorPool& descriptorPool,
	          pvrvk::CommandPool& commandPool,
	          std::vector<pvr::utils::ImageUploadResults>& outImageUpload,
	          pvrvk::PipelineCache& pipeCache)
	{
		_trilinearSampler = createTrilinearImageSampler(device);
		initDescriptorSetLayout(device);
		createBuffers(device, static_cast<uint32_t>(framebuffers.size()));

		// load the  skybox texture
		outImageUpload.push_back(pvr::utils::loadAndUploadImage(device, CubeTexFile,
		                         true, setupCommandBuffer, shell));
		_skyboxTex = outImageUpload.back().getImageView();
		if (_skyboxTex.isNull())
		{
			Log("Failed to load Skybox texture");
			return false;
		}

		if (!createDescriptorSets(device, descriptorPool, _trilinearSampler, static_cast<uint32_t>(framebuffers.size())))
		{
			return false;
		}
		if (!initPipeline(shell, device, renderpass, framebuffers[0]->getDimensions(), pipeCache))
		{
			return false;
		}

		recordCommands(device, framebuffers, commandPool);

		return true;
	}

	pvrvk::SecondaryCommandBuffer& getSecondaryCommandBuffer(uint32_t swapchain)
	{
		return secondaryCommandBuffers[swapchain];
	}

	void recordCommands(pvrvk::Device& device,
	                    pvr::Multi<pvrvk::Framebuffer>& framebuffers, pvrvk::CommandPool& commandPool)
	{
		for (uint32_t i = 0; i < framebuffers.size(); ++i)
		{
			secondaryCommandBuffers[i] = commandPool->allocateSecondaryCommandBuffer();
			secondaryCommandBuffers[i]->begin(framebuffers[i], 0);
			secondaryCommandBuffers[i]->bindPipeline(_pipeline);
			secondaryCommandBuffers[i]->bindVertexBuffer(_vbo, 0, 0);
			secondaryCommandBuffers[i]->bindDescriptorSet(
			  VkPipelineBindPoint::e_GRAPHICS,
			  _pipeline->getPipelineLayout(), 0, _descriptorSets[i]);

			secondaryCommandBuffers[i]->draw(0, 6, 0, 1);

			secondaryCommandBuffers[i]->end();
		}
	}
};

// balloon pass
struct PassBalloon : public IModelPass
{
	// variable number of balloons
	enum {NumBalloon = 2};

	// structured memory view with entries for each balloon
	pvr::utils::StructuredBufferView _bufferMemoryView;
	pvrvk::Buffer _buffer;

	// descriptor set layout and per swap chain descriptor set
	pvrvk::DescriptorSetLayout _matrixBufferDescriptorSetLayout;
	pvr::Multi<pvrvk::DescriptorSet> _matrixDescriptorSets;

	pvrvk::DescriptorSetLayout _textureBufferDescriptorSetLayout;
	pvrvk::DescriptorSet _textureDescriptorSets[NumBalloon];

	// texture for each balloon
	pvrvk::ImageView _balloonTexures[NumBalloon];
	enum UboElement { UboElementModelViewProj, UboElementLightDir, UboElementEyePos, UboElementCount };
	enum UboBalloonIdElement { UboBalloonId };

	// graphics pipeline used for rendering the balloons
	pvrvk::GraphicsPipeline _pipeline;

	// container for the balloon model
	Model _balloonModel;

	pvrvk::Sampler _trilinearSampler;

	const glm::vec3 EyePos;
	const glm::vec3 LightDir;

	pvr::Multi<pvrvk::SecondaryCommandBuffer> _secondaryCommandBuffers;

	PassBalloon() : EyePos(0.0f, 0.0f, 0.0f), LightDir(19.0f, 22.0f, -50.0f) {}

	bool initDescriptorSetLayout(pvrvk::Device& device)
	{
		{
			pvrvk::DescriptorSetLayoutCreateInfo descSetLayout;
			// uniform buffer
			descSetLayout.setBinding(0, VkDescriptorType::e_UNIFORM_BUFFER_DYNAMIC, 1,
			                         VkShaderStageFlags::e_VERTEX_BIT);
			_matrixBufferDescriptorSetLayout = device->createDescriptorSetLayout(descSetLayout);
		}

		{
			pvrvk::DescriptorSetLayoutCreateInfo descSetLayout;
			// combined image sampler descriptor
			descSetLayout.setBinding(0, VkDescriptorType::e_COMBINED_IMAGE_SAMPLER, 1,
			                         VkShaderStageFlags::e_FRAGMENT_BIT);
			_textureBufferDescriptorSetLayout = device->createDescriptorSetLayout(descSetLayout);
		}
		return true;
	}

	void createBuffers(pvrvk::Device& device, uint32_t swapchainLength)
	{
		pvr::utils::appendSingleBuffersFromModel(device, *_balloonModel.handle,
		    _balloonModel.vbos, _balloonModel.ibos);

		// create the structured memory view
		pvr::utils::StructuredMemoryDescription desc;
		desc.addElement("UboElementModelViewProj", pvr::GpuDatatypes::mat4x4);
		desc.addElement("UboElementLightDir", pvr::GpuDatatypes::vec4);
		desc.addElement("UboElementEyePos", pvr::GpuDatatypes::vec4);

		_bufferMemoryView.initDynamic(desc, NumBalloon * swapchainLength, pvr::BufferUsageFlags::UniformBuffer,
		                              static_cast<uint32_t>(device->getPhysicalDevice()->getProperties().limits.minUniformBufferOffsetAlignment));
		_buffer = pvr::utils::createBuffer(device,_bufferMemoryView.getSize(), VkBufferUsageFlags::e_UNIFORM_BUFFER_BIT,
		                               VkMemoryPropertyFlags::e_HOST_VISIBLE_BIT | VkMemoryPropertyFlags::e_HOST_COHERENT_BIT);
	}

	bool createDescriptorSets(pvrvk::Device& device,
	                          pvrvk::Sampler& sampler, pvrvk::DescriptorPool& descpool,
	                          uint32_t numSwapchain)
	{
		pvrvk::WriteDescriptorSet writeDescSet[pvrvk::FrameworkCaps::MaxSwapChains + NumBalloon];
		uint32_t writeIndex = 0;
		// create a descriptor set per swapchain
		for (uint32_t i = 0; i < numSwapchain; ++i, ++writeIndex)
		{
			_matrixDescriptorSets.add(
			  descpool->allocateDescriptorSet(_matrixBufferDescriptorSetLayout));

			writeDescSet[writeIndex]
			.set(VkDescriptorType::e_UNIFORM_BUFFER_DYNAMIC, _matrixDescriptorSets[i])
			.setBufferInfo(0, pvrvk::DescriptorBufferInfo(_buffer, 0, _bufferMemoryView.getDynamicSliceSize()));
		}

		for (uint32_t i = 0; i < NumBalloon; ++i, ++writeIndex)
		{
			_textureDescriptorSets[i] = descpool->allocateDescriptorSet(
			                              _textureBufferDescriptorSetLayout);

			writeDescSet[writeIndex]
			.set(VkDescriptorType::e_COMBINED_IMAGE_SAMPLER, _textureDescriptorSets[i])
			.setImageInfo(0, pvrvk::DescriptorImageInfo(_balloonTexures[i], sampler,
			              VkImageLayout::e_SHADER_READ_ONLY_OPTIMAL));
		}
		device->updateDescriptorSets(writeDescSet, numSwapchain + NumBalloon, nullptr, 0);

		return true;
	}

	void setPipeline(pvrvk::GraphicsPipeline& pipeline)
	{
		_pipeline = pipeline;
	}

	bool initPipeline(pvr::Shell& shell,
	                  pvrvk::Device& device,
	                  const pvrvk::RenderPass& renderpass,
	                  const pvrvk::Extent2D& viewportDim,
	                  pvrvk::PipelineCache& pipeCache)
	{
		pvrvk::GraphicsPipelineCreateInfo pipeInfo;

		// on screen renderpass
		pipeInfo.renderPass = renderpass;

		// load, create and set the shaders for rendering the skybox
		auto& vertexShader = Shaders::Names[Shaders::DefaultVS];
		auto& fragmentShader = Shaders::Names[Shaders::DefaultFS];
		pvr::Stream::ptr_type vertexShaderSource = shell.getAssetStream(vertexShader);
		pvr::Stream::ptr_type fragmentShaderSource = shell.getAssetStream(fragmentShader);

		pipeInfo.vertexShader.setShader(device->createShader(vertexShaderSource->readToEnd<uint32_t>()));
		pipeInfo.fragmentShader.setShader(device->createShader(fragmentShaderSource->readToEnd<uint32_t>()));

		// create the pipeline layout
		pvrvk::PipelineLayoutCreateInfo pipelineLayout;
		pipelineLayout.setDescSetLayout(0, _matrixBufferDescriptorSetLayout);
		pipelineLayout.setDescSetLayout(1, _textureBufferDescriptorSetLayout);

		pipeInfo.pipelineLayout = device->createPipelineLayout(pipelineLayout);

		// depth stencil state
		pipeInfo.depthStencil.enableDepthWrite(true);
		pipeInfo.depthStencil.enableDepthTest(true);

		// rasterizer state
		pipeInfo.rasterizer.setCullMode(VkCullModeFlags::e_BACK_BIT);

		// blend state
		pipeInfo.colorBlend.setAttachmentState(0, pvrvk::PipelineColorBlendAttachmentState());

		// input assembler
		pipeInfo.inputAssembler.setPrimitiveTopology(VkPrimitiveTopology::e_TRIANGLE_LIST);
		pvr::utils::populateInputAssemblyFromMesh(_balloonModel.handle->getMesh(0), VertexBindings,
		    sizeof(VertexBindings) / sizeof(VertexBindings[0]), pipeInfo.vertexInput, pipeInfo.inputAssembler);

		pipeInfo.viewport.setViewportAndScissor(0, pvrvk::Viewport(0.0f, 0.0f, static_cast<float>(viewportDim.width),
		                                        static_cast<float>(viewportDim.height)), pvrvk::Rect2Di(0, 0, viewportDim.width, viewportDim.height));

		_pipeline = device->createGraphicsPipeline(pipeInfo, pipeCache);
		if (!_pipeline.isValid())
		{
			Log("Failed to create the balloon pipeline");
			return false;
		}
		return true;
	}

	bool init(
	  pvr::Shell& shell,
	  pvrvk::Device& device,
	  pvr::Multi<pvrvk::Framebuffer>& framebuffers,
	  const pvrvk::RenderPass& renderpass,
	  pvrvk::CommandBuffer& setupCmdBuffer,
	  pvrvk::DescriptorPool& descriptorPool,
	  pvrvk::CommandPool& commandPool,
	  std::vector<pvr::utils::ImageUploadResults>& imageUploads,
	  const Model& modelBalloon,
	  pvrvk::PipelineCache& pipeCache)
	{
		_balloonModel = modelBalloon;

		_trilinearSampler = createTrilinearImageSampler(device);
		initDescriptorSetLayout(device);
		createBuffers(device, static_cast<uint32_t>(framebuffers.size()));

		for (uint32_t i = 0; i < NumBalloon; ++i)
		{
			imageUploads.push_back(pvr::utils::loadAndUploadImage(
			                         device, BalloonTexFile[i], true, setupCmdBuffer, shell));
			_balloonTexures[i] = imageUploads.back().getImageView();
			if (!_balloonTexures[i].isValid())
			{
				return false;
			}
		}

		if (!createDescriptorSets(device, _trilinearSampler, descriptorPool, static_cast<uint32_t>(framebuffers.size())))
		{
			return false;
		}

		// create the pipeline
		if (!initPipeline(shell, device, renderpass, framebuffers[0]->getDimensions(), pipeCache))
		{
			return false;
		}
		recordCommands(device, framebuffers, commandPool);
		return true;
	}

	void recordCommands(pvrvk::Device& device,
	    pvr::Multi<pvrvk::Framebuffer>& framebuffers, pvrvk::CommandPool& commandPool)
	{
		for (uint32_t i = 0; i < framebuffers.size(); ++i)
		{
			_secondaryCommandBuffers[i] = commandPool->allocateSecondaryCommandBuffer();
			_secondaryCommandBuffers[i]->begin(framebuffers[i], 0);

			recordCommandsIntoSecondary(_secondaryCommandBuffers[i],
			    _bufferMemoryView, _matrixDescriptorSets[i],
			    _bufferMemoryView.getDynamicSliceOffset(i * NumBalloon));

			_secondaryCommandBuffers[i]->end();
		}
	}

	void recordCommandsIntoSecondary(pvrvk::SecondaryCommandBuffer& command,
	                                 pvr::utils::StructuredBufferView& bufferView,
	                                 pvrvk::DescriptorSet& matrixDescriptorSet, uint32_t baseOffset)
	{
		command->bindPipeline(_pipeline);
		for (uint32_t i = 0; i < NumBalloon; ++i)
		{
			const uint32_t offset = bufferView.getDynamicSliceOffset(i) + baseOffset;

			command->bindDescriptorSet(VkPipelineBindPoint::e_GRAPHICS,
			                           _pipeline->getPipelineLayout(), 0, matrixDescriptorSet, &offset, 1);

			command->bindDescriptorSet(VkPipelineBindPoint::e_GRAPHICS,
			                           _pipeline->getPipelineLayout(), 1, _textureDescriptorSets[i]);
			drawMesh(command, _balloonModel, 0);
		}
	}

	pvrvk::SecondaryCommandBuffer& getSecondaryCommandBuffer(uint32_t swapchain)
	{
		return _secondaryCommandBuffers[swapchain];
	}

	void update(uint32_t swapchain, const glm::mat4 model[NumBalloon],
	            const glm::mat4& view, const glm::mat4& proj)
	{
		void* memory;
		uint32_t mappedDynamicSlice = swapchain * NumBalloon;
		_buffer->getDeviceMemory()->map(&memory, _bufferMemoryView.getDynamicSliceOffset(mappedDynamicSlice), _bufferMemoryView.getDynamicSliceSize() * NumBalloon);
		_bufferMemoryView.pointToMappedMemory(memory, mappedDynamicSlice);

		for (uint32_t i = 0; i < NumBalloon; ++i)
		{
			const glm::mat4 modelView = view * model[i];
			uint32_t dynamicSlice = i + mappedDynamicSlice;

			_bufferMemoryView.getElement(UboElementModelViewProj, 0, dynamicSlice).setValue(proj * modelView);
			// Calculate and set the model space light direction
			_bufferMemoryView.getElement(UboElementLightDir, 0, dynamicSlice).setValue(glm::normalize(glm::inverse(model[i]) * glm::vec4(LightDir, 1.0f)));
			// Calculate and set the model space eye position
			_bufferMemoryView.getElement(UboElementEyePos, 0, dynamicSlice).setValue(glm::inverse(modelView) * glm::vec4(EyePos, 0.0f));
		}

		_buffer->getDeviceMemory()->unmap();
	}
};

// paraboloid pass
struct PassParabloid
{
	enum { ParabolidLeft, ParabolidRight, NumParabloid = 2};
private:
	enum { UboMV, UboLightDir, UboEyePos, UboNear, UboFar, UboCount };
	enum UboBalloonIdElement { UboBalloonId };
	const static std::pair<pvr::StringHash, pvr::GpuDatatypes> UboElementMap[UboCount];

	PassBalloon _passes[NumParabloid];
	pvrvk::GraphicsPipeline _pipelines[2];
	pvr::Multi<pvrvk::Framebuffer> _framebuffer;
	pvr::Multi<pvrvk::ImageView> _paraboloidTextures;
	pvrvk::RenderPass _renderPass;
	pvrvk::Sampler _trilinearSampler;
	pvrvk::DescriptorSetLayout _descriptorSetLayout;
	pvr::utils::StructuredBufferView _bufferMemoryView;
	pvrvk::Buffer _buffer;
	pvr::Multi<pvrvk::DescriptorSet> _matrixDescriptorSets;
	pvrvk::DescriptorSet _textureDescriptorSets[PassBalloon::NumBalloon];

	pvr::Multi<pvrvk::SecondaryCommandBuffer> _secondaryCommandBuffers;

	bool initPipeline(pvr::Shell& shell, pvrvk::Device& device, const Model& modelBalloon,  pvrvk::PipelineCache& pipeCache)
	{
		pvrvk::Rect2Di parabolidViewport[] =
		{
			pvrvk::Rect2Di(0, 0, ParaboloidTexSize, ParaboloidTexSize),				// first parabolid (Viewport left)
			pvrvk::Rect2Di(ParaboloidTexSize, 0, ParaboloidTexSize, ParaboloidTexSize) // second paraboloid (Viewport right)
		};

		//create the first pipeline for the left viewport
		pvrvk::GraphicsPipelineCreateInfo pipeInfo;

		pipeInfo.renderPass = _renderPass;

		pipeInfo.vertexShader.setShader(device->createShader(
		                                  shell.getAssetStream(Shaders::Names[Shaders::ParaboloidVS])->readToEnd<uint32_t>()));

		pipeInfo.fragmentShader.setShader(device->createShader(
		                                    shell.getAssetStream(Shaders::Names[Shaders::DefaultFS])->readToEnd<uint32_t>()));

		// create the pipeline layout
		pvrvk::PipelineLayoutCreateInfo pipelineLayout;
		pipelineLayout.setDescSetLayout(0, _descriptorSetLayout);
		pipelineLayout.setDescSetLayout(1, _passes[0]._textureBufferDescriptorSetLayout);

		pipeInfo.pipelineLayout = device->createPipelineLayout(pipelineLayout);

		// blend state
		pipeInfo.colorBlend.setAttachmentState(0, pvrvk::PipelineColorBlendAttachmentState());

		// input assembler
		pipeInfo.inputAssembler.setPrimitiveTopology(VkPrimitiveTopology::e_TRIANGLE_LIST);

		pvr::utils::populateInputAssemblyFromMesh(modelBalloon.handle->getMesh(0),
		    VertexBindings, sizeof(VertexBindings) / sizeof(VertexBindings[0]),
		    pipeInfo.vertexInput, pipeInfo.inputAssembler);

		// depth stencil state
		pipeInfo.depthStencil.enableDepthWrite(true);
		pipeInfo.depthStencil.enableDepthTest(true);

		// rasterizer state
		pipeInfo.rasterizer.setCullMode(VkCullModeFlags::e_FRONT_BIT);

		// set the viewport to render to the left paraboloid
		pipeInfo.viewport.setViewportAndScissor(0,
		                                        pvrvk::Viewport(parabolidViewport[0]),
		                                        pvrvk::Rect2Di(0, 0, ParaboloidTexSize * 2, ParaboloidTexSize));

		// create the left paraboloid graphics pipeline
		_pipelines[0] = device->createGraphicsPipeline(pipeInfo, pipeCache);

		// clear viewport/scissors before resetting them
		pipeInfo.viewport.clear();

		// create the second pipeline for the right viewport
		pipeInfo.viewport.setViewportAndScissor(0, pvrvk::Viewport(
		    parabolidViewport[1]), pvrvk::Rect2Di(0, 0, ParaboloidTexSize * 2, ParaboloidTexSize));
		pipeInfo.rasterizer.setCullMode(VkCullModeFlags::e_BACK_BIT);

		// create the right paraboloid graphics pipeline
		_pipelines[1] = device->createGraphicsPipeline(pipeInfo, pipeCache);

		// validate paraboloid pipeline creation
		if (!_pipelines[0].isValid() || !_pipelines[1].isValid())
		{
			("Faild to create paraboild pipelines");
			return false;
		}
		return true;
	}

	bool initFramebuffer(pvrvk::Device& device,
	                     pvrvk::CommandBuffer& setupCmdBuffer, uint32_t numSwapchain)
	{
		// create the paraboloid subpass
		pvrvk::SubPassDescription subpass(VkPipelineBindPoint::e_GRAPHICS);
		// uses a single color attachment
		subpass.setColorAttachmentReference(0, pvrvk::AttachmentReference(0, VkImageLayout::e_COLOR_ATTACHMENT_OPTIMAL));
		// subpass uses depth stencil attachment
		subpass.setDepthStencilAttachmentReference(pvrvk::AttachmentReference(1, VkImageLayout::e_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));


		VkFormat depthStencilFormat = VkFormat::e_D16_UNORM;
		VkFormat colorFormat = VkFormat::e_R8G8B8A8_UNORM;

		//create the renderpass
		// set the final layout to ShaderReadOnlyOptimal so that the image can be bound as a texture in following passes.
		pvrvk::RenderPassCreateInfo renderPassInfo;
		// clear the image at the beginning of the renderpass and store it at the end
		// the images initial layout will be color attachment optimal and the final layout will be shader read only optimal
		renderPassInfo.setAttachmentDescription(0,
		                                        pvrvk::AttachmentDescription::createColorDescription(colorFormat,
		                                            VkImageLayout::e_UNDEFINED, VkImageLayout::e_SHADER_READ_ONLY_OPTIMAL,
		                                            VkAttachmentLoadOp::e_CLEAR));

		// clear the depth stencil image at the beginning of the renderpass and ignore at the end
		renderPassInfo.setAttachmentDescription(1, pvrvk::AttachmentDescription::createDepthStencilDescription(
		    depthStencilFormat, VkImageLayout::e_UNDEFINED,
		    VkImageLayout::e_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		    VkAttachmentLoadOp::e_CLEAR, VkAttachmentStoreOp::e_DONT_CARE,
		    VkAttachmentLoadOp::e_DONT_CARE, VkAttachmentStoreOp::e_DONT_CARE));

		renderPassInfo.setSubPass(0, subpass);

		// create the renderpass to use when rendering into the paraboloid
		_renderPass = device->createRenderPass(renderPassInfo);

		// the paraboloid will be split up into left and right sections when rendering
		const pvrvk::Extent2D framebufferDim(ParaboloidTexSize * 2, ParaboloidTexSize);
		const pvrvk::Extent3D textureDim(framebufferDim, 1u);
		_framebuffer.resize(numSwapchain);

		for (uint32_t i = 0; i < numSwapchain; ++i)
		{
			//---------------
			// create the render-target color texture and transform to
			// shader read layout so that the layout transformtion
			// works properly durring the command buffer recording.
			pvrvk::Image colorTexture = pvr::utils::createImage(device, VkImageType::e_2D, colorFormat,
			                            textureDim, VkImageUsageFlags::e_COLOR_ATTACHMENT_BIT | VkImageUsageFlags::e_SAMPLED_BIT,
			                            VkImageCreateFlags(0), pvrvk::ImageLayersSize(), VkSampleCountFlags::e_1_BIT,
			                            VkMemoryPropertyFlags::e_DEVICE_LOCAL_BIT);

			_paraboloidTextures[i] = device->createImageView(colorTexture);

			//---------------
			// create the render-target depth-stencil texture
			// make depth stencil attachment transient as it is only used within this renderpass
			pvrvk::Image depthTexture = pvr::utils::createImage(device, VkImageType::e_2D,
			                            depthStencilFormat, textureDim, VkImageUsageFlags::e_DEPTH_STENCIL_ATTACHMENT_BIT |
			                            VkImageUsageFlags::e_TRANSIENT_ATTACHMENT_BIT, VkImageCreateFlags(0), pvrvk::ImageLayersSize(),
			                            VkSampleCountFlags::e_1_BIT, VkMemoryPropertyFlags::e_LAZILY_ALLOCATED_BIT);

			//---------------
			// create the framebuffer
			pvrvk::FramebufferCreateInfo framebufferInfo;
			framebufferInfo.setRenderPass(_renderPass);
			framebufferInfo.setAttachment(0, _paraboloidTextures[i]);
			framebufferInfo.setAttachment(1, device->createImageView(depthTexture));
			framebufferInfo.setDimensions(framebufferDim);

			_framebuffer[i] = device->createFramebuffer(framebufferInfo);
			if (!_framebuffer[i].isValid())
			{
				("failed to create the paraboloid framebuffer");
				return false;
			}
		}
		return true;
	}

	void createBuffers(pvrvk::Device& device, uint32_t numSwapchain)
	{
		// create the structured memory view
		pvr::utils::StructuredMemoryDescription desc;
		desc.addElement(UboElementMap[PassParabloid::UboMV].first, UboElementMap[PassParabloid::UboMV].second);
		desc.addElement(UboElementMap[PassParabloid::UboLightDir].first, UboElementMap[PassParabloid::UboLightDir].second);
		desc.addElement(UboElementMap[PassParabloid::UboEyePos].first, UboElementMap[PassParabloid::UboEyePos].second);
		desc.addElement(UboElementMap[PassParabloid::UboNear].first, UboElementMap[PassParabloid::UboNear].second);
		desc.addElement(UboElementMap[PassParabloid::UboFar].first, UboElementMap[PassParabloid::UboFar].second);

		_bufferMemoryView.initDynamic(desc, PassBalloon::NumBalloon * NumParabloid * numSwapchain, pvr::BufferUsageFlags::UniformBuffer,
		                              static_cast<uint32_t>(device->getPhysicalDevice()->getProperties().limits.minUniformBufferOffsetAlignment));
		_buffer = pvr::utils::createBuffer(device,_bufferMemoryView.getSize(), VkBufferUsageFlags::e_UNIFORM_BUFFER_BIT,
		                               VkMemoryPropertyFlags::e_HOST_VISIBLE_BIT | VkMemoryPropertyFlags::e_HOST_COHERENT_BIT);
	}

	bool initDescriptorSetLayout(pvrvk::Device& device)
	{
		// create skybox descriptor set layout
		pvrvk::DescriptorSetLayoutCreateInfo descSetLayout;

		// uniform buffer
		descSetLayout.setBinding(0, VkDescriptorType::e_UNIFORM_BUFFER_DYNAMIC, 1, VkShaderStageFlags::e_VERTEX_BIT);

		_descriptorSetLayout = device->createDescriptorSetLayout(descSetLayout);

		return true;
	}

	bool createDescriptorSets(pvrvk::Device& device,
	                          pvrvk::DescriptorPool& descriptorPool, uint32_t numSwapchain)
	{
		pvrvk::WriteDescriptorSet descSetWrites[pvrvk::FrameworkCaps::MaxSwapChains];

		// create a descriptor set per swapchain
		for (uint32_t i = 0; i < numSwapchain; ++i)
		{
			_matrixDescriptorSets.add(descriptorPool->allocateDescriptorSet(_descriptorSetLayout));
			descSetWrites[i]
			.set(VkDescriptorType::e_UNIFORM_BUFFER_DYNAMIC, _matrixDescriptorSets[i], 0)
			.setBufferInfo(0, pvrvk::DescriptorBufferInfo(_buffer, 0, _bufferMemoryView.getDynamicSliceSize()));
		}
		device->updateDescriptorSets(descSetWrites, numSwapchain, nullptr, 0);
		return true;
	}

public:
	pvrvk::Framebuffer& getFramebuffer(uint32_t swapchainIndex)
	{
		return _framebuffer[swapchainIndex];
	}

	const pvrvk::ImageView& getParaboloid(uint32_t swapchainIndex)
	{
		return _paraboloidTextures[swapchainIndex];
	}

	bool init(pvr::Shell& shell, pvrvk::Device& device,
	          const Model& modelBalloon, pvrvk::CommandBuffer& setupCmdBuffer,
	          pvrvk::CommandPool& commandPool, pvrvk::DescriptorPool& descriptorPool,
	          uint32_t numSwapchain, std::vector<pvr::utils::ImageUploadResults>& outResults,
	          pvrvk::PipelineCache& pipeCache)
	{
		if (!initFramebuffer(device, setupCmdBuffer, numSwapchain)) { return false; }

		for (uint32_t i = 0; i < NumParabloid; i++)
		{
			_passes[i].init(shell, device, _framebuffer, _renderPass, setupCmdBuffer,
			                descriptorPool, commandPool, outResults, modelBalloon, pipeCache);
		}

		_trilinearSampler = createTrilinearImageSampler(device);
		initDescriptorSetLayout(device);
		createBuffers(device, numSwapchain);
		if (!createDescriptorSets(device, descriptorPool, numSwapchain))
		{
			return false;
		}

		// create the pipeline
		if (!initPipeline(shell, device, modelBalloon, pipeCache)) { return false; }

		for (uint32_t i = 0; i < NumParabloid; i++)
		{
			_passes[i].setPipeline(_pipelines[i]);
		}

		recordCommands(commandPool, numSwapchain);

		return true;
	}

	void update(uint32_t swapchain,
	            const glm::mat4 balloonModelMatrices[PassBalloon::NumBalloon],
	            const glm::vec3& position)
	{
		//--- Create the first view matrix and make it flip the X coordinate
		glm::mat4 mViewLeft = glm::lookAt(position, position +
		                                  glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));
		mViewLeft = glm::scale(glm::vec3(-1.0f, 1.0f, 1.0f)) * mViewLeft;

		glm::mat4 mViewRight = glm::lookAt(position, position -
		                                   glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));
		glm::mat4 modelView;

		void* memory;
		uint32_t mappedDynamicSlice = swapchain * PassBalloon::NumBalloon * NumParabloid;
		// map the whole of the current swap chain buffer
		_buffer->getDeviceMemory()->map(&memory, _bufferMemoryView.getDynamicSliceOffset(mappedDynamicSlice),
		                                _bufferMemoryView.getDynamicSliceSize() * PassBalloon::NumBalloon * NumParabloid);
		_bufferMemoryView.pointToMappedMemory(memory, mappedDynamicSlice);

		// [LeftParaboloid_balloon0, LeftParaboloid_balloon1, RightParaboloid_balloon0, RightParaboloid_balloon1]
		for (uint32_t i = 0; i < PassBalloon::NumBalloon; ++i)
		{
			// left paraboloid
			{
				const uint32_t dynamicSlice = i + mappedDynamicSlice;
				modelView = mViewLeft * balloonModelMatrices[i];
				_bufferMemoryView.getElement(UboMV, 0, dynamicSlice).setValue(modelView);
				_bufferMemoryView.getElement(UboLightDir, 0, dynamicSlice).setValue(
				    glm::normalize(glm::inverse(balloonModelMatrices[i]) *
				    glm::vec4(_passes[i].LightDir, 1.0f)));

				// Calculate and set the model space eye position
				_bufferMemoryView.getElement(UboEyePos, 0, dynamicSlice).setValue(glm::inverse(modelView) *
				    glm::vec4(_passes[i].EyePos, 0.0f));
				_bufferMemoryView.getElement(UboNear, 0, dynamicSlice).setValue(CamNear);
				_bufferMemoryView.getElement(UboFar, 0, dynamicSlice).setValue(CamFar);
			}
			// right paraboloid
			{
				const uint32_t dynamicSlice = i + PassBalloon::NumBalloon + mappedDynamicSlice;
				modelView = mViewRight * balloonModelMatrices[i];
				_bufferMemoryView.getElement(UboMV, 0, dynamicSlice).setValue(modelView);
				_bufferMemoryView.getElement(UboLightDir, 0, dynamicSlice).setValue(glm::normalize(glm::inverse(balloonModelMatrices[i]) *
				    glm::vec4(_passes[i].LightDir, 1.0f)));

				// Calculate and set the model space eye position
				_bufferMemoryView.getElement(UboEyePos, 0, dynamicSlice).setValue(glm::inverse(modelView) * glm::vec4(_passes[i].EyePos, 0.0f));
				_bufferMemoryView.getElement(UboNear, 0, dynamicSlice).setValue(CamNear);
				_bufferMemoryView.getElement(UboFar, 0, dynamicSlice).setValue(CamFar);
			}
		}

		_buffer->getDeviceMemory()->unmap();
	}

	pvrvk::SecondaryCommandBuffer& getSecondaryCommandBuffer(uint32_t swapchain)
	{
		return _secondaryCommandBuffers[swapchain];
	}

	void recordCommands(pvrvk::CommandPool& commandPool, uint32_t swapchain)
	{
		for (uint32_t i = 0; i < swapchain; ++i)
		{
			_secondaryCommandBuffers[i] = commandPool->allocateSecondaryCommandBuffer();

			_secondaryCommandBuffers[i]->begin(_framebuffer[i], 0);

			// left paraboloid
			uint32_t baseOffset = _bufferMemoryView.getDynamicSliceOffset(
			    i * PassBalloon::NumBalloon * NumParabloid);
			_passes[ParabolidLeft].recordCommandsIntoSecondary(_secondaryCommandBuffers[i],
			    _bufferMemoryView, _matrixDescriptorSets[i], baseOffset);
			// right paraboloid

			baseOffset = _bufferMemoryView.getDynamicSliceOffset(
			    i * PassBalloon::NumBalloon * NumParabloid + PassBalloon::NumBalloon);
			_passes[ParabolidRight].recordCommandsIntoSecondary(_secondaryCommandBuffers[i],
			    _bufferMemoryView, _matrixDescriptorSets[i], baseOffset);

			_secondaryCommandBuffers[i]->end();
		}
	}
};

const std::pair<pvr::StringHash, pvr::GpuDatatypes>
PassParabloid::UboElementMap[PassParabloid::UboCount] =
{
	{ "MVMatrix", pvr::GpuDatatypes::mat4x4 },
	{ "LightDir", pvr::GpuDatatypes::vec4 },
	{ "EyePos", pvr::GpuDatatypes::vec4 },
	{ "Near", pvr::GpuDatatypes::Float },
	{ "Far", pvr::GpuDatatypes::Float },
};

struct PassStatue : public IModelPass
{
	pvrvk::GraphicsPipeline _effectPipelines[Effects::NumEffects];

	pvr::utils::StructuredBufferView _bufferMemoryView;
	pvrvk::Buffer _buffer;
	pvrvk::DescriptorSetLayout _descriptorSetLayout;
	pvr::Multi<pvrvk::DescriptorSet> _descriptorSets;
	pvrvk::Sampler _trilinearSampler;

	struct Model _modelStatue;

	pvr::Multi<pvrvk::SecondaryCommandBuffer> _secondaryCommandBuffers;

	enum { DescSetUbo, DescSetParabolid, DescSetSkybox,};
	enum UboElements { MVP, Model, EyePos, Count };
	static  const std::pair<pvr::StringHash, pvr::GpuDatatypes> UboElementsNames[UboElements::Count];

	bool initDescriptorSetLayout(pvrvk::Device& device)
	{
		// create skybox descriptor set layout
		pvrvk::DescriptorSetLayoutCreateInfo descSetLayout;

		// combined image sampler descriptors
		descSetLayout.setBinding(1, VkDescriptorType::e_COMBINED_IMAGE_SAMPLER, 1, VkShaderStageFlags::e_FRAGMENT_BIT);

		descSetLayout.setBinding(2, VkDescriptorType::e_COMBINED_IMAGE_SAMPLER, 1, VkShaderStageFlags::e_FRAGMENT_BIT);

		// uniform buffer
		descSetLayout.setBinding(0, VkDescriptorType::e_UNIFORM_BUFFER_DYNAMIC, 1, VkShaderStageFlags::e_VERTEX_BIT);

		_descriptorSetLayout = device->createDescriptorSetLayout(descSetLayout);

		return true;
	}

	void createBuffers(pvrvk::Device& device, uint32_t numSwapchain)
	{
		// create the vbo & ibos
		pvr::utils::appendSingleBuffersFromModel(device,
		    *this->_modelStatue.handle, this->_modelStatue.vbos, this->_modelStatue.ibos);

		{
			// create the structured memory view
			pvr::utils::StructuredMemoryDescription desc;
			desc.addElement(UboElementsNames[UboElements::MVP].first, UboElementsNames[UboElements::MVP].second);
			desc.addElement(UboElementsNames[UboElements::Model].first, UboElementsNames[UboElements::Model].second);
			desc.addElement(UboElementsNames[UboElements::EyePos].first, UboElementsNames[UboElements::EyePos].second);

			_bufferMemoryView.initDynamic(desc, _modelStatue.handle->getNumMeshNodes() * numSwapchain, pvr::BufferUsageFlags::UniformBuffer,
			                              static_cast<uint32_t>(device->getPhysicalDevice()->getProperties().limits.minUniformBufferOffsetAlignment));
			_buffer = pvr::utils::createBuffer(device,_bufferMemoryView.getSize(),
			                               VkBufferUsageFlags::e_UNIFORM_BUFFER_BIT, VkMemoryPropertyFlags::e_HOST_VISIBLE_BIT | VkMemoryPropertyFlags::e_HOST_COHERENT_BIT);
		}
	}

	bool createDescriptorSets(pvrvk::Device& device,
	                          PassParabloid& passParabloid, PassSkyBox& passSkybox,
	                          pvrvk::Sampler& sampler, pvrvk::DescriptorPool& descriptorPool,
	                          uint32_t numSwapchain)
	{
		pvrvk::WriteDescriptorSet writeDescSets[pvrvk::FrameworkCaps::MaxSwapChains * 3];
		// create a descriptor set per swapchain
		for (uint32_t i = 0; i < numSwapchain; ++i)
		{
			_descriptorSets.add(descriptorPool->allocateDescriptorSet(_descriptorSetLayout));
			writeDescSets[i * 3]
			.set(VkDescriptorType::e_UNIFORM_BUFFER_DYNAMIC, _descriptorSets[i], 0)
			.setBufferInfo(0, pvrvk::DescriptorBufferInfo(_buffer, 0, _bufferMemoryView.getDynamicSliceSize()));

			writeDescSets[i * 3 + 1]
			.set(VkDescriptorType::e_COMBINED_IMAGE_SAMPLER, _descriptorSets[i], 1)
			.setImageInfo(0, pvrvk::DescriptorImageInfo(passParabloid.getParaboloid(i), sampler,
			              VkImageLayout::e_SHADER_READ_ONLY_OPTIMAL));

			writeDescSets[i * 3 + 2]
			.set(VkDescriptorType::e_COMBINED_IMAGE_SAMPLER, _descriptorSets[i], 2)
			.setImageInfo(0, pvrvk::DescriptorImageInfo(passSkybox.getSkyBox(), sampler,
			              VkImageLayout::e_SHADER_READ_ONLY_OPTIMAL));
		}
		device->updateDescriptorSets(writeDescSets, numSwapchain * 3, nullptr, 0);
		return true;
	}

	bool initEffectPipelines(pvr::Shell& shell,
	                         pvrvk::Device& device,
	                         const pvrvk::RenderPass& renderpass,
	                         const pvrvk::Extent2D& viewportDim,
	                         pvrvk::PipelineCache& pipeCache)
	{
		pvrvk::GraphicsPipelineCreateInfo pipeInfo;

		// on screen renderpass
		pipeInfo.renderPass = renderpass;

		// create the pipeline layout
		pvrvk::PipelineLayoutCreateInfo pipelineLayout;
		pipelineLayout.setDescSetLayout(0, _descriptorSetLayout);

		pipeInfo.pipelineLayout = device->createPipelineLayout(pipelineLayout);

		// depth stencil state
		pipeInfo.depthStencil.enableDepthWrite(true);
		pipeInfo.depthStencil.enableDepthTest(true);

		// rasterizer state
		pipeInfo.rasterizer.setCullMode(VkCullModeFlags::e_BACK_BIT);

		// blend state
		pipeInfo.colorBlend.setAttachmentState(0, pvrvk::PipelineColorBlendAttachmentState());

		// input assembler
		pipeInfo.inputAssembler.setPrimitiveTopology(VkPrimitiveTopology::e_TRIANGLE_LIST);

		pvr::utils::populateInputAssemblyFromMesh(_modelStatue.handle->getMesh(0),
		    VertexBindings, 2, pipeInfo.vertexInput, pipeInfo.inputAssembler);

		// load, create and set the shaders for rendering the skybox
		auto& vertexShader = Shaders::Names[Shaders::SkyboxVS];
		auto& fragmentShader = Shaders::Names[Shaders::SkyboxFS];
		pvr::Stream::ptr_type vertexShaderSource = shell.getAssetStream(vertexShader);
		pvr::Stream::ptr_type fragmentShaderSource = shell.getAssetStream(fragmentShader);

		pipeInfo.vertexShader.setShader(device->createShader(vertexShaderSource->readToEnd<uint32_t>()));
		pipeInfo.fragmentShader.setShader(device->createShader(fragmentShaderSource->readToEnd<uint32_t>()));

		pipeInfo.viewport.setViewportAndScissor(0, pvrvk::Viewport(0.0f, 0.0f, static_cast<float>(viewportDim.width), static_cast<float>(viewportDim.height)),
		                                        pvrvk::Rect2Di(0, 0, viewportDim.width, viewportDim.height));

		pvrvk::Shader shaders[Shaders::NumShaders];
		for (uint32_t i = 0; i < Shaders::NumShaders; ++i)
		{
			shaders[i] = device->createShader(shell.getAssetStream(Shaders::Names[i])->readToEnd<uint32_t>());
			if (!shaders[i].isValid())
			{
				("Failed to create the demo effect shaders");
				return false;
			}
		}

		// Effects Vertex and fragment shader
		std::pair<Shaders::Enum, Shaders::Enum> effectShaders[Effects::NumEffects] =
		{
			{ Shaders::EffectReflectChromDispersionVS, Shaders::EffectReflectChromDispersionFS }, // ReflectChromDispersion
			{ Shaders::EffectReflectionRefractionVS, Shaders::EffectReflectionRefractionFS },//ReflectRefraction
			{ Shaders::EffectReflectVS, Shaders::EffectReflectFS },// Reflection
			{ Shaders::EffectChromaticDispersionVS, Shaders::EffectChromaticDispersionFS },// ChromaticDispersion
			{ Shaders::EffectRefractionVS, Shaders::EffectRefractionFS }// Refraction
		};

		for (uint32_t i = 0; i < Effects::NumEffects; ++i)
		{
			pipeInfo.vertexShader.setShader(shaders[effectShaders[i].first]);
			pipeInfo.fragmentShader.setShader(shaders[effectShaders[i].second]);
			_effectPipelines[i] = device->createGraphicsPipeline(pipeInfo, pipeCache);
			if (!_effectPipelines[i].isValid())
			{
				Log("Failed to create the effects pipelines");
				return false;
			}
		}

		return true;
	}

	bool init(pvr::Shell& shell, pvrvk::Device& device,
	          pvrvk::CommandBuffer& setupCmdBuffer, pvrvk::DescriptorPool& descriptorPool,
	          std::vector<pvr::utils::ImageUploadResults>& imageUploads,
	          uint32_t numSwapchain, const struct Model& modelStatue,
	          PassParabloid& passParabloid, PassSkyBox& passSkybox,
	          const pvrvk::RenderPass& renderpass,
	          const pvrvk::Extent2D& viewportDim, pvrvk::PipelineCache& pipeCache)
	{
		_modelStatue = modelStatue;

		_trilinearSampler = createTrilinearImageSampler(device);
		initDescriptorSetLayout(device);
		createBuffers(device, numSwapchain);
		if (!createDescriptorSets(device, passParabloid, passSkybox,
		                          _trilinearSampler, descriptorPool, numSwapchain))
		{
			return false;
		}
		if (!initEffectPipelines(shell, device, renderpass, viewportDim, pipeCache))
		{
			return false;
		}

		return true;
	}

	void recordCommands(pvrvk::Device& device,
	                    pvrvk::CommandPool& commandPool, uint32_t pipeEffect,
	                    pvrvk::Framebuffer& framebuffer, uint32_t swapchain)
	{
		// create the command buffer if it does not already exist
		if (!_secondaryCommandBuffers[swapchain].isValid())
		{
			_secondaryCommandBuffers[swapchain] = commandPool->allocateSecondaryCommandBuffer();
		}

		_secondaryCommandBuffers[swapchain]->begin(framebuffer, 0);

		_secondaryCommandBuffers[swapchain]->bindPipeline(_effectPipelines[pipeEffect]);
		// bind the texture and samplers and the ubos

		for (uint32_t i = 0; i < _modelStatue.handle->getNumMeshNodes(); i++)
		{
			uint32_t offsets = _bufferMemoryView.getDynamicSliceOffset(i + swapchain * _modelStatue.handle->getNumMeshNodes());
			_secondaryCommandBuffers[swapchain]->bindDescriptorSet(VkPipelineBindPoint::e_GRAPHICS,
			    _effectPipelines[pipeEffect]->getPipelineLayout(), 0, _descriptorSets[swapchain], &offsets, 1);
			drawMesh(_secondaryCommandBuffers[swapchain], _modelStatue, 0);
		}

		_secondaryCommandBuffers[swapchain]->end();
	}

	pvrvk::SecondaryCommandBuffer& getSecondaryCommandBuffer(uint32_t swapchain)
	{
		return _secondaryCommandBuffers[swapchain];
	}

	void update(uint32_t swapchain, const glm::mat4& view, const glm::mat4& proj)
	{
		// The final statue transform brings him with 0.0.0 coordinates at his feet.
		// For this model we want 0.0.0 to be the around the center of the statue, and the statue to be smaller.
		// So, we apply a transformation, AFTER all transforms that have brought him to the center,
		// that will shrink him and move him downwards.
		void* memory;
		uint32_t mappedDynamicSlice = swapchain * _modelStatue.handle->getNumMeshNodes();
		_buffer->getDeviceMemory()->map(&memory, _bufferMemoryView.getDynamicSliceOffset(mappedDynamicSlice),
		                                _bufferMemoryView.getDynamicSliceSize() * _modelStatue.handle->getNumMeshNodes());
		_bufferMemoryView.pointToMappedMemory(memory, mappedDynamicSlice);
		static const glm::vec3 scale = glm::vec3(0.25f, 0.25f, 0.25f);
		static const glm::vec3 offset = glm::vec3(0.f, -2.f, 0.f);
		static const glm::mat4 local_transform = glm::translate(offset) * glm::scale(scale);

		for (uint32_t i = 0; i < _modelStatue.handle->getNumMeshNodes(); ++i)
		{
			uint32_t dynamicSlice = i + mappedDynamicSlice;
			const glm::mat4& modelMat = local_transform * _modelStatue.handle->getWorldMatrix(i);
			const glm::mat3 modelMat3x3 = glm::mat3(modelMat);

			const glm::mat4& modelView = view * modelMat;
			_bufferMemoryView.getElement(UboElements::MVP, 0, dynamicSlice).setValue(proj * modelView);
			_bufferMemoryView.getElement(UboElements::Model, 0, dynamicSlice).setValue(modelMat3x3);
			_bufferMemoryView.getElement(UboElements::EyePos, 0, dynamicSlice).setValue(glm::inverse(modelView) * glm::vec4(0, 0, 0, 1));
		}
		_buffer->getDeviceMemory()->unmap();
	}
};

const std::pair<pvr::StringHash, pvr::GpuDatatypes>
PassStatue::UboElementsNames[PassStatue::UboElements::Count]
{
	{ "MVPMatrix", pvr::GpuDatatypes::mat4x4 },
	{ "MMatrix", pvr::GpuDatatypes::mat3x3 },
	{ "EyePos", pvr::GpuDatatypes::vec4 },
};

/*!*********************************************************************************************************************
 Class implementing the Shell functions.
***********************************************************************************************************************/
class VulkanGlass : public pvr::Shell
{
	struct DeviceResources
	{
		pvrvk::Instance instance;
		pvrvk::Device device;

		pvrvk::PipelineCache pipeCache;

		// UIRenderer used to display text
		pvr::ui::UIRenderer uiRenderer;

		Model balloon;
		Model statue;

		pvr::Multi<pvrvk::Framebuffer> onScreenFramebuffer;

		// related sets of drawing commands are grouped into "passes"
		PassSkyBox passSkyBox;
		PassParabloid passParaboloid;
		PassStatue  passStatue;
		PassBalloon passBalloon;

		pvr::Multi<pvrvk::CommandBuffer> sceneCommandBuffers;
		pvr::Multi<pvrvk::SecondaryCommandBuffer> uiSecondaryCommandBuffers;
		pvr::Multi<pvrvk::ImageView> depthStencilImages;
		pvrvk::Sampler samplerTrilinear;

		pvrvk::CommandPool commandPool;
		pvrvk::DescriptorPool descriptorPool;
		pvrvk::Swapchain swapchain;
		pvrvk::Surface surface;
		pvrvk::Queue queue;
		pvrvk::Semaphore semaphoreAcquire[uint32_t(pvrvk::FrameworkCaps::MaxSwapChains)];
		pvrvk::Semaphore semaphoreSubmit[uint32_t(pvrvk::FrameworkCaps::MaxSwapChains)];
		pvrvk::Fence perFrameFence[uint32_t(pvrvk::FrameworkCaps::MaxSwapChains)];
	};

	std::unique_ptr<DeviceResources> _deviceResources;

	// Projection, view and model matrices
	glm::mat4 _projectionMatrix;
	glm::mat4 _viewMatrix;

	// Rotation angle for the model
	float _cameraAngle;
	float _balloonAngle[PassBalloon::NumBalloon];
	int32_t _currentEffect;
	float _tilt;
	float _currentTilt;
	uint32_t _frameId;
public:
	VulkanGlass() : _tilt(0), _currentTilt(0) {}
	virtual pvr::Result initApplication();
	virtual pvr::Result initView();
	virtual pvr::Result releaseView();
	virtual pvr::Result quitApplication();
	virtual pvr::Result renderFrame();
private:
	void eventMappedInput(pvr::SimplifiedInput action);
	void updateScene(uint32_t swapchainIndex);
	void recordCommands();
};

void VulkanGlass::eventMappedInput(pvr::SimplifiedInput action)
{
	switch (action)
	{
	case pvr::SimplifiedInput::Left:
		_currentEffect -= 1;
		_currentEffect = (_currentEffect + Effects::NumEffects) % Effects::NumEffects;
		_deviceResources->uiRenderer.getDefaultDescription()->setText(Effects::Names[_currentEffect]);
		_deviceResources->uiRenderer.getDefaultDescription()->commitUpdates();
		_deviceResources->device->waitIdle();// make sure the command buffer is finished before re-recording
		recordCommands();
		break;
	case pvr::SimplifiedInput::Up:
		_tilt += 5.f;
		break;
	case pvr::SimplifiedInput::Down:
		_tilt -= 5.f;
		break;
	case pvr::SimplifiedInput::Right:
		_currentEffect += 1;
		_currentEffect = (_currentEffect + Effects::NumEffects) % Effects::NumEffects;
		_deviceResources->uiRenderer.getDefaultDescription()->setText(Effects::Names[_currentEffect]);
		_deviceResources->uiRenderer.getDefaultDescription()->commitUpdates();
		_deviceResources->device->waitIdle();// make sure the command buffer is finished before re-recording
		recordCommands();
		break;
	case pvr::SimplifiedInput::ActionClose: exitShell(); break;
	}
}

/*!*********************************************************************************************************************
\return	Return Result::Success if no error occurred
\brief	Code in initApplication() will be called by PVRShell once perrun, before the rendering device is created.
        Used to initialize variables that are not dependent on it (e.g. external modules, loading meshes, etc.)
        If the rendering device is lost, initApplication() will not be called again.
***********************************************************************************************************************/
pvr::Result VulkanGlass::initApplication()
{
	_deviceResources.reset(new DeviceResources());

	_cameraAngle =  glm::pi<float>() - .6f;

	for (int i = 0; i < PassBalloon::NumBalloon; ++i) { _balloonAngle[i] = glm::pi<float>() * i / 5.f;	}

	_currentEffect = 0;

	// load the balloon
	if (!pvr::assets::helper::loadModel(*this, BalloonFile, _deviceResources->balloon.handle))
	{
		setExitMessage("ERROR: Couldn't load the %s file\n", BalloonFile);
		return pvr::Result::UnknownError;
	}

	// load the statue
	if (!pvr::assets::helper::loadModel(*this, StatueFile, _deviceResources->statue.handle))
	{
		setExitMessage("ERROR: Couldn't load the %s file", StatueFile);
		return pvr::Result::UnknownError;
	}


	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\return	Return Result::Success if no error occurred
\brief	Code in quitApplication() will be called by PVRShell once per run, just before exiting the program.If the rendering device
        is lost, quitApplication() will not be called.
***********************************************************************************************************************/
pvr::Result VulkanGlass::quitApplication() { return pvr::Result::Success; }

/*!*********************************************************************************************************************
\return	Return Result::Success if no error occurred
\brief	Code in lPVREgl() will be called by PVRShell upon initialization or after a change in the rendering device.
        Used to initialize variables that are dependent on the rendering device (e.g. textures, vertex buffers, etc.)
***********************************************************************************************************************/
pvr::Result VulkanGlass::initView()
{
	_frameId = 0;
	// create the vk instance
	if (!pvr::utils::createInstanceAndSurface(this->getApplicationName(), this->getWindow(), this->getDisplay(), _deviceResources->instance, _deviceResources->surface))
	{
		return pvr::Result::UnknownError;
	}

	// create the logical device and the queues
	const pvr::utils::QueuePopulateInfo populateInfo =
	{
		VkQueueFlags::e_GRAPHICS_BIT, _deviceResources->surface
	};
	pvr::utils::QueueAccessInfo queueAccessInfo;
	_deviceResources->device = pvr::utils::createDeviceAndQueues(
	                             _deviceResources->instance->getPhysicalDevice(0), &populateInfo, 1, &queueAccessInfo);

	// Get the queue
	_deviceResources->queue = _deviceResources->device->getQueue(queueAccessInfo.familyId, queueAccessInfo.queueId);

	pvrvk::SurfaceCapabilitiesKHR surfaceCapabilities = _deviceResources->instance->getPhysicalDevice(0)->getSurfaceCapabilities(_deviceResources->surface);

	// validate the supported swapchain image usage
	VkImageUsageFlags swapchainImageUsage = VkImageUsageFlags::e_COLOR_ATTACHMENT_BIT;
	if (pvr::utils::isImageUsageSupportedBySurface(surfaceCapabilities, VkImageUsageFlags::e_TRANSFER_SRC_BIT))
	{
		swapchainImageUsage |= VkImageUsageFlags::e_TRANSFER_SRC_BIT;
	}

	//---------------
	// create the swapchain
	if (!pvr::utils::createSwapchainAndDepthStencilImageView(_deviceResources->device,
	    _deviceResources->surface, getDisplayAttributes(), _deviceResources->swapchain, _deviceResources->depthStencilImages, swapchainImageUsage))
	{
		return pvr::Result::UnknownError;
	}

	//---------------
	// create the framebuffer
	if (!pvr::utils::createOnscreenFramebufferAndRenderpass(_deviceResources->swapchain,
	    &_deviceResources->depthStencilImages[0], _deviceResources->onScreenFramebuffer))
	{
		setExitMessage("Failed to create onscreen framebuffer");
		return pvr::Result::UnknownError;
	}

	//---------------
	// Create the commandpool
	_deviceResources->commandPool = _deviceResources->device->createCommandPool(_deviceResources->queue->getQueueFamilyId(),
	                                VkCommandPoolCreateFlags::e_RESET_COMMAND_BUFFER_BIT);

	//---------------
	// Create the DescriptorPool
	pvrvk::DescriptorPoolCreateInfo descPoolInfo; descPoolInfo
	.addDescriptorInfo(VkDescriptorType::e_COMBINED_IMAGE_SAMPLER, 32)
	.addDescriptorInfo(VkDescriptorType::e_UNIFORM_BUFFER_DYNAMIC, 32)
	.addDescriptorInfo(VkDescriptorType::e_UNIFORM_BUFFER, 32)
	.setMaxDescriptorSets(32);

	_deviceResources->descriptorPool = _deviceResources->device->createDescriptorPool(descPoolInfo);

	_deviceResources->sceneCommandBuffers[0] = _deviceResources->commandPool->allocateCommandBuffer();
	// Prepare the per swapchain resources
	// set Swapchain and depthstencil attachment image initial layout
	for (uint32_t i = 0; i < _deviceResources->swapchain->getSwapchainLength(); ++i)
	{
		_deviceResources->sceneCommandBuffers[i] = _deviceResources->commandPool->allocateCommandBuffer();
		_deviceResources->uiSecondaryCommandBuffers[i] = _deviceResources->commandPool->allocateSecondaryCommandBuffer();
		if (i == 0)
		{
			_deviceResources->sceneCommandBuffers[0]->begin();
		}

		_deviceResources->semaphoreSubmit[i] = _deviceResources->device->createSemaphore();
		_deviceResources->semaphoreAcquire[i] = _deviceResources->device->createSemaphore();
		_deviceResources->perFrameFence[i] = _deviceResources->device->createFence(VkFenceCreateFlags::e_SIGNALED_BIT);
	}

	// Create the pipeline cache
	_deviceResources->pipeCache = _deviceResources->device->createPipelineCache(0, 0, 0);


	std::vector<pvr::utils::ImageUploadResults> imageUploads(0);

	// set up the passes
	if (!_deviceResources->passSkyBox.init(*this, _deviceResources->device,
	                                       _deviceResources->onScreenFramebuffer, _deviceResources->onScreenFramebuffer[0]->getRenderPass(),
	                                       _deviceResources->sceneCommandBuffers[0], _deviceResources->descriptorPool, _deviceResources->commandPool,
	                                       imageUploads, _deviceResources->pipeCache))
	{
		setExitMessage("Failed to initialize the Skybox pass");
		return pvr::Result::UnknownError;
	}

	if (!_deviceResources->passBalloon.init(*this, _deviceResources->device,
	                                        _deviceResources->onScreenFramebuffer, _deviceResources->onScreenFramebuffer[0]->getRenderPass(),
	                                        _deviceResources->sceneCommandBuffers[0], _deviceResources->descriptorPool, _deviceResources->commandPool,
	                                        imageUploads, _deviceResources->balloon, _deviceResources->pipeCache))
	{
		setExitMessage("Failed to initialize Balloon pass");
		return pvr::Result::UnknownError;
	}

	if (!_deviceResources->passParaboloid.init(*this, _deviceResources->device, _deviceResources->balloon,
	    _deviceResources->sceneCommandBuffers[0], _deviceResources->commandPool, _deviceResources->descriptorPool,
	    _deviceResources->swapchain->getSwapchainLength(), imageUploads,
	    _deviceResources->pipeCache))
	{
		setExitMessage("Failed to initialize Paraboloid pass");
		return pvr::Result::UnknownError;
	}

	if (!_deviceResources->passStatue.init(*this, _deviceResources->device,
	                                       _deviceResources->sceneCommandBuffers[0], _deviceResources->descriptorPool,
	                                       imageUploads, _deviceResources->swapchain->getSwapchainLength(),
	                                       _deviceResources->statue, _deviceResources->passParaboloid, _deviceResources->passSkyBox,
	                                       _deviceResources->onScreenFramebuffer[0]->getRenderPass(),
	                                       _deviceResources->onScreenFramebuffer[0]->getDimensions(),
	                                       _deviceResources->pipeCache))
	{
		setExitMessage("Failed to initialize Statue pass"); return pvr::Result::UnknownError;
	}

	// Initialize UIRenderer
	if (!_deviceResources->uiRenderer.init(getWidth(), getHeight(), isFullScreen(),
	                                       _deviceResources->onScreenFramebuffer[0]->getRenderPass(), 0, _deviceResources->commandPool,
	                                       _deviceResources->queue))
	{
		this->setExitMessage("ERROR: Cannot initialize UIRenderer\n");
		return pvr::Result::UnknownError;
	}

	//---------------
	// Submit the inital commands
	_deviceResources->sceneCommandBuffers[0]->end();
	pvrvk::SubmitInfo submitInfo;
	submitInfo.commandBuffers = &_deviceResources->sceneCommandBuffers[0];
	submitInfo.numCommandBuffers = 1;
	_deviceResources->queue->submit(&submitInfo, 1);
	_deviceResources->queue->waitIdle();// make sure all the uploads are finished

	_deviceResources->uiRenderer.getDefaultTitle()->setText("Glass");
	_deviceResources->uiRenderer.getDefaultTitle()->commitUpdates();
	_deviceResources->uiRenderer.getDefaultDescription()->setText(Effects::Names[_currentEffect]);
	_deviceResources->uiRenderer.getDefaultDescription()->commitUpdates();
	_deviceResources->uiRenderer.getDefaultControls()->setText("Left / Right : Change the"
	    " effect\nUp / Down  : Tilt camera");
	_deviceResources->uiRenderer.getDefaultControls()->commitUpdates();
	//Calculate the projection and view matrices
	_projectionMatrix = pvr::math::perspectiveFov(pvr::Api::Vulkan, CamFov,
	                    (float)this->getWidth(), (float)this->getHeight(),
	                    CamNear, CamFar, (isScreenRotated() ? glm::pi<float>() * .5f : 0.0f));
	recordCommands();
	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\return	Result::Success if no error occurred
\brief	Code in releaseView() will be called by Shell when the application quits or before a change in the rendering device.
***********************************************************************************************************************/
pvr::Result VulkanGlass::releaseView()
{
	_deviceResources->device->waitIdle();
	_deviceResources.reset();
	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\return	Return Result::Success if no error occurred
\brief	Main rendering loop function of the program. The shell will call this function every frame.
***********************************************************************************************************************/
pvr::Result VulkanGlass::renderFrame()
{
	// make sure the commandbuffer and the semaphore are free to use.
	_deviceResources->perFrameFence[_frameId]->wait();
	_deviceResources->perFrameFence[_frameId]->reset();
	_deviceResources->swapchain->acquireNextImage(uint64_t(-1), _deviceResources->semaphoreAcquire[_frameId]);
	const uint32_t swapchainIndex = _deviceResources->swapchain->getSwapchainIndex();
	updateScene(swapchainIndex);

	//--------------------
	// Submit the graphics Commands
	pvrvk::SubmitInfo submitInfo;
	VkPipelineStageFlags waitStage = VkPipelineStageFlags::e_TRANSFER_BIT;
	submitInfo.commandBuffers = &_deviceResources->sceneCommandBuffers[swapchainIndex];
	submitInfo.numCommandBuffers = 1;
	submitInfo.numWaitSemaphores = 1;
	submitInfo.waitSemaphores = &_deviceResources->semaphoreAcquire[_frameId];
	submitInfo.waitDestStages = &waitStage;
	submitInfo.signalSemaphores = &_deviceResources->semaphoreSubmit[_frameId];
	submitInfo.numSignalSemaphores = 1;
	_deviceResources->queue->submit(&submitInfo, 1, _deviceResources->perFrameFence[_frameId]);

	if (this->shouldTakeScreenshot())
	{
		if (_deviceResources->swapchain->supportsUsage(VkImageUsageFlags::e_TRANSFER_SRC_BIT))
		{
			pvr::utils::takeScreenshot(_deviceResources->swapchain, swapchainIndex, _deviceResources->commandPool, _deviceResources->queue, this->getScreenshotFileName());
		}
		else
		{
			Log(LogLevel::Warning, "Could not take screenshot as the swapchain does not support TRANSFER_SRC_BIT");
		}
	}

	//--------------------
	// Present
	pvrvk::PresentInfo presentInfo;
	presentInfo.swapchains = &_deviceResources->swapchain;
	presentInfo.numWaitSemaphores = 1;
	presentInfo.waitSemaphores = &_deviceResources->semaphoreSubmit[_frameId];
	presentInfo.imageIndices = &swapchainIndex;
	presentInfo.numSwapchains = 1;

	_deviceResources->queue->present(presentInfo);

	++_frameId;
	_frameId %= _deviceResources->swapchain->getSwapchainLength();
	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\brief	Update the scene
***********************************************************************************************************************/
void VulkanGlass::updateScene(uint32_t swapchainIndex)
{
	// Fetch current time and make sure the previous time isn't greater
	uint64_t timeDifference = getFrameTime();
	// Store the current time for the next frame
	_cameraAngle += timeDifference * 0.00005f;
	for (int32_t i = 0; i < PassBalloon::NumBalloon; ++i)
	{
		_balloonAngle[i] += timeDifference * 0.0002f * (float(i) * .5f + 1.f);
	}

	static const glm::vec3 rotateAxis(0.0f, 1.0f, 0.0f);
	float diff = fabs(_tilt - _currentTilt);
	float diff2 = timeDifference / 20.f;
	_currentTilt += glm::sign(_tilt - _currentTilt) * (std::min)(diff, diff2);

	// Rotate the camera
	_viewMatrix = glm::lookAt(glm::vec3(0, -4, -10), glm::vec3(0, _currentTilt - 3, 0),
	                          glm::vec3(0, 1, 0)) * glm::rotate(_cameraAngle, rotateAxis);

	static glm::mat4 balloonModelMatrices[PassBalloon::NumBalloon];
	for (int32_t i = 0; i < PassBalloon::NumBalloon; ++i)
	{
		// Rotate the balloon model matrices
		balloonModelMatrices[i] = glm::rotate(_balloonAngle[i], rotateAxis) *
		                          glm::translate(glm::vec3(120.f + i * 40.f,
		                              sin(_balloonAngle[i] * 3.0f) * 20.0f, 0.0f)) *
		                          glm::scale(glm::vec3(3.0f, 3.0f, 3.0f));
	}
	_deviceResources->passParaboloid.update(swapchainIndex, balloonModelMatrices, glm::vec3(0, 0, 0));
	_deviceResources->passStatue.update(swapchainIndex, _viewMatrix, _projectionMatrix);
	_deviceResources->passBalloon.update(swapchainIndex, balloonModelMatrices, _viewMatrix, _projectionMatrix);
	_deviceResources->passSkyBox.update(swapchainIndex, glm::inverse(_projectionMatrix * _viewMatrix),
	                                    glm::vec3(glm::inverse(_viewMatrix) * glm::vec4(0, 0, 0, 1)));
}

/*!*********************************************************************************************************************
\brief	record all the secondary command buffers
***********************************************************************************************************************/
void VulkanGlass::recordCommands()
{
	pvrvk::ClearValue paraboloidPassClearValues[8];
	pvr::utils::populateClearValues(_deviceResources->passParaboloid.getFramebuffer(0)->getRenderPass(),
	                                pvrvk::ClearValue(ClearSkyColor.r, ClearSkyColor.g, ClearSkyColor.b, ClearSkyColor.a),
	                                pvrvk::ClearValue::createDefaultDepthStencilClearValue(), paraboloidPassClearValues);

	const pvrvk::ClearValue onScreenClearValues[2] =
	{
		pvrvk::ClearValue(ClearSkyColor.r, ClearSkyColor.g, ClearSkyColor.b, ClearSkyColor.a),
		pvrvk::ClearValue::createDefaultDepthStencilClearValue()
	};

	for (uint32_t i = 0; i < _deviceResources->swapchain->getSwapchainLength(); ++i)
	{
		//---------------
		// Render the UIRenderer
		_deviceResources->uiRenderer.beginRendering(_deviceResources->uiSecondaryCommandBuffers[i], _deviceResources->onScreenFramebuffer[i]);
		_deviceResources->uiRenderer.getSdkLogo()->render();
		_deviceResources->uiRenderer.getDefaultTitle()->render();
		_deviceResources->uiRenderer.getDefaultDescription()->render();
		_deviceResources->uiRenderer.getDefaultControls()->render();
		_deviceResources->uiRenderer.endRendering();

		// record the statue pass with the current effect
		_deviceResources->passStatue.recordCommands(_deviceResources->device, _deviceResources->commandPool,
		    _currentEffect, _deviceResources->onScreenFramebuffer[i], i);

		_deviceResources->sceneCommandBuffers[i]->begin();

		// Render into the paraboloid
		_deviceResources->sceneCommandBuffers[i]->beginRenderPass(_deviceResources->passParaboloid.getFramebuffer(i),
		    pvrvk::Rect2Di(0, 0, 2 * ParaboloidTexSize, ParaboloidTexSize), false,
		    paraboloidPassClearValues, _deviceResources->passParaboloid.getFramebuffer(i)->getNumAttachments());

		_deviceResources->sceneCommandBuffers[i]->executeCommands(_deviceResources->passParaboloid.getSecondaryCommandBuffer(i));

		_deviceResources->sceneCommandBuffers[i]->endRenderPass();

		// Create the final commandbuffer
		// make use of the paraboloid and render the other elements of the scene
		_deviceResources->sceneCommandBuffers[i]->beginRenderPass(_deviceResources->onScreenFramebuffer[i],
		    pvrvk::Rect2Di(0, 0, getWidth(), getHeight()), false, onScreenClearValues, ARRAY_SIZE(onScreenClearValues));

		_deviceResources->sceneCommandBuffers[i]->executeCommands(_deviceResources->passSkyBox.getSecondaryCommandBuffer(i));

		_deviceResources->sceneCommandBuffers[i]->executeCommands(_deviceResources->passBalloon.getSecondaryCommandBuffer(i));

		_deviceResources->sceneCommandBuffers[i]->executeCommands(_deviceResources->passStatue.getSecondaryCommandBuffer(i));

		_deviceResources->sceneCommandBuffers[i]->executeCommands(_deviceResources->uiSecondaryCommandBuffers[i]);

		_deviceResources->sceneCommandBuffers[i]->endRenderPass();
		_deviceResources->sceneCommandBuffers[i]->end();
	}
}

/*!*********************************************************************************************************************
\return	auto ptr of the demo supplied by the user
\brief	This function must be implemented by the user of the shell. The user should return its PVRShell object defining the
        behaviour of the application.
***********************************************************************************************************************/
std::unique_ptr<pvr::Shell> pvr::newDemo() {	return std::unique_ptr<pvr::Shell>(new VulkanGlass()); }
