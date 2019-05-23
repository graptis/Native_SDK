/*!
\brief The Physical Device class
\file PVRVk/PhysicalDeviceVk.h
\author PowerVR by Imagination, Developer Technology Team
\copyright Copyright (c) Imagination Technologies Limited.
*/

#pragma once
#include "PVRVk/ForwardDecObjectsVk.h"
#include "PVRVk/InstanceVk.h"
#include "PVRVk/ObjectHandleVk.h"
#include "PVRVk/TypesVk.h"
#include "PVRVk/DebugMarkerVk.h"

namespace pvrvk {
namespace impl {

/// <summary>The representation of an entire actual, physical GPU device (as opposed to Device,
/// which is a local, logical part of it). A Physical device is "determined", or "found", or
/// "enumerated", (while a logical device is "created"). You can use the physical device to
/// create logical Devices, determine Extensions etc. See Vulkan spec. </summary>
class PhysicalDevice_ : public InstanceObjectHandle<VkPhysicalDevice>, public EmbeddedRefCount<PhysicalDevice_>
{
public:
	DECLARE_NO_COPY_SEMANTICS(PhysicalDevice_)

	/// <summary>Get device properties</summary>
	/// <returns>Returns PhysicalDeviceProperties</returns>
	const PhysicalDeviceProperties& getProperties() const
	{
		return _deviceProperties;
	}

	/// <summary>Get the list of displays (const)</summary>
	/// <returns>const std::vector<Display>&</returns>
	const std::vector<Display>& getDisplays() const;

	/// <summary>Get Display (const)</summary>
	/// <param name="id">Display id</param>
	/// <returns>const Display&</returns>
	const Display& getDisplay(uint32_t id) const;

	/// <summary>Get Display (const)</summary>
	/// <param name="id">Display id</param>
	/// <returns>const Display&</returns>
	Display getDisplay(uint32_t id);

	/// <summary>Get the number of displays</summary>
	/// <returns>Returns the number of supported displays</returns>
	uint32_t getNumDisplays() const
	{
		return static_cast<uint32_t>(_displays.size());
	}

	/// <summary>Get supported memory properties (const)</summary>
	/// <returns>PhysicalDeviceMemoryProperties</returns>
	const PhysicalDeviceMemoryProperties& getMemoryProperties() const
	{
		return _deviceMemoryProperties;
	}

	/// <summary>Determine whether the specified surface supports presentation</summary>
	/// <param name="queueFamilyIndex">The queue family to check for WSI support</param>
	/// <param name="surface">The surface to check for WSI support for the specified queue family index</param>
	/// <returns>True if the specified queue family supports Window System Integration (WSI) with the specified surface</returns>
	bool getSurfaceSupport(uint32_t queueFamilyIndex, Surface surface) const;

	/// <summary>Get format properties</summary>
	/// <param name="format">Format</param>
	/// <returns>FormatProperties</returns>
	FormatProperties getFormatProperties(pvrvk::Format format);

	/// <summary>Get surface capabilities for a surface created using this physical device</summary>
	/// <param name="surface">The surface to retrieve capabilities for</param>
	/// <returns>SurfaceCapabilities</returns>
	SurfaceCapabilitiesKHR getSurfaceCapabilities(const Surface& surface) const;

	/// <summary>Get This physical device features</summary>
	/// <returns>PhysicalDeviceFeatures</returns>
	const PhysicalDeviceFeatures& getFeatures() const
	{
		return _deviceFeatures;
	}

	/// <summary>Create a display mode</summary>
	/// <param name="display">The display from which to create a display mode</param>
	/// <param name="displayModeCreateInfo">Display Mode create info</param>
	/// <returns>The created DisplayMode</returns>
	DisplayMode createDisplayMode(pvrvk::Display& display, const pvrvk::DisplayModeCreateInfo& displayModeCreateInfo);

	/// <summary> create gpu device. </summary>
	/// <param name="deviceCreateInfo">Device create info</param>
	/// <returns>Device</returns>
	Device createDevice(const DeviceCreateInfo& deviceCreateInfo);

	/// <summary>Get the list of queue family properties</summary>
	/// <returns>A list of QueueFamilyProperties for the physical device</returns>
	const std::vector<QueueFamilyProperties>& getQueueFamilyProperties() const
	{
		return _queueFamilyPropeties;
	}

	/// <summary>Enumerate device extensions properties</summary>
	/// <returns>std::vector<pvrvk::ExtensionProperties></returns>
	std::vector<ExtensionProperties>& enumerateDeviceExtensionsProperties();

	/// <summary>Returns the image format properties for a given set of image creation parameters</summary>
	/// <param name="format">The image format to get sparse image properties for.</param>
	/// <param name="imageType">The image type to get sparse image properties for.</param>
	/// <param name="tiling">The image tiling mode.</param>
	/// <param name="usage">The image usage flags.</param>
	/// <param name="flags">The image creation flags.</param>
	/// <returns>An ImageFormatProperties structure</returns>
	const ImageFormatProperties getImageFormatProperties(
		pvrvk::Format format, pvrvk::ImageType imageType, pvrvk::ImageTiling tiling, pvrvk::ImageUsageFlags usage, pvrvk::ImageCreateFlags flags);

