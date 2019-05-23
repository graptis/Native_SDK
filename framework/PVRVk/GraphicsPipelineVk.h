/*!
\brief The PVRVk GraphicsPipeline. This is an interface for a Vulkan VkPipeline
that was built for the VK_BINDING_POINT_GRAPHICS, separating it from the corresponding Compute pipeline
\file PVRVk/GraphicsPipelineVk.h
\author PowerVR by Imagination, Developer Technology Team
\copyright Copyright (c) Imagination Technologies Limited.
*/
#pragma once
#include "PVRVk/PipelineVk.h"
#include "PVRVk/PipelineConfigVk.h"
#include "PVRVk/DeviceVk.h"
namespace pvrvk {
/// <summary>This represents all the information needed to create a GraphicsPipeline. All items must have proper
/// values for a pipeline to be successfully created, but all those for which it is possible (except, for example,
/// Shaders and Vertex Formats) will have defaults same as their default values OpenGL ES graphics API.
///
/// NOTES: The folloowing are required
///  - at least one viewport & scissor
///  - renderpass
///  - pipeline layout
/// </summary>
struct GraphicsPipelineCreateInfo : public PipelineCreateInfo<GraphicsPipeline>
{
public:
	DepthStencilStateCreateInfo depthStencil; //!< Depth and stencil buffer creation info
	ColorBlendStateCreateInfo colorBlend; //!< Color blending and attachments info
	ViewportStateCreateInfo viewport; //!< Viewport creation info
	RasterStateCreateInfo rasterizer; //!< Rasterizer configuration creation info
	PipelineVertexInputStateCreateInfo vertexInput; //!< Vertex Input creation info
	InputAssemblerStateCreateInfo inputAssembler; //!< Input Assembler creation info
	ShaderStageCreateInfo vertexShader; //!< Vertex shader information
	ShaderStageCreateInfo fragmentShader; //!< Fragment shader information
	ShaderStageCreateInfo geometryShader; //!< Geometry shader information
	TesselationStageCreateInfo tesselationStates; //!< Tesselation Control and evaluation shader information
	MultiSampleStateCreateInfo multiSample; //!< Multisampling information
	DynamicStatesCreateInfo dynamicStates; //!< Dynamic state Information
	RenderPass renderPass; //!< The Renderpass
	uint32_t subpass; //!< The subpass index

	GraphicsPipelineCreateInfo() : PipelineCreateInfo(), subpass(0) {}
};
namespace impl {

/// <summary>A Graphics Pipeline is a PVRVk adapter to a Vulkan Pipeline to a pipeline created for
/// VK_PIPELINE_BINDING_POINT_COMPUTE, and as such only supports the part of Vulkan that is
/// supported for Graphics pipelines.</summary>
class GraphicsPipeline_ : public Pipeline<GraphicsPipeline, GraphicsPipelineCreateInfo>
{
public:
	DECLARE_NO_COPY_SEMANTICS(GraphicsPipeline_)

private:
	template<typename>
	friend struct ::pvrvk::RefCountEntryIntrusive;
	friend class ::pvrvk::impl::Device_;

	GraphicsPipeline_(DeviceWeakPtr device, VkPipeline vkPipeline, const GraphicsPipelineCreateInfo& desc) : Pipeline(device, vkPipeline, desc) {}
};
} // namespace impl
} // namespace pvrvk
