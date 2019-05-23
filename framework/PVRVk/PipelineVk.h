/*!
\brief The PVRVk GraphicsPipeline. This is an interface for a Vulkan VkPipeline
that was built for the VK_BINDING_POINT_GRAPHICS, separating it from the corresponding Compute pipeline
\file PVRVk/GraphicsPipelineVk.h
\author PowerVR by Imagination, Developer Technology Team
\copyright Copyright (c) Imagination Technologies Limited.
*/
#pragma once
#include "PVRVk/PipelineConfigVk.h"
#include "PVRVk/HeadersVk.h"
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
template<class PVRVkPipeline>
struct PipelineCreateInfo
{
public:
	PipelineLayout pipelineLayout; //!< The pipeline layout
	PVRVkPipeline basePipeline; //!< The parent pipeline, in case of pipeline derivative.
	int32_t basePipelineIndex; //!< The index of the base pipeline
	pvrvk::PipelineCreateFlags flags; //!< Any flags used for pipeline creation

	virtual ~PipelineCreateInfo() {}

protected:
	PipelineCreateInfo() : basePipelineIndex(-1), flags(pvrvk::PipelineCreateFlags(0)) {}
};

namespace impl {

/// <summary>A Graphics Pipeline is a PVRVk adapter to a Vulkan Pipeline to a pipeline created for
/// VK_PIPELINE_BINDING_POINT_COMPUTE, and as such only supports the part of Vulkan that is
/// supported for Graphics pipelines.</summary>
template<class PVRVkPipeline, class PVRVkPipelineCreateInfo>
class Pipeline : public DeviceObjectHandle<VkPipeline>, public DeviceObjectDebugMarker<Pipeline<PVRVkPipeline, PVRVkPipelineCreateInfo> >
{
public:
	DECLARE_NO_COPY_SEMANTICS(Pipeline)

	/// <summary>Return pipeline layout.</summary>
	/// <returns>const PipelineLayout&</returns>
	const PipelineLayout& getPipelineLayout() const
	{
		return _createInfo.pipelineLayout;
	}

	/// <summary>return pipeline create param used to create the child pipeline</summary>
	/// <returns>const PipelineCreateInfo<PVRVkPipeline>&</returns>
	const PVRVkPipelineCreateInfo& getCreateInfo() const
	{
		return _createInfo;
	}

protected:
	template<typename>
	friend struct ::pvrvk::RefCountEntryIntrusive;
	friend class ::pvrvk::impl::Device_;

	~Pipeline()
	{
		if (_vkHandle != VK_NULL_HANDLE || _pipeCache != VK_NULL_HANDLE)
		{
			if (_device.isValid())
			{
				if (_vkHandle != VK_NULL_HANDLE)
				{
					_device->getVkBindings().vkDestroyPipeline(_device->getVkHandle(), _vkHandle, nullptr);
					_vkHandle = VK_NULL_HANDLE;
				}
				if (_pipeCache != VK_NULL_HANDLE)
				{
					_device->getVkBindings().vkDestroyPipelineCache(_device->getVkHandle(), _pipeCache, nullptr);
					_pipeCache = VK_NULL_HANDLE;
				}
				_device.reset();
			}
			else
			{
				reportDestroyedAfterDevice("Pipeline");
			}
		}
		_parent.reset();
	}

	/// <summary>Constructor for a PVRVk pipeline object. This constructor shouldn't be called directly and should instead be called indirectly via the creation of Graphics and
	/// Compute pipelines.</summary>
	/// <param name="device">The device from which this PVRVk pipeline will be created from.</param>
	/// <param name="vkPipeline">The Vulkan pipeline object itself.</param>
	/// <param name="desc">The pipeline creation information.</param>
	Pipeline(DeviceWeakPtr device, VkPipeline vkPipeline, const PVRVkPipelineCreateInfo& desc)
		: DeviceObjectHandle<VkPipeline>(device), DeviceObjectDebugMarker<Pipeline<PVRVkPipeline, PVRVkPipelineCreateInfo> >(pvrvk::DebugReportObjectTypeEXT::e_PIPELINE_EXT),
		  _pipeCache(VK_NULL_HANDLE)
	{
		_vkHandle = vkPipeline;
		_createInfo = desc;
	}

	/// <summary>Pipeline creation information.</summary>
	PVRVkPipelineCreateInfo _createInfo;

	/// <summary>Pipeline cache object providing potential optimisations when create subsequent pipelines.</summary>
	VkPipelineCache _pipeCache;

	/// <summary>A parent pipeline to using when creating this pipeline. Its expected that the parent and this pipeline will have much commonality. Making use of a common parent
	/// pipeline means it may be more efficient to create this pipeline. The parent pipeline must have been created using the VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT.</summary>
	PVRVkPipeline _parent;
};
} // namespace impl
} // namespace pvrvk