	/// <summary>Returns the sparse image format properties for a given set of image creation parameters</summary>
	/// <param name="format">The image format to get sparse image properties for.</param>
	/// <param name="imageType">The image type to get sparse image properties for.</param>
	/// <param name="sampleCount">The image sample count.</param>
	/// <param name="usage">The image usage flags.</param>
	/// <param name="tiling">The image tiling mode.</param>
	/// <returns>A list of SparseImageFormatProperties structures</returns>
	const std::vector<SparseImageFormatProperties> getSparseImageFormatProperties(
		pvrvk::Format format, pvrvk::ImageType imageType, pvrvk::SampleCountFlags sampleCount, pvrvk::ImageUsageFlags usage, pvrvk::ImageTiling tiling);

	/// <summary>Attempts to find the index for a suitable memory type supporting the memory type bits required from the set of memory type bits supported.</summary>
	/// <param name="allowedMemoryTypeBits">The memory type bits allowed. The required memory type chosen must be one of those allowed.</param>
	/// <param name="requiredMemoryProperties">The memory type property flags required.</param>
	/// <param name="usedMemoryProperties">The memory type property flags found which support the memory type bits and memory property flags. Note that the use memory property
	/// flags may be different to those that were provided as required although they will be a superset of those required.</param> <returns>The memory type index found supporting
	/// the specified MemoryPropertyFlags.</returns>
	uint32_t getMemoryTypeIndex(uint32_t allowedMemoryTypeBits, pvrvk::MemoryPropertyFlags requiredMemoryProperties, pvrvk::MemoryPropertyFlags& usedMemoryProperties) const;

	/// <summary>Returns the number of supported display planes.</summary>
	/// <returns>The number of supported display planes.</returns>
	uint32_t getNumDisplayPlanes()
	{
		return static_cast<uint32_t>(_displayPlaneProperties.size());
	}

	/// <summary>Attempts to find the display plane properties for a given display.</summary>
	/// <param name="displayPlaneIndex">The display plane index to get display plane properties for.</param>
	/// <param name="currentStackIndex">The display to find display plane properties for.</param>
	/// <returns>The DisplayPlanePropertiesKHR for the given display else VK_NULL_HANDLE if the display does not currently have an associated display plane.</returns>
	Display getDisplayPlaneProperties(uint32_t displayPlaneIndex, uint32_t& currentStackIndex);

	/// <summary>Attempts to find the supported displays for a given display plane.</summary>
	/// <param name="planeIndex">The display plane index to check for supported displays.</param>
	/// <returns>The list of supported displays for the given plane.</returns>
	std::vector<Display> getDisplayPlaneSupportedDisplays(uint32_t planeIndex);

	/// <summary>Attempts to find the supported displays for a given display plane.</summary>
	/// <param name="mode">The display mode object to get display plane capabilities for.</param>
	/// <param name="planeIndex">The display plane index to check for supported displays.</param>
	/// <returns>The list of supported displays for the given plane.</returns>
	DisplayPlaneCapabilitiesKHR getDisplayPlaneCapabilities(DisplayMode& mode, uint32_t planeIndex);

private:
	friend class pvrvk::impl::Instance_;
	friend class pvrvk::EmbeddedRefCount<PhysicalDevice_>;
	PhysicalDevice_(InstanceWeakPtr instance, const VkPhysicalDevice& vkPhysicalDevice);

	static PhysicalDevice createNew(InstanceWeakPtr instance, const VkPhysicalDevice& vkPhysicalDevice)
	{
		return EmbeddedRefCount<PhysicalDevice_>::createNew(instance, vkPhysicalDevice);
	}

	void updateDisplayPlaneProperties();

	void destroyObject()
	{
		if (getVkHandle() != VK_NULL_HANDLE)
		{
			if (_instance.isValid() || _instance.isInDestructor())
			{
				_vkHandle = VK_NULL_HANDLE;
				_instance.reset();
			}
			else
			{
				Log(LogLevel::Warning, "Attempted to destroy object of type [%s] after its corresponding instance", "Physical Device");
			}
		}
		_supportedFormats.clear();
		_deviceExtensions.clear();
	}

	std::vector<QueueFamilyProperties> _queueFamilyPropeties;
	PhysicalDeviceProperties _deviceProperties;
	std::vector<Display> _displays;
	std::vector<DisplayPlanePropertiesKHR> _displayPlaneProperties;
	PhysicalDeviceMemoryProperties _deviceMemoryProperties;
	PhysicalDeviceFeatures _deviceFeatures;
	std::vector<FormatProperties> _supportedFormats;
	std::vector<ExtensionProperties> _deviceExtensions;
	uint32_t _graphicsQueueIndex;
	uint32_t _universalQueueFamilyId;
};
} // namespace impl
} // namespace pvrvk
