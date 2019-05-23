/*!
\brief Implementation for a number of functions defined in the HelperVk header file.
\file PVRUtils/Vulkan/HelperVk.cpp
\author PowerVR by Imagination, Developer Technology Team
\copyright Copyright (c) Imagination Technologies Limited.
*/

//!\cond NO_DOXYGEN
#include "HelperVk.h"
#include "PVRCore/Texture/PVRTDecompress.h"
#include "PVRCore/TGAWriter.h"
#include "PVRVk/ImageVk.h"
#include "PVRVk/CommandPoolVk.h"
#include "PVRVk/QueueVk.h"
#include "PVRVk/HeadersVk.h"
#include "PVRVk/SwapchainVk.h"
#include "PVRVk/MemoryBarrierVk.h"
#include "PVRUtils/Vulkan/MemoryAllocator.h"
#include "PVRVk/MemoryBarrierVk.h"
#include "PVRVk/DisplayVk.h"
#include "PVRVk/DisplayModeVk.h"
#include "pvr_openlib.h"

namespace pvr {
namespace utils {
pvrvk::Buffer createBuffer(pvrvk::Device device, VkDeviceSize size, pvrvk::BufferUsageFlags bufferUsage, pvrvk::MemoryPropertyFlags requiredMemoryFlags,
	pvrvk::MemoryPropertyFlags optimalMemoryFlags, vma::Allocator* bufferAllocator, vma::AllocationCreateFlags vmaAllocationCreateFlags, pvrvk::BufferCreateFlags bufferCreateFlags,
	pvrvk::SharingMode sharingMode, const uint32_t* queueFamilyIndices, uint32_t numQueueFamilyIndices)
{
	// create the PVRVk Buffer
	pvrvk::BufferCreateInfo createInfo = pvrvk::BufferCreateInfo(size, bufferUsage, pvrvk::BufferCreateFlags::e_NONE, sharingMode, queueFamilyIndices, numQueueFamilyIndices);
	pvrvk::Buffer buffer = device->createBuffer(createInfo);

	// if the required memory flags is pvrvk::MemoryPropertyFlags::e_NONE then no backing will be provided for the buffer
	if (requiredMemoryFlags != pvrvk::MemoryPropertyFlags::e_NONE)
	{
		// use the allocator
		if (bufferAllocator)
		{
			vma::AllocationCreateInfo allocationInfo;
			allocationInfo.usage = vma::MemoryUsage::e_UNKNOWN;
			allocationInfo.requiredFlags = requiredMemoryFlags;
			allocationInfo.preferredFlags = optimalMemoryFlags | requiredMemoryFlags;
			allocationInfo.flags = vmaAllocationCreateFlags;
			allocationInfo.memoryTypeBits = buffer->getMemoryRequirement().getMemoryTypeBits();
			vma::Allocation allocation;
			allocation = (*bufferAllocator)->allocateMemoryForBuffer(buffer, allocationInfo);
			buffer->bindMemory(pvrvk::DeviceMemory(allocation), allocation->getOffset());
		}
		else
		{
			// get the buffer memory requirements, memory type index and memory property flags required for backing the PVRVk buffer
			const pvrvk::MemoryRequirements& memoryRequirements = buffer->getMemoryRequirement();
			uint32_t memoryTypeIndex;
			pvrvk::MemoryPropertyFlags memoryPropertyFlags;
			getMemoryTypeIndex(device->getPhysicalDevice(), memoryRequirements.getMemoryTypeBits(), requiredMemoryFlags, optimalMemoryFlags, memoryTypeIndex, memoryPropertyFlags);

			// allocate the buffer memory using the retrieved memory type index and memory property flags
			pvrvk::DeviceMemory deviceMemory = device->allocateMemory(pvrvk::MemoryAllocationInfo(buffer->getMemoryRequirement().getSize(), memoryTypeIndex));

			// attach the memory to the buffer
			buffer->bindMemory(deviceMemory, 0);
		}
	}
	return buffer;
}

pvrvk::Image createImage(pvrvk::Device device, pvrvk::ImageType imageType, pvrvk::Format format, const pvrvk::Extent3D& dimension, pvrvk::ImageUsageFlags usage,
	pvrvk::ImageCreateFlags flags, const pvrvk::ImageLayersSize& layerSize, pvrvk::SampleCountFlags samples, pvrvk::MemoryPropertyFlags requiredMemoryFlags,
	pvrvk::MemoryPropertyFlags optimalMemoryFlags, vma::Allocator* imageAllocator, vma::AllocationCreateFlags vmaAllocationCreateFlags, pvrvk::SharingMode sharingMode,
	pvrvk::ImageTiling tiling, pvrvk::ImageLayout initialLayout, const uint32_t* queueFamilyIndices, uint32_t numQueueFamilyIndices)
{
	// create the PVRVk Image
	pvrvk::ImageCreateInfo createInfo = pvrvk::ImageCreateInfo(imageType, format, dimension, usage, flags, layerSize.getNumMipLevels(), layerSize.getNumArrayLevels(), samples,
		tiling, sharingMode, initialLayout, queueFamilyIndices, numQueueFamilyIndices);
	pvrvk::Image image = device->createImage(createInfo);

	// if the required memory flags is pvrvk::MemoryPropertyFlags::e_NONE then no backing will be provided for the image
	if (requiredMemoryFlags != pvrvk::MemoryPropertyFlags::e_NONE)
	{
		// if no flags are provided for the optimal flags then just reuse the required set of memory property flags to optimise the getMemoryTypeIndex
		if (optimalMemoryFlags == pvrvk::MemoryPropertyFlags::e_NONE)
		{
			optimalMemoryFlags = requiredMemoryFlags;
		}

		// Create a memory block if it is non sparse and a valid memory propery flag.
		if ((flags & (pvrvk::ImageCreateFlags::e_SPARSE_ALIASED_BIT | pvrvk::ImageCreateFlags::e_SPARSE_BINDING_BIT | pvrvk::ImageCreateFlags::e_SPARSE_RESIDENCY_BIT)) == 0 &&
			(requiredMemoryFlags != pvrvk::MemoryPropertyFlags(0)))
		// If it's not sparse, create memory backing
		{
			if (imageAllocator != nullptr)
			{
				vma::AllocationCreateInfo allocInfo = {};
				allocInfo.memoryTypeBits = image->getMemoryRequirement().getMemoryTypeBits();
				allocInfo.requiredFlags = requiredMemoryFlags;
				allocInfo.preferredFlags = requiredMemoryFlags | optimalMemoryFlags;
				allocInfo.flags = vmaAllocationCreateFlags;
				vma::Allocation allocation = (*imageAllocator)->allocateMemoryForImage(image, allocInfo);
				image->bindMemoryNonSparse(allocation, allocation->getOffset());
			}
			else
			{
				// get the image memory requirements, memory type index and memory property flags required for backing the PVRVk image
				const pvrvk::MemoryRequirements& memoryRequirements = image->getMemoryRequirement();
				uint32_t memoryTypeIndex;
				pvrvk::MemoryPropertyFlags memoryPropertyFlags;
				getMemoryTypeIndex(device->getPhysicalDevice(), memoryRequirements.getMemoryTypeBits(), requiredMemoryFlags, optimalMemoryFlags, memoryTypeIndex, memoryPropertyFlags);

				// allocate the image memory using the retrieved memory type index and memory property flags
				pvrvk::DeviceMemory memBlock = device->allocateMemory(pvrvk::MemoryAllocationInfo(memoryRequirements.getSize(), memoryTypeIndex));

				// attach the memory to the image
				image->bindMemoryNonSparse(memBlock, 0);
			}
		}
	}
	return image;
}

using namespace pvrvk;
pvrvk::ImageAspectFlags inferAspectFromFormat(pvrvk::Format format)
{
	pvrvk::ImageAspectFlags imageAspect = pvrvk::ImageAspectFlags::e_COLOR_BIT;

	if (format >= pvrvk::Format::e_D16_UNORM && format <= pvrvk::Format::e_D32_SFLOAT_S8_UINT)
	{
		const pvrvk::ImageAspectFlags aspects[] = {
			pvrvk::ImageAspectFlags::e_DEPTH_BIT | pvrvk::ImageAspectFlags::e_STENCIL_BIT, //  pvrvk::Format::e_D32_SFLOAT_S8_UINT
			pvrvk::ImageAspectFlags::e_DEPTH_BIT | pvrvk::ImageAspectFlags::e_STENCIL_BIT, //  pvrvk::Format::e_D24_UNORM_S8_UINT
			pvrvk::ImageAspectFlags::e_DEPTH_BIT | pvrvk::ImageAspectFlags::e_STENCIL_BIT, //  pvrvk::Format::e_D16_UNORM_S8_UINT
			pvrvk::ImageAspectFlags::e_STENCIL_BIT, //  pvrvk::Format::e_S8_UINT
			pvrvk::ImageAspectFlags::e_DEPTH_BIT, //  pvrvk::Format::e_D32_SFLOAT
			pvrvk::ImageAspectFlags::e_DEPTH_BIT, //  pvrvk::Format::e_X8_D24_UNORM_PACK32
			pvrvk::ImageAspectFlags::e_DEPTH_BIT, //  pvrvk::Format::e_D16_UNORM
		};
		// (Depthstenil format end) - format
		imageAspect = aspects[(int)(pvrvk::Format::e_D32_SFLOAT_S8_UINT) - (int)(format)];
	}
	return imageAspect;
}

void getColorBits(pvrvk::Format format, uint32_t& redBits, uint32_t& greenBits, uint32_t& blueBits, uint32_t& alphaBits)
{
	switch (format)
	{
	case pvrvk::Format::e_R8G8B8A8_SRGB:
	case pvrvk::Format::e_R8G8B8A8_UNORM:
	case pvrvk::Format::e_R8G8B8A8_SNORM:
	case pvrvk::Format::e_B8G8R8A8_UNORM:
	case pvrvk::Format::e_B8G8R8A8_SRGB:
		redBits = 8;
		greenBits = 8;
		blueBits = 8;
		alphaBits = 8;
		break;
	case pvrvk::Format::e_B8G8R8_SRGB:
	case pvrvk::Format::e_B8G8R8_UNORM:
	case pvrvk::Format::e_B8G8R8_SNORM:
	case pvrvk::Format::e_R8G8B8_SRGB:
	case pvrvk::Format::e_R8G8B8_UNORM:
	case pvrvk::Format::e_R8G8B8_SNORM:
		redBits = 8;
		greenBits = 8;
		blueBits = 8;
		alphaBits = 0;
		break;
	case pvrvk::Format::e_R5G6B5_UNORM_PACK16:
		redBits = 5;
		greenBits = 6;
		blueBits = 5;
		alphaBits = 0;
		break;
	default:
		assertion(0, "UnSupported pvrvk::Format");
	}
}

void getDepthStencilBits(pvrvk::Format format, uint32_t& depthBits, uint32_t& stencilBits)
{
	switch (format)
	{
	case pvrvk::Format::e_D16_UNORM:
		depthBits = 16;
		stencilBits = 0;
		break;
	case pvrvk::Format::e_D16_UNORM_S8_UINT:
		depthBits = 16;
		stencilBits = 8;
		break;
	case pvrvk::Format::e_D24_UNORM_S8_UINT:
		depthBits = 24;
		stencilBits = 8;
		break;
	case pvrvk::Format::e_D32_SFLOAT:
		depthBits = 32;
		stencilBits = 0;
		break;
	case pvrvk::Format::e_D32_SFLOAT_S8_UINT:
		depthBits = 32;
		stencilBits = 8;
		break;
	case pvrvk::Format::e_X8_D24_UNORM_PACK32:
		depthBits = 24;
		stencilBits = 0;
		break;
	case pvrvk::Format::e_S8_UINT:
		depthBits = 0;
		stencilBits = 8;
		break;
	default:
		assertion(0, "UnSupported pvrvk::Format");
	}
}

ImageView uploadImageAndViewSubmit(pvrvk::Device& device, const Texture& texture, bool allowDecompress, pvrvk::CommandPool& cmdPool, pvrvk::Queue& queue,
	pvrvk::ImageUsageFlags usageFlags, pvrvk::ImageLayout finalLayout, vma::Allocator* stagingBufferAllocator, vma::Allocator* imageAllocator,
	vma::AllocationCreateFlags imageAllocationCreateFlags)
{
	CommandBuffer cmdBuffer = cmdPool->allocateCommandBuffer();
	cmdBuffer->begin();
	cmdBuffer->debugMarkerBeginEXT("PVRUtilsVk::uploadImageAndSubmit");
	ImageView result = uploadImageAndView(device, texture, allowDecompress, cmdBuffer, usageFlags, finalLayout, stagingBufferAllocator, imageAllocator, imageAllocationCreateFlags);
	cmdBuffer->debugMarkerEndEXT();
	cmdBuffer->end();

	SubmitInfo submitInfo;
	submitInfo.commandBuffers = &cmdBuffer;
	submitInfo.numCommandBuffers = 1;
	Fence fence = device->createFence();
	queue->submit(&submitInfo, 1, fence);
	fence->wait();

	return result;
}
namespace {
void decompressPvrtc(const Texture& texture, Texture& cDecompressedTexture)
{
	// Set up the new texture and header.
	TextureHeader cDecompressedHeader(texture);
	// robin: not sure what should happen here. The PVRTGENPIXELID4 macro is used in the old SDK.
	cDecompressedHeader.setPixelFormat(GeneratePixelType4<'r', 'g', 'b', 'a', 8, 8, 8, 8>::ID);

	cDecompressedHeader.setChannelType(VariableType::UnsignedByteNorm);
	cDecompressedTexture = Texture(cDecompressedHeader);

	// Do decompression, one surface at a time.
	for (uint32_t uiMipMapLevel = 0; uiMipMapLevel < texture.getNumMipMapLevels(); ++uiMipMapLevel)
	{
		for (uint32_t uiArray = 0; uiArray < texture.getNumArrayMembers(); ++uiArray)
		{
			for (uint32_t uiFace = 0; uiFace < texture.getNumFaces(); ++uiFace)
			{
				PVRTDecompressPVRTC(texture.getDataPointer(uiMipMapLevel, uiArray, uiFace), (texture.getBitsPerPixel() == 2 ? 1 : 0), texture.getWidth(uiMipMapLevel),
					texture.getHeight(uiMipMapLevel), cDecompressedTexture.getDataPointer(uiMipMapLevel, uiArray, uiFace));
			}
		}
	}
}

inline pvrvk::Format getDepthStencilFormat(const DisplayAttributes& displayAttribs)
{
	uint32_t depthBpp = displayAttribs.depthBPP;
	uint32_t stencilBpp = displayAttribs.stencilBPP;

	pvrvk::Format dsFormat = pvrvk::Format::e_UNDEFINED;

	if (stencilBpp)
	{
		switch (depthBpp)
		{
		case 0:
			dsFormat = pvrvk::Format::e_S8_UINT;
			break;
		case 16:
			dsFormat = pvrvk::Format::e_D16_UNORM_S8_UINT;
			break;
		case 24:
			dsFormat = pvrvk::Format::e_D24_UNORM_S8_UINT;
			break;
		case 32:
			dsFormat = pvrvk::Format::e_D32_SFLOAT_S8_UINT;
			break;
		default:
			assertion("Unsupported Depth Stencil pvrvk::Format");
		}
	}
	else
	{
		switch (depthBpp)
		{
		case 16:
			dsFormat = pvrvk::Format::e_D16_UNORM;
			break;
		case 24:
			dsFormat = pvrvk::Format::e_X8_D24_UNORM_PACK32;
			break;
		case 32:
			dsFormat = pvrvk::Format::e_D32_SFLOAT;
			break;
		default:
			assertion("Unsupported Depth Stencil pvrvk::Format");
		}
	}
	return dsFormat;
}

const inline std::string depthStencilFormatToString(pvrvk::Format format)
{
	const std::string preferredDepthStencilFormat[] = {
		"pvrvk::Format::e_D16_UNORM",
		"pvrvk::Format::e_X8_D24_UNORM_PACK32",
		"pvrvk::Format::e_D32_SFLOAT",
		"pvrvk::Format::e_S8_UINT",
		"pvrvk::Format::e_D16_UNORM_S8_UINT",
		"pvrvk::Format::e_D24_UNORM_S8_UINT",
		"pvrvk::Format::e_D32_SFLOAT_S8_UINT",
	};
	return preferredDepthStencilFormat[(int)format - (int)pvrvk::Format::e_D16_UNORM];
}

Swapchain createSwapchainHelper(Device& device, const Surface& surface, pvr::DisplayAttributes& displayAttributes, const pvrvk::ImageUsageFlags& swapchainImageUsageFlags,
	pvrvk::Format* preferredColorFormats, uint32_t numPreferredColorFormats)
{
	Log(LogLevel::Information, "Creating Vulkan Swapchain using pvr::DisplayAttributes");

	SurfaceCapabilitiesKHR surfaceCapabilities = device->getPhysicalDevice()->getSurfaceCapabilities(surface);

	Log(LogLevel::Information, "Queried Surface Capabilities:");
	Log(LogLevel::Information, "\tMinimum Image count: %u", surfaceCapabilities.getMinImageCount());
	Log(LogLevel::Information, "\tMaximum Image count: %u", surfaceCapabilities.getMaxImageCount());
	Log(LogLevel::Information, "\tMaximum Image Array Layers: %u", surfaceCapabilities.getMaxImageArrayLayers());
	Log(LogLevel::Information, "\tImage size (now): %ux%u", surfaceCapabilities.getCurrentExtent().getWidth(), surfaceCapabilities.getCurrentExtent().getHeight());
	Log(LogLevel::Information, "\tMinimum Image extent: %dx%d", surfaceCapabilities.getMinImageExtent().getWidth(), surfaceCapabilities.getMinImageExtent().getHeight());
	Log(LogLevel::Information, "\tMaximum Image extent: %dx%d", surfaceCapabilities.getMaxImageExtent().getWidth(), surfaceCapabilities.getMaxImageExtent().getHeight());
	Log(LogLevel::Information, "\tSupported Usage Flags: %s", pvrvk::to_string(surfaceCapabilities.getSupportedUsageFlags()).c_str());
	Log(LogLevel::Information, "\tCurrent transform: %s", pvrvk::to_string(surfaceCapabilities.getCurrentTransform()).c_str());
	Log(LogLevel::Information, "\tSupported transforms: %s", pvrvk::to_string(surfaceCapabilities.getSupportedTransforms()).c_str());
	Log(LogLevel::Information, "\tComposite Alpha Flags: %s", pvrvk::to_string(surfaceCapabilities.getSupportedCompositeAlpha()).c_str());

	uint32_t usedWidth = surfaceCapabilities.getCurrentExtent().getWidth();
	uint32_t usedHeight = surfaceCapabilities.getCurrentExtent().getHeight();
#if !defined(ANDROID)
	usedWidth =
		std::max<uint32_t>(surfaceCapabilities.getMinImageExtent().getWidth(), std::min<uint32_t>(displayAttributes.width, surfaceCapabilities.getMaxImageExtent().getWidth()));

	usedHeight =
		std::max<uint32_t>(surfaceCapabilities.getMinImageExtent().getHeight(), std::min<uint32_t>(displayAttributes.height, surfaceCapabilities.getMaxImageExtent().getHeight()));
#endif
	// Log modifications made to the surface properties set via DisplayAttributes
	Log(LogLevel::Information, "Modified Surface Properties after inspecting DisplayAttributes:");

	displayAttributes.width = usedWidth;
	displayAttributes.height = usedHeight;

	Log(LogLevel::Information, "\tImage size to be used: %dx%d", displayAttributes.width, displayAttributes.height);

	uint32_t numFormats = 0;
	pvrvk::impl::vkThrowIfFailed(device->getPhysicalDevice()->getInstance()->getVkBindings().vkGetPhysicalDeviceSurfaceFormatsKHR(
									 device->getPhysicalDevice()->getVkHandle(), surface->getVkHandle(), &numFormats, NULL),
		"Unable to retrieve the physical device surface formats");

	std::vector<pvrvk::SurfaceFormatKHR> surfaceFormats(numFormats);
	pvrvk::impl::vkThrowIfFailed(device->getPhysicalDevice()->getInstance()->getVkBindings().vkGetPhysicalDeviceSurfaceFormatsKHR(
									 device->getPhysicalDevice()->getVkHandle(), surface->getVkHandle(), &numFormats, reinterpret_cast<VkSurfaceFormatKHR*>(surfaceFormats.data())),
		"Unable to retrieve the physical device surface formats");

	pvrvk::SurfaceFormatKHR imageFormat = surfaceFormats[0];

	pvrvk::Format frameworkPreferredColorFormats[7] = { pvrvk::Format::e_R8G8B8A8_UNORM, pvrvk::Format::e_R8G8B8A8_SRGB, pvrvk::Format::e_R8G8B8A8_SNORM,
		pvrvk::Format::e_B8G8R8_SNORM, pvrvk::Format::e_B8G8R8A8_UNORM, pvrvk::Format::e_B8G8R8A8_SRGB, pvrvk::Format::e_R5G6B5_UNORM_PACK16 };
	std::vector<pvrvk::Format> colorFormats;

	if (numPreferredColorFormats)
	{
		colorFormats.insert(colorFormats.begin(), &preferredColorFormats[0], &preferredColorFormats[numPreferredColorFormats]);
	}
	else
	{
		colorFormats.insert(colorFormats.begin(), &frameworkPreferredColorFormats[0], &frameworkPreferredColorFormats[7]);
	}

	uint32_t requestedRedBpp = displayAttributes.redBits;
	uint32_t requestedGreenBpp = displayAttributes.greenBits;
	uint32_t requestedBlueBpp = displayAttributes.blueBits;
	uint32_t requestedAlphaBpp = displayAttributes.alphaBits;
	bool foundFormat = false;
	for (unsigned int i = 0; i < colorFormats.size() && !foundFormat; ++i)
	{
		for (uint32_t f = 0; f < numFormats; ++f)
		{
			if (surfaceFormats[f].getFormat() == colorFormats[i])
			{
				if (displayAttributes.forceColorBPP)
				{
					uint32_t currentRedBpp, currentGreenBpp, currentBlueBpp, currentAlphaBpp = 0;
					getColorBits(surfaceFormats[f].getFormat(), currentRedBpp, currentGreenBpp, currentBlueBpp, currentAlphaBpp);
					if (currentRedBpp == requestedRedBpp && requestedGreenBpp == currentGreenBpp && requestedBlueBpp == currentBlueBpp && requestedAlphaBpp == currentAlphaBpp)
					{
						imageFormat = surfaceFormats[f];
						foundFormat = true;
						break;
					}
				}
				else
				{
					imageFormat = surfaceFormats[f];
					foundFormat = true;
					break;
				}
			}
		}
	}
	if (!foundFormat)
	{
		Log(LogLevel::Warning, "Swapchain - Unable to find supported preferred color format. Using format, color space: %s, %s", pvrvk::to_string(imageFormat.getFormat()).c_str(),
			pvrvk::to_string(imageFormat.getColorSpace()).c_str());
	}

	uint32_t numPresentModes;
	pvrvk::impl::vkThrowIfFailed(device->getPhysicalDevice()->getInstance()->getVkBindings().vkGetPhysicalDeviceSurfacePresentModesKHR(
									 device->getPhysicalDevice()->getVkHandle(), surface->getVkHandle(), &numPresentModes, NULL),
		"Failed to get the number of present modes");
	if (numPresentModes <= 0)
	{
		throw pvrvk::ErrorUnknown("0 presentation modes returned");
	}
	std::vector<pvrvk::PresentModeKHR> presentModes(numPresentModes);
	pvrvk::impl::vkThrowIfFailed(device->getPhysicalDevice()->getInstance()->getVkBindings().vkGetPhysicalDeviceSurfacePresentModesKHR(
									 device->getPhysicalDevice()->getVkHandle(), surface->getVkHandle(), &numPresentModes, reinterpret_cast<VkPresentModeKHR*>(&presentModes[0])),
		"Failed to get the present modes");

	// With VK_PRESENT_MODE_FIFO_KHR the presentation engine will wait for the next vblank (vertical blanking period) to update the current image. When using FIFO tearing cannot
	// occur. VK_PRESENT_MODE_FIFO_KHR is required to be supported.
	pvrvk::PresentModeKHR swapchainPresentMode = pvrvk::PresentModeKHR::e_FIFO_KHR;
	pvrvk::PresentModeKHR desiredSwapMode = pvrvk::PresentModeKHR::e_FIFO_KHR;

	// We make use of PVRShell for handling command line arguments for configuring vsync modes using the -vsync command line argument.
	switch (displayAttributes.vsyncMode)
	{
	case VsyncMode::Off:
		Log(LogLevel::Information, "Requested presentation mode: Immediate (VsyncMode::Off)");
		desiredSwapMode = pvrvk::PresentModeKHR::e_IMMEDIATE_KHR;
		break;
	case VsyncMode::Mailbox:
		Log(LogLevel::Information, "Requested presentation mode: Mailbox (VsyncMode::Mailbox)");
		desiredSwapMode = pvrvk::PresentModeKHR::e_MAILBOX_KHR;
		break;
	case VsyncMode::Relaxed:
		Log(LogLevel::Information, "Requested presentation mode: Relaxed (VsyncMode::Relaxed)");
		desiredSwapMode = pvrvk::PresentModeKHR::e_FIFO_RELAXED_KHR;
		break;
		// Default vsync mode
	case pvr::VsyncMode::On:
		Log(LogLevel::Information, "Requested presentation mode: Fifo (VsyncMode::On)");
		break;
	case pvr::VsyncMode::Half:
		Log(LogLevel::Information, "Unsupported presentation mode requested: Half. Defaulting to PresentModeKHR::e_FIFO_KHR");
	}
	std::string supported = "Supported presentation modes: [";
	for (size_t i = 0; i < numPresentModes; i++)
	{
		supported += to_string(presentModes[i]) + ((i + 1 != numPresentModes) ? " " : "]");
	}
	Log(LogLevel::Information, supported.c_str());
	for (size_t i = 0; i < numPresentModes; i++)
	{
		pvrvk::PresentModeKHR currentPresentMode = presentModes[i];

		// Primary matches : Check for a precise match between the desired presentation mode and the presentation modes supported.
		if (currentPresentMode == desiredSwapMode)
		{
			swapchainPresentMode = desiredSwapMode;
			break;
		}
		// Secondary matches : Immediate and Mailbox are better fits for each other than FIFO, so set them as secondaries
		// If the user asked for Mailbox, and we found Immediate, set it (in case Mailbox is not found) and keep looking
		if ((desiredSwapMode == pvrvk::PresentModeKHR::e_MAILBOX_KHR) && (currentPresentMode == pvrvk::PresentModeKHR::e_IMMEDIATE_KHR))
		{
			swapchainPresentMode = pvrvk::PresentModeKHR::e_IMMEDIATE_KHR;
		}
		// ... And vice versa: If the user asked for Immediate, and we found Mailbox, set it (in case Immediate is not found) and keep looking
		if ((desiredSwapMode == pvrvk::PresentModeKHR::e_IMMEDIATE_KHR) && (currentPresentMode == pvrvk::PresentModeKHR::e_MAILBOX_KHR))
		{
			swapchainPresentMode = pvrvk::PresentModeKHR::e_MAILBOX_KHR;
		}
	}
	switch (swapchainPresentMode)
	{
	case pvrvk::PresentModeKHR::e_IMMEDIATE_KHR:
		Log(LogLevel::Information, "Presentation mode: Immediate (Vsync OFF)");
		break;
	case pvrvk::PresentModeKHR::e_MAILBOX_KHR:
		Log(LogLevel::Information, "Presentation mode: Mailbox (Triple-buffering)");
		break;
	case pvrvk::PresentModeKHR::e_FIFO_KHR:
		Log(LogLevel::Information, "Presentation mode: FIFO (Vsync ON)");
		break;
	case pvrvk::PresentModeKHR::e_FIFO_RELAXED_KHR:
		Log(LogLevel::Information, "Presentation mode: Relaxed FIFO (Relaxed Vsync)");
		break;
	default:
		assertion(false, "Unrecognised presentation mode");
		break;
	}

	// Set the swapchain length appropriately based on the choice of presentation mode.
	if (!displayAttributes.swapLength)
	{
		switch (swapchainPresentMode)
		{
		case pvrvk::PresentModeKHR::e_MAILBOX_KHR:
			displayAttributes.swapLength = 3;
			break;
		default:
			displayAttributes.swapLength = 2;
			break;
		}
	}

	// Check for a supported composite alpha value in a predefined order
	pvrvk::CompositeAlphaFlagsKHR supportedCompositeAlphaFlags = pvrvk::CompositeAlphaFlagsKHR::e_NONE;
	if ((surfaceCapabilities.getSupportedCompositeAlpha() & pvrvk::CompositeAlphaFlagsKHR::e_OPAQUE_BIT_KHR) != 0)
	{
		supportedCompositeAlphaFlags = pvrvk::CompositeAlphaFlagsKHR::e_OPAQUE_BIT_KHR;
	}
	else if ((surfaceCapabilities.getSupportedCompositeAlpha() & pvrvk::CompositeAlphaFlagsKHR::e_INHERIT_BIT_KHR) != 0)
	{
		supportedCompositeAlphaFlags = pvrvk::CompositeAlphaFlagsKHR::e_INHERIT_BIT_KHR;
	}

	SwapchainCreateInfo createInfo;
	createInfo.clipped = true;
	createInfo.compositeAlpha = supportedCompositeAlphaFlags;
	createInfo.surface = surface;

	displayAttributes.swapLength = std::max<uint32_t>(displayAttributes.swapLength, surfaceCapabilities.getMinImageCount());
	if (surfaceCapabilities.getMaxImageCount())
	{
		displayAttributes.swapLength = std::min<uint32_t>(displayAttributes.swapLength, surfaceCapabilities.getMaxImageCount());
	}

	displayAttributes.swapLength = std::min<uint32_t>(displayAttributes.swapLength, FrameworkCaps::MaxSwapChains);

	createInfo.minImageCount = displayAttributes.swapLength;
	createInfo.imageFormat = imageFormat.getFormat();

	createInfo.imageArrayLayers = 1;
	createInfo.imageColorSpace = imageFormat.getColorSpace();
	createInfo.imageExtent.setWidth(displayAttributes.width);
	createInfo.imageExtent.setHeight(displayAttributes.height);
	createInfo.imageUsage = swapchainImageUsageFlags;

	createInfo.preTransform = pvrvk::SurfaceTransformFlagsKHR::e_IDENTITY_BIT_KHR;
	if ((surfaceCapabilities.getSupportedTransforms() & pvrvk::SurfaceTransformFlagsKHR::e_IDENTITY_BIT_KHR) == 0)
	{
		throw new InvalidOperationError("Surface does not support VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR transformation");
	}

	createInfo.imageSharingMode = pvrvk::SharingMode::e_EXCLUSIVE;
	createInfo.presentMode = swapchainPresentMode;
	createInfo.numQueueFamilyIndex = 1;
	uint32_t queueFamily = 0;
	createInfo.queueFamilyIndices = &queueFamily;

	Swapchain swapchain;
	swapchain = device->createSwapchain(createInfo, surface);
	displayAttributes.swapLength = swapchain->getSwapchainLength();
	return swapchain;
}

void createDepthStencilImageAndViewsHelper(Device& device, pvr::DisplayAttributes& displayAttributes, pvrvk::Format* preferredDepthFormats, uint32_t numDepthFormats,
	const pvrvk::Extent2D& imageExtent, Multi<ImageView>& depthStencilImages, pvrvk::Format& outFormat, const pvrvk::ImageUsageFlags& imageUsageFlags,
	pvrvk::SampleCountFlags sampleCount, vma::Allocator* dsImageAllocator, vma::AllocationCreateFlags dsImageAllocationCreateFlags)
{
	pvrvk::Format depthStencilFormatRequested = getDepthStencilFormat(displayAttributes);
	pvrvk::Format supportedDepthStencilFormat = pvrvk::Format::e_UNDEFINED;

	pvrvk::Format frameworkPreferredDepthStencilFormat[6] = {
		pvrvk::Format::e_D32_SFLOAT_S8_UINT,
		pvrvk::Format::e_D24_UNORM_S8_UINT,
		pvrvk::Format::e_D16_UNORM_S8_UINT,
		pvrvk::Format::e_D32_SFLOAT,
		pvrvk::Format::e_D16_UNORM,
		pvrvk::Format::e_X8_D24_UNORM_PACK32,
	};

	std::vector<pvrvk::Format> depthFormats;

	if (numDepthFormats)
	{
		depthFormats.insert(depthFormats.begin(), &preferredDepthFormats[0], &preferredDepthFormats[numDepthFormats]);
	}
	else
	{
		depthFormats.insert(depthFormats.begin(), &frameworkPreferredDepthStencilFormat[0], &frameworkPreferredDepthStencilFormat[6]);
	}

	// start by checking for the requested depth stencil format
	pvrvk::Format currentDepthStencilFormat = depthStencilFormatRequested;
	for (uint32_t f = 0; f < depthFormats.size(); ++f)
	{
		pvrvk::FormatProperties prop = device->getPhysicalDevice()->getFormatProperties(currentDepthStencilFormat);
		if ((prop.getOptimalTilingFeatures() & pvrvk::FormatFeatureFlags::e_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
		{
			supportedDepthStencilFormat = currentDepthStencilFormat;
			break;
		}
		currentDepthStencilFormat = depthFormats[f];
	}

	if (depthStencilFormatRequested != supportedDepthStencilFormat)
	{
		Log(LogLevel::Information, "Requested DepthStencil VkFormat %s is not supported. Falling back to %s", depthStencilFormatToString(depthStencilFormatRequested).c_str(),
			depthStencilFormatToString(supportedDepthStencilFormat).c_str());
	}
	getDepthStencilBits(supportedDepthStencilFormat, displayAttributes.depthBPP, displayAttributes.stencilBPP);
	Log(LogLevel::Information, "DepthStencil VkFormat: %s", depthStencilFormatToString(supportedDepthStencilFormat).c_str());

	// create the depth stencil images
	depthStencilImages.resize(displayAttributes.swapLength);

	// the required memory property flags
	const pvrvk::MemoryPropertyFlags requiredMemoryProperties = pvrvk::MemoryPropertyFlags::e_DEVICE_LOCAL_BIT;

	// more optimal set of memory property flags
	const pvrvk::MemoryPropertyFlags optimalMemoryProperties = (imageUsageFlags & pvrvk::ImageUsageFlags::e_TRANSIENT_ATTACHMENT_BIT) != 0
		? pvrvk::MemoryPropertyFlags::e_LAZILY_ALLOCATED_BIT
		: pvrvk::MemoryPropertyFlags::e_DEVICE_LOCAL_BIT;

	for (int32_t i = 0; i < displayAttributes.swapLength; ++i)
	{
		Image depthStencilImage = createImage(device, pvrvk::ImageType::e_2D, supportedDepthStencilFormat, pvrvk::Extent3D(imageExtent.getWidth(), imageExtent.getHeight(), 1u),
			imageUsageFlags, pvrvk::ImageCreateFlags(0), pvrvk::ImageLayersSize(), sampleCount, requiredMemoryProperties, optimalMemoryProperties, dsImageAllocator,
			dsImageAllocationCreateFlags);
		depthStencilImage->setObjectName(std::string("PVRUtilsVk::Depth Stencil Image [") + std::to_string(i) + std::string("]"));

		depthStencilImages[i] = device->createImageView(depthStencilImage);
		depthStencilImages[i]->setObjectName(std::string("PVRUtilsVk::Depth Stencil Image View [") + std::to_string(i) + std::string("]"));
	}

	outFormat = supportedDepthStencilFormat;
}
} // namespace

namespace impl {
const Texture* decompressIfRequired(const Texture& texture, Texture& decompressedTexture, bool allowDecompress, bool supportPvrtc, bool& isDecompressed)
{
	const Texture* textureToUse = &texture;
	// Setup code to get various state
	// Generic error strings for textures being unsupported.
	const char* cszUnsupportedFormat = "TextureUtils.h:textureUpload:: Texture format %s is not supported in this implementation.\n";
	const char* cszUnsupportedFormatDecompressionAvailable = "TextureUtils.h:textureUpload:: Texture format %s is not supported in this implementation."
															 " Allowing software decompression (allowDecompress=true) will enable you to use this format.\n";

	// Check that extension support exists for formats supported in this way.
	// Check format not supportedfor formats only supported by extensions.
	switch (texture.getPixelFormat().getPixelTypeId())
	{
	case static_cast<uint64_t>(CompressedPixelFormat::PVRTCI_2bpp_RGB):
	case static_cast<uint64_t>(CompressedPixelFormat::PVRTCI_2bpp_RGBA):
	case static_cast<uint64_t>(CompressedPixelFormat::PVRTCI_4bpp_RGB):
	case static_cast<uint64_t>(CompressedPixelFormat::PVRTCI_4bpp_RGBA):
	{
		bool decompress = !supportPvrtc;
		if (decompress)
		{
			if (allowDecompress)
			{
				Log(LogLevel::Information,
					"PVRTC texture format support not detected. Decompressing PVRTC to"
					" corresponding format (RGBA32 or RGB24)");
				decompressPvrtc(texture, decompressedTexture);
				textureToUse = &decompressedTexture;
				isDecompressed = true;
			}
			else
			{
				Log(LogLevel::Error, cszUnsupportedFormatDecompressionAvailable, "PVRTC");
				return nullptr;
			}
		}
		break;
	}
	case static_cast<uint64_t>(CompressedPixelFormat::PVRTCII_2bpp):
	case static_cast<uint64_t>(CompressedPixelFormat::PVRTCII_4bpp):
	{
		if (!supportPvrtc)
		{
			Log(LogLevel::Error, cszUnsupportedFormat, "PVRTC2");
			return nullptr;
		}
		break;
	}
	case static_cast<uint64_t>(CompressedPixelFormat::ETC1):
		Log(LogLevel::Error, cszUnsupportedFormatDecompressionAvailable, "ETC1");
		return nullptr;
	case static_cast<uint64_t>(CompressedPixelFormat::DXT1):
		Log(LogLevel::Error, cszUnsupportedFormatDecompressionAvailable, "DXT1");
		return nullptr;
	case static_cast<uint64_t>(CompressedPixelFormat::DXT3):
		Log(LogLevel::Error, cszUnsupportedFormatDecompressionAvailable, "DXT1");
		return nullptr;
	case static_cast<uint64_t>(CompressedPixelFormat::DXT5):
		Log(LogLevel::Error, cszUnsupportedFormatDecompressionAvailable, "DXT3");
		return nullptr;
	default:
	{}
	}
	return textureToUse;
}
} // namespace impl

Image uploadImageHelper(Device& device, const Texture& texture, bool allowDecompress, CommandBufferBase commandBuffer, pvrvk::ImageUsageFlags usageFlags,
	pvrvk::ImageLayout finalLayout, vma::Allocator* bufferAllocator = nullptr, vma::Allocator* imageAllocator = nullptr,
	vma::AllocationCreateFlags imageAllocationCreateFlags = vma::AllocationCreateFlags::e_NONE)
{
	// Check that the texture is valid.
	if (!texture.getDataSize())
	{
		throw ErrorValidationFailedEXT("TextureUtils.h:textureUpload:: Invalid texture supplied, please verify inputs.");
	}
	commandBuffer->debugMarkerBeginEXT("PVRUtilsVk::uploadImage");
	bool isDecompressed;

	pvrvk::Format format = pvrvk::Format::e_UNDEFINED;

	// Texture to use if we decompress in software.
	Texture decompressedTexture;

	// Texture pointer which points at the texture we should use for the function.
	// Allows switching to, for example, a decompressed version of the texture.
	const Texture* textureToUse = impl::decompressIfRequired(texture, decompressedTexture, allowDecompress, device->supportsPVRTC(), isDecompressed);

	format = convertToPVRVkPixelFormat(textureToUse->getPixelFormat(), textureToUse->getColorSpace(), textureToUse->getChannelType());
	if (format == pvrvk::Format::e_UNDEFINED)
	{
		ErrorUnknown("TextureUtils.h:textureUpload:: Texture's pixel type is not supported by this API.");
	}

	uint32_t texWidth = static_cast<uint32_t>(textureToUse->getWidth());
	uint32_t texHeight = static_cast<uint32_t>(textureToUse->getHeight());
	uint32_t texDepth = static_cast<uint32_t>(textureToUse->getDepth());

	uint32_t dataWidth = static_cast<uint32_t>(textureToUse->getWidth());
	uint32_t dataHeight = static_cast<uint32_t>(textureToUse->getHeight());
	uint32_t dataDepth = static_cast<uint32_t>(textureToUse->getDepth());

	uint16_t texMipLevels = static_cast<uint16_t>(textureToUse->getNumMipMapLevels());
	uint16_t texArraySlices = static_cast<uint16_t>(textureToUse->getNumArrayMembers());
	uint16_t texFaces = static_cast<uint16_t>(textureToUse->getNumFaces());
	Image image;

	usageFlags |= pvrvk::ImageUsageFlags::e_TRANSFER_DST_BIT;

	if (texDepth > 1)
	{
		image = createImage(device, pvrvk::ImageType::e_3D, format, pvrvk::Extent3D(texWidth, texHeight, texDepth), usageFlags, pvrvk::ImageCreateFlags(0),
			pvrvk::ImageLayersSize(texArraySlices, static_cast<uint8_t>(texMipLevels)), pvrvk::SampleCountFlags::e_1_BIT, pvrvk::MemoryPropertyFlags::e_DEVICE_LOCAL_BIT,
			pvrvk::MemoryPropertyFlags::e_DEVICE_LOCAL_BIT, imageAllocator, imageAllocationCreateFlags);
	}
	else if (texHeight > 1)
	{
		image = createImage(device, pvrvk::ImageType::e_2D, format, pvrvk::Extent3D(texWidth, texHeight, 1u), usageFlags,
			pvrvk::ImageCreateFlags::e_CUBE_COMPATIBLE_BIT * (texture.getNumFaces() > 1) |
				pvrvk::ImageCreateFlags::e_2D_ARRAY_COMPATIBLE_BIT_KHR * static_cast<uint32_t>(texArraySlices > 1),
			pvrvk::ImageLayersSize(texArraySlices * (texture.getNumFaces() > 1 ? 6 : 1), static_cast<uint8_t>(texMipLevels)), pvrvk::SampleCountFlags::e_1_BIT,
			pvrvk::MemoryPropertyFlags::e_DEVICE_LOCAL_BIT, pvrvk::MemoryPropertyFlags::e_DEVICE_LOCAL_BIT, imageAllocator, imageAllocationCreateFlags);
	}
	else
	{
		image = createImage(device, pvrvk::ImageType::e_1D, format, pvrvk::Extent3D(texWidth, 1u, 1u), usageFlags, pvrvk::ImageCreateFlags(0),
			pvrvk::ImageLayersSize(texArraySlices, static_cast<uint8_t>(texMipLevels)), pvrvk::SampleCountFlags::e_1_BIT, pvrvk::MemoryPropertyFlags::e_DEVICE_LOCAL_BIT,
			pvrvk::MemoryPropertyFlags::e_DEVICE_LOCAL_BIT, imageAllocator, imageAllocationCreateFlags);
	}

	// POPULATE, TRANSITION ETC
	{
		// Create a bunch of buffers that will be used as copy destinations - each will be one mip level, one array slice / one face
		// Faces are considered array elements, so each Framework array slice in a cube array will be 6 vulkan array slices.

		// Edit the info to be the small, linear images that we are using.
		std::vector<ImageUpdateInfo> imageUpdates(texMipLevels * texArraySlices * texFaces);
		uint32_t imageUpdateIndex = 0;
		for (uint32_t mipLevel = 0; mipLevel < texMipLevels; ++mipLevel)
		{
			uint32_t minWidth, minHeight, minDepth;
			textureToUse->getMinDimensionsForFormat(minWidth, minHeight, minDepth);
			dataWidth = static_cast<uint32_t>(std::max(textureToUse->getWidth(mipLevel), minWidth));
			dataHeight = static_cast<uint32_t>(std::max(textureToUse->getHeight(mipLevel), minHeight));
			dataDepth = static_cast<uint32_t>(std::max(textureToUse->getDepth(mipLevel), minDepth));
			texWidth = textureToUse->getWidth(mipLevel);
			texHeight = textureToUse->getHeight(mipLevel);
			texDepth = textureToUse->getDepth(mipLevel);
			for (uint32_t arraySlice = 0; arraySlice < texArraySlices; ++arraySlice)
			{
				for (uint32_t face = 0; face < texFaces; ++face)
				{
					ImageUpdateInfo& update = imageUpdates[imageUpdateIndex];
					update.imageWidth = texWidth;
					update.imageHeight = texHeight;
					update.dataWidth = dataWidth;
					update.dataHeight = dataHeight;
					update.depth = texDepth;
					update.arrayIndex = arraySlice;
					update.cubeFace = face;
					update.mipLevel = mipLevel;
					update.data = textureToUse->getDataPointer(mipLevel, arraySlice, face);
					update.dataSize = textureToUse->getDataSize(mipLevel, false, false);
					++imageUpdateIndex;
				} // next face
			} // next arrayslice
		} // next miplevel

		updateImage(device, commandBuffer, imageUpdates.data(), static_cast<uint32_t>(imageUpdates.size()), format, finalLayout, texFaces > 1, image, bufferAllocator);
	}
	commandBuffer->debugMarkerEndEXT();
	return image;
}

ImageView uploadImageAndViewHelper(Device& device, const Texture& texture, bool allowDecompress, CommandBufferBase commandBuffer, pvrvk::ImageUsageFlags usageFlags,
	pvrvk::ImageLayout finalLayout, vma::Allocator* bufferAllocator = nullptr, vma::Allocator* imageAllocator = nullptr,
	vma::AllocationCreateFlags imageAllocationCreateFlags = vma::AllocationCreateFlags::e_NONE)
{
	ComponentMapping swizzle = {
		pvrvk::ComponentSwizzle::e_IDENTITY,
		pvrvk::ComponentSwizzle::e_IDENTITY,
		pvrvk::ComponentSwizzle::e_IDENTITY,
		pvrvk::ComponentSwizzle::e_IDENTITY,
	};
	if (texture.getPixelFormat().getChannelContent(0) == 'l')
	{
		if (texture.getPixelFormat().getChannelContent(1) == 'a')
		{
			swizzle.setR(pvrvk::ComponentSwizzle::e_R);
			swizzle.setG(pvrvk::ComponentSwizzle::e_R);
			swizzle.setB(pvrvk::ComponentSwizzle::e_R);
			swizzle.setA(pvrvk::ComponentSwizzle::e_G);
		}
		else
		{
			swizzle.setR(pvrvk::ComponentSwizzle::e_R);
			swizzle.setG(pvrvk::ComponentSwizzle::e_R);
			swizzle.setB(pvrvk::ComponentSwizzle::e_R);
			swizzle.setA(pvrvk::ComponentSwizzle::e_ONE);
		}
	}
	else if (texture.getPixelFormat().getChannelContent(0) == 'a')
	{
		swizzle.setR(pvrvk::ComponentSwizzle::e_ZERO);
		swizzle.setG(pvrvk::ComponentSwizzle::e_ZERO);
		swizzle.setB(pvrvk::ComponentSwizzle::e_ZERO);
		swizzle.setA(pvrvk::ComponentSwizzle::e_R);
	}
	return device->createImageView(
		uploadImageHelper(device, texture, allowDecompress, commandBuffer, usageFlags, finalLayout, bufferAllocator, imageAllocator, imageAllocationCreateFlags), swizzle);
}

inline ImageView loadAndUploadImageAndViewHelper(Device& device, const char* fileName, bool allowDecompress, CommandBufferBase commandBuffer, IAssetProvider& assetProvider,
	pvrvk::ImageUsageFlags usageFlags, pvrvk::ImageLayout finalLayout, Texture* outAssetTexture = nullptr, vma::Allocator* imageAllocator = nullptr,
	vma::Allocator* bufferAllocator = nullptr, vma::AllocationCreateFlags imageAllocationCreateFlags = vma::AllocationCreateFlags::e_NONE)
{
	Texture outTexture;
	Texture* pOutTexture = &outTexture;
	if (outAssetTexture)
	{
		pOutTexture = outAssetTexture;
	}
	auto assetStream = assetProvider.getAssetStream(fileName);
	*pOutTexture = pvr::assets::textureLoad(assetStream, pvr::getTextureFormatFromFilename(fileName));
	return uploadImageAndViewHelper(device, *pOutTexture, allowDecompress, commandBuffer, usageFlags, finalLayout, bufferAllocator, imageAllocator, imageAllocationCreateFlags);
}

inline Image loadAndUploadImageHelper(Device& device, const char* fileName, bool allowDecompress, CommandBufferBase commandBuffer, IAssetProvider& assetProvider,
	pvrvk::ImageUsageFlags usageFlags, pvrvk::ImageLayout finalLayout, Texture* outAssetTexture = nullptr, vma::Allocator* stagingBufferAllocator = nullptr,
	vma::Allocator* imageAllocator = nullptr, vma::AllocationCreateFlags imageAllocationCreateFlags = vma::AllocationCreateFlags::e_NONE)
{
	Texture outTexture;
	Texture* pOutTexture = &outTexture;
	if (outAssetTexture)
	{
		pOutTexture = outAssetTexture;
	}
	auto assetStream = assetProvider.getAssetStream(fileName);
	*pOutTexture = pvr::assets::textureLoad(assetStream, pvr::getTextureFormatFromFilename(fileName));
	return uploadImageHelper(device, *pOutTexture, allowDecompress, commandBuffer, usageFlags, finalLayout, stagingBufferAllocator, imageAllocator, imageAllocationCreateFlags);
}

ImageView loadAndUploadImageAndView(Device& device, const char* fileName, bool allowDecompress, CommandBuffer& commandBuffer, IAssetProvider& assetProvider,
	pvrvk::ImageUsageFlags usageFlags, pvrvk::ImageLayout finalLayout, Texture* outAssetTexture, vma::Allocator* stagingBufferAllocator, vma::Allocator* imageAllocator,
	vma::AllocationCreateFlags imageAllocationCreateFlags)
{
	return loadAndUploadImageAndViewHelper(device, fileName, allowDecompress, CommandBufferBase(commandBuffer), assetProvider, usageFlags, finalLayout, outAssetTexture,
		imageAllocator, stagingBufferAllocator, imageAllocationCreateFlags);
}

ImageView loadAndUploadImageAndView(Device& device, const char* fileName, bool allowDecompress, SecondaryCommandBuffer& commandBuffer, IAssetProvider& assetProvider,
	pvrvk::ImageUsageFlags usageFlags, pvrvk::ImageLayout finalLayout, Texture* outAssetTexture, vma::Allocator* stagingBufferAllocator, vma::Allocator* imageAllocator,
	vma::AllocationCreateFlags imageAllocationCreateFlags)
{
	return loadAndUploadImageAndViewHelper(device, fileName, allowDecompress, CommandBufferBase(commandBuffer), assetProvider, usageFlags, finalLayout, outAssetTexture,
		imageAllocator, stagingBufferAllocator, imageAllocationCreateFlags);
}

Image loadAndUploadImage(Device& device, const char* fileName, bool allowDecompress, CommandBuffer& commandBuffer, IAssetProvider& assetProvider, pvrvk::ImageUsageFlags usageFlags,
	pvrvk::ImageLayout finalLayout, Texture* outAssetTexture, vma::Allocator* stagingBufferAllocator, vma::Allocator* imageAllocator,
	vma::AllocationCreateFlags imageAllocationCreateFlags)
{
	return loadAndUploadImageHelper(device, fileName, allowDecompress, CommandBufferBase(commandBuffer), assetProvider, usageFlags, finalLayout, outAssetTexture,
		stagingBufferAllocator, imageAllocator, imageAllocationCreateFlags);
}

Image loadAndUploadImage(Device& device, const char* fileName, bool allowDecompress, SecondaryCommandBuffer& commandBuffer, IAssetProvider& assetProvider,
	pvrvk::ImageUsageFlags usageFlags, pvrvk::ImageLayout finalLayout, Texture* outAssetTexture, vma::Allocator* stagingBufferAllocator, vma::Allocator* imageAllocator,
	vma::AllocationCreateFlags imageAllocationCreateFlags)
{
	return loadAndUploadImageHelper(device, fileName, allowDecompress, CommandBufferBase(commandBuffer), assetProvider, usageFlags, finalLayout, outAssetTexture,
		stagingBufferAllocator, imageAllocator, imageAllocationCreateFlags);
}

ImageView uploadImageAndView(pvrvk::Device& device, const Texture& texture, bool allowDecompress, pvrvk::SecondaryCommandBuffer& commandBuffer, pvrvk::ImageUsageFlags usageFlags,
	pvrvk::ImageLayout finalLayout, vma::Allocator* stagingBufferAllocator, vma::Allocator* imageAllocator, vma::AllocationCreateFlags imageAllocationCreateFlags)
{
	return uploadImageAndViewHelper(
		device, texture, allowDecompress, CommandBufferBase(commandBuffer), usageFlags, finalLayout, stagingBufferAllocator, imageAllocator, imageAllocationCreateFlags);
}

ImageView uploadImageAndView(Device& device, const Texture& texture, bool allowDecompress, CommandBuffer& commandBuffer, pvrvk::ImageUsageFlags usageFlags,
	pvrvk::ImageLayout finalLayout, vma::Allocator* stagingBufferAllocator, vma::Allocator* imageAllocator, vma::AllocationCreateFlags imageAllocationCreateFlags)
{
	return uploadImageAndViewHelper(
		device, texture, allowDecompress, CommandBufferBase(commandBuffer), usageFlags, finalLayout, stagingBufferAllocator, imageAllocator, imageAllocationCreateFlags);
}

void generateTextureAtlas(pvrvk::Device& device, const pvrvk::Image* inputImages, pvrvk::Rect2Df* outUVs, uint32_t numImages, pvrvk::ImageLayout inputImageLayout,
	pvrvk::ImageView* outImageView, TextureHeader* outDescriptor, pvrvk::CommandBufferBase cmdBuffer, pvrvk::ImageLayout finalLayout, vma::Allocator* imageAllocator,
	vma::AllocationCreateFlags imageAllocationCreateFlags)
{
	TextureHeader header;
	struct SortedImage
	{
		uint32_t id;
		Image image;
		uint16_t width;
		uint16_t height;
		uint16_t srcX;
		uint16_t srcY;
		bool hasAlpha;
	};
	std::vector<SortedImage> sortedImage(numImages);
	struct SortCompare
	{
		bool operator()(const SortedImage& a, const SortedImage& b)
		{
			uint32_t aSize = a.width * a.height;
			uint32_t bSize = b.width * b.height;
			return (aSize > bSize);
		}
	};

	struct Area
	{
		int32_t x;
		int32_t y;
		int32_t w;
		int32_t h;
		int32_t size;
		bool isFilled;

		Area* right;
		Area* left;

	private:
		void setSize(int32_t width, int32_t height)
		{
			w = width;
			h = height;
			size = width * height;
		}

	public:
		Area(int32_t width, int32_t height) : x(0), y(0), isFilled(false), right(NULL), left(NULL)
		{
			setSize(width, height);
		}

		Area() : x(0), y(0), isFilled(false), right(NULL), left(NULL)
		{
			setSize(0, 0);
		}

		Area* insert(int32_t width, int32_t height)
		{
			// If this area has branches below it (i.e. is not a leaf) then traverse those.
			// Check the left branch first.
			if (left)
			{
				Area* tempPtr = NULL;
				tempPtr = left->insert(width, height);
				if (tempPtr != NULL)
				{
					return tempPtr;
				}
			}
			// Now check right
			if (right)
			{
				return right->insert(width, height);
			}
			// Already filled!
			if (isFilled)
			{
				return NULL;
			}

			// Too small
			if (size < width * height || w < width || h < height)
			{
				return NULL;
			}

			// Just right!
			if (size == width * height && w == width && h == height)
			{
				isFilled = true;
				return this;
			}
			// Too big. Split up.
			if (size > width * height && w >= width && h >= height)
			{
				// Initializes the children, and sets the left child's coordinates as these don't change.
				left = new Area;
				right = new Area;
				left->x = x;
				left->y = y;

				// --- Splits the current area depending on the size and position of the placed texture.
				// Splits vertically if larger free distance across the texture.
				if ((w - width) > (h - height))
				{
					left->w = width;
					left->h = h;

					right->x = x + width;
					right->y = y;
					right->w = w - width;
					right->h = h;
				}
				// Splits horizontally if larger or equal free distance downwards.
				else
				{
					left->w = w;
					left->h = height;

					right->x = x;
					right->y = y + height;
					right->w = w;
					right->h = h - height;
				}

				// Initializes the child members' size attributes.
				left->size = left->h * left->w;
				right->size = right->h * right->w;

				// Inserts the texture into the left child member.
				return left->insert(width, height);
			}
			// Catch all error return.
			return NULL;
		}

		bool deleteArea()
		{
			if (left != NULL)
			{
				if (left->left != NULL)
				{
					if (!left->deleteArea())
					{
						return false;
					}
					if (!right->deleteArea())
					{
						return false;
					}
				}
			}
			if (right != NULL)
			{
				if (right->left != NULL)
				{
					if (!left->deleteArea())
					{
						return false;
					}
					if (!right->deleteArea())
					{
						return false;
					}
				}
			}
			delete right;
			right = NULL;
			delete left;
			left = NULL;
			return true;
		}
	};

	// load the textures
	for (uint32_t i = 0; i < numImages; ++i)
	{
		sortedImage[i].image = inputImages[i];
		sortedImage[i].id = i;
		sortedImage[i].width = static_cast<uint16_t>(inputImages[i]->getWidth());
		sortedImage[i].height = static_cast<uint16_t>(inputImages[i]->getHeight());
	}
	//// sort the sprites
	std::sort(sortedImage.begin(), sortedImage.end(), SortCompare());
	// find the best width and height
	int32_t width = 0, height = 0, area = 0;
	uint32_t preferredDim[] = { 8, 16, 32, 64, 128, 256, 512, 1024 };
	const uint32_t atlasPixelBorder = 1;
	const uint32_t totalBorder = atlasPixelBorder * 2;
	uint32_t sortedImagesIterator = 0;
	// calculate the total area
	for (; sortedImagesIterator < sortedImage.size(); ++sortedImagesIterator)
	{
		area += (sortedImage[sortedImagesIterator].width + totalBorder) * (sortedImage[sortedImagesIterator].height + totalBorder);
	}
	sortedImagesIterator = 0;
	while ((static_cast<int32_t>(preferredDim[sortedImagesIterator]) * static_cast<int32_t>(preferredDim[sortedImagesIterator])) < area &&
		sortedImagesIterator < sizeof(preferredDim) / sizeof(preferredDim[0]))
	{
		++sortedImagesIterator;
	}
	if (sortedImagesIterator >= sizeof(preferredDim) / sizeof(preferredDim[0]))
	{
		throw ErrorValidationFailedEXT("Cannot find a best size for the texture atlas");
	}

	cmdBuffer->debugMarkerBeginEXT("PVRUtilsVk::generateTextureAtlas");

	width = height = preferredDim[sortedImagesIterator];
	float oneOverWidth = 1.f / width;
	float oneOverHeight = 1.f / height;
	Area* head = new Area(width, height);
	Area* pRtrn = nullptr;
	pvrvk::Offset3D dstOffsets[2];

	// create the out texture store
	pvrvk::Format outFmt = pvrvk::Format::e_R8G8B8A8_UNORM;
	Image outTexStore = createImage(device, pvrvk::ImageType::e_2D, outFmt, pvrvk::Extent3D(width, height, 1u),
		pvrvk::ImageUsageFlags::e_SAMPLED_BIT | pvrvk::ImageUsageFlags::e_TRANSFER_DST_BIT, pvrvk::ImageCreateFlags::e_NONE, pvrvk::ImageLayersSize(),
		pvrvk::SampleCountFlags::e_1_BIT, pvrvk::MemoryPropertyFlags::e_DEVICE_LOCAL_BIT, pvrvk::MemoryPropertyFlags::e_DEVICE_LOCAL_BIT, imageAllocator, imageAllocationCreateFlags);

	utils::setImageLayout(outTexStore, pvrvk::ImageLayout::e_UNDEFINED, pvrvk::ImageLayout::e_TRANSFER_DST_OPTIMAL, cmdBuffer);

	ImageView view = device->createImageView(outTexStore);
	cmdBuffer->clearColorImage(view, ClearColorValue(0.0f, 0.f, 0.f, 0.f), pvrvk::ImageLayout::e_TRANSFER_DST_OPTIMAL);

	for (uint32_t i = 0; i < numImages; ++i)
	{
		const SortedImage& image = sortedImage[i];
		pRtrn = head->insert(static_cast<int32_t>(sortedImage[i].width) + totalBorder, static_cast<int32_t>(sortedImage[i].height) + totalBorder);
		if (!pRtrn)
		{
			head->deleteArea();
			delete head;
			throw ErrorUnknown("Cannot find a best size for the texture atlas");
		}
		dstOffsets[0].setX(static_cast<uint16_t>(pRtrn->x + atlasPixelBorder));
		dstOffsets[0].setY(static_cast<uint16_t>(pRtrn->y + atlasPixelBorder));
		dstOffsets[0].setZ(0);

		dstOffsets[1].setX(static_cast<uint16_t>(dstOffsets[0].getX() + sortedImage[i].width));
		dstOffsets[1].setY(static_cast<uint16_t>(dstOffsets[0].getY() + sortedImage[i].height));
		dstOffsets[1].setZ(1);

		pvrvk::Offset2Df offset(dstOffsets[0].getX() * oneOverWidth, dstOffsets[0].getY() * oneOverHeight);
		pvrvk::Extent2Df extent(sortedImage[i].width * oneOverWidth, sortedImage[i].height * oneOverHeight);

		outUVs[image.id].setOffset(offset);
		outUVs[image.id].setExtent(extent);

		pvrvk::Offset3D srcOffsets[2] = { pvrvk::Offset3D(0, 0, 0), pvrvk::Offset3D(image.width, image.height, 1) };
		ImageBlit blit(pvrvk::ImageSubresourceLayers(), srcOffsets, pvrvk::ImageSubresourceLayers(), dstOffsets);

		cmdBuffer->blitImage(sortedImage[i].image, outTexStore, &blit, 1, pvrvk::Filter::e_NEAREST, inputImageLayout, pvrvk::ImageLayout::e_TRANSFER_DST_OPTIMAL);
	}
	if (outDescriptor)
	{
		outDescriptor->setWidth(width);
		outDescriptor->setHeight(height);
		outDescriptor->setChannelType(VariableType::UnsignedByteNorm);
		outDescriptor->setColorSpace(ColorSpace::lRGB);
		outDescriptor->setDepth(1);
		outDescriptor->setPixelFormat(PixelFormat::RGBA_8888);
	}
	*outImageView = device->createImageView(outTexStore);

	const uint32_t queueFamilyId = cmdBuffer->getCommandPool()->getQueueFamilyId();

	MemoryBarrierSet barrier;
	barrier.addBarrier(pvrvk::ImageMemoryBarrier(pvrvk::AccessFlags::e_TRANSFER_WRITE_BIT, pvrvk::AccessFlags::e_SHADER_READ_BIT, outTexStore,
		pvrvk::ImageSubresourceRange(pvrvk::ImageAspectFlags::e_COLOR_BIT), pvrvk::ImageLayout::e_TRANSFER_DST_OPTIMAL, finalLayout, queueFamilyId, queueFamilyId));

	cmdBuffer->pipelineBarrier(pvrvk::PipelineStageFlags::e_TRANSFER_BIT, pvrvk::PipelineStageFlags::e_FRAGMENT_SHADER_BIT | pvrvk::PipelineStageFlags::e_COMPUTE_SHADER_BIT, barrier);

	head->deleteArea();
	delete head;

	cmdBuffer->debugMarkerEndEXT();
}

pvrvk::Device createDeviceAndQueues(PhysicalDevice physicalDevice, const QueuePopulateInfo* queueCreateFlags, uint32_t numQueueCreateFlags, QueueAccessInfo* outAccessInfo,
	const DeviceExtensions& deviceExtensions)
{
	std::vector<DeviceQueueCreateInfo> queueCreateInfo;
	const std::vector<QueueFamilyProperties>& queueFamilyProperties = physicalDevice->getQueueFamilyProperties();
	std::vector<uint32_t> queuesRemaining;
	queuesRemaining.resize(queueFamilyProperties.size());

	const char* graphics = "GRAPHICS ";
	const char* compute = "COMPUTE ";
	const char* present = "PRESENT ";
	const char* transfer = "TRANSFER ";
	const char* sparse = "SPARSE_BINDING ";
	const char* nothing = "";

	// Log the supported queue families
	Log(LogLevel::Information, "Supported Queue Families:");
	for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); ++i)
	{
		Log(LogLevel::Information, "\tqueue family %d (#queues %d)  FLAGS: %d ( %s%s%s%s%s)", i, queueFamilyProperties[i].getQueueCount(), queueFamilyProperties[i].getQueueFlags(),
			((queueFamilyProperties[i].getQueueFlags() & QueueFlags::e_GRAPHICS_BIT) != 0) ? graphics : nothing,
			((queueFamilyProperties[i].getQueueFlags() & QueueFlags::e_COMPUTE_BIT) != 0) ? compute : nothing,
			((queueFamilyProperties[i].getQueueFlags() & QueueFlags::e_TRANSFER_BIT) != 0) ? transfer : nothing,
			((queueFamilyProperties[i].getQueueFlags() & QueueFlags::e_SPARSE_BINDING_BIT) != 0) ? sparse : nothing, nothing, nothing);
	}

	for (uint32_t i = 0; i < queueFamilyProperties.size(); ++i)
	{
		queuesRemaining[i] = queueFamilyProperties[i].getQueueCount();
	}

	std::vector<int32_t> queueIndices(queueFamilyProperties.size(), -1);
	std::vector<pvrvk::QueueFlags> queueFlags(queueFamilyProperties.size(), pvrvk::QueueFlags(0));
	for (uint32_t i = 0; i < numQueueCreateFlags; ++i)
	{
		for (uint32_t j = 0; j < queueFamilyProperties.size(); ++j)
		{
			// look for the flags
			if (((static_cast<uint32_t>(queueFamilyProperties[j].getQueueFlags()) & static_cast<uint32_t>(queueCreateFlags[i].queueFlags)) ==
					static_cast<uint32_t>(queueCreateFlags[i].queueFlags)) &&
				queuesRemaining[j])
			{
				if (queueCreateFlags[i].surface.isValid()) // look for presentation
				{
					if (physicalDevice->getSurfaceSupport(j, queueCreateFlags[i].surface))
					{
						outAccessInfo[i].familyId = j;
						outAccessInfo[i].queueId = ++queueIndices[j];
						queueFlags[j] |= queueCreateFlags[i].queueFlags;
						--queuesRemaining[j];
						break;
					}
				}
				else
				{
					outAccessInfo[i].familyId = j;
					outAccessInfo[i].queueId = ++queueIndices[j];
					--queuesRemaining[j];
					break;
				}
			}
		}
	}

	// populate the queue create info
	for (uint32_t i = 0; i < queueIndices.size(); ++i)
	{
		if (queueIndices[i] != -1)
		{
			queueCreateInfo.push_back(DeviceQueueCreateInfo());
			DeviceQueueCreateInfo& createInfo = queueCreateInfo.back();
			createInfo.setQueueFamilyIndex(i);
			for (uint32_t j = 0; j < static_cast<uint32_t>(queueIndices[i] + 1); ++j)
			{
				createInfo.addQueue(1.f);
			}
		}
	}

	// create the device
	DeviceCreateInfo deviceInfo;
	PhysicalDeviceFeatures features = physicalDevice->getFeatures();
	// We disable robustBufferAccess unless the application is being run in debug mode. "robustBufferAccess" specifies whether accesses to buffers are bounds - checked against
	// the range of the buffer descriptor. Enabling robustBufferAccess in debug mode provides additional robustness and validation above and beyond that of the validation layers.
#ifdef DEBUG
	features.setRobustBufferAccess(true);
#else
	features.setRobustBufferAccess(false);
#endif
	deviceInfo.setEnabledFeatures(&features);
	deviceInfo.setDeviceQueueCreateInfos(queueCreateInfo);

	// Filter the given set of extensions so only the set of device extensions which are supported by the device remain
	if (deviceExtensions.extensionStrings.size())
	{
		deviceInfo.setEnabledExtensions(Extensions::filterExtensions(
			physicalDevice->enumerateDeviceExtensionsProperties(), deviceExtensions.extensionStrings.data(), static_cast<uint32_t>(deviceExtensions.extensionStrings.size())));
		if (deviceInfo.getNumEnabledExtensionNames() != deviceExtensions.extensionStrings.size())
		{
			Log(LogLevel::Warning, "Not all requested Logical device extensions are supported");
		}

		Log(LogLevel::Information, "Supported Device Extensions:");
		for (uint32_t i = 0; i < deviceInfo.getNumEnabledExtensionNames(); ++i)
		{
			Log(LogLevel::Information, "\t%s", deviceInfo.getEnabledExtensionName(i).c_str());
		}
	}

	pvrvk::Device outDevice = physicalDevice->createDevice(deviceInfo);

	// Log the retrieved queues
	Log(LogLevel::Information, "Queues Created:");
	for (uint32_t i = 0; i < queueCreateInfo.size(); ++i)
	{
		bool supportsWsi = physicalDevice->getSurfaceSupport(i, queueCreateFlags[i].surface);

		Log(LogLevel::Information, "\t queue Family: %d ( %s%s%s%s%s) \tqueue count: %d", queueCreateInfo[i].getQueueFamilyIndex(),
			((queueFamilyProperties[queueCreateInfo[i].getQueueFamilyIndex()].getQueueFlags() & QueueFlags::e_GRAPHICS_BIT) != 0) ? graphics : nothing,
			((queueFamilyProperties[queueCreateInfo[i].getQueueFamilyIndex()].getQueueFlags() & QueueFlags::e_COMPUTE_BIT) != 0) ? compute : nothing,
			((queueFamilyProperties[queueCreateInfo[i].getQueueFamilyIndex()].getQueueFlags() & QueueFlags::e_TRANSFER_BIT) != 0) ? transfer : nothing,
			((queueFamilyProperties[queueCreateInfo[i].getQueueFamilyIndex()].getQueueFlags() & QueueFlags::e_SPARSE_BINDING_BIT) != 0) ? sparse : nothing,
			(supportsWsi ? present : nothing), queueCreateInfo[i].getNumQueues());
	}

	return outDevice;
}

void createSwapchainAndDepthStencilImageAndViews(Device& device, const Surface& surface, DisplayAttributes& displayAttributes, Swapchain& outSwapchain,
	Multi<ImageView>& outDepthStencil, const pvrvk::ImageUsageFlags& swapchainImageUsageFlags, const pvrvk::ImageUsageFlags& dsImageUsageFlags, vma::Allocator* dsImageAllocator,
	vma::AllocationCreateFlags dsImageAllocationCreateFlags)
{
	outSwapchain = createSwapchain(device, surface, displayAttributes, swapchainImageUsageFlags);

	pvrvk::Format outDepthFormat;
	return createDepthStencilImageAndViewsHelper(device, displayAttributes, nullptr, 0, outSwapchain->getDimension(), outDepthStencil, outDepthFormat, dsImageUsageFlags,
		pvrvk::SampleCountFlags::e_1_BIT, dsImageAllocator, dsImageAllocationCreateFlags);
}

void createSwapchainAndDepthStencilImageAndViews(Device& device, const Surface& surface, DisplayAttributes& displayAttributes, Swapchain& outSwapchain,
	Multi<ImageView>& outDepthStencil, pvrvk::Format* preferredColorFormats, uint32_t numColorFormats, pvrvk::Format* preferredDepthFormats, uint32_t numDepthFormats,
	const pvrvk::ImageUsageFlags& swapchainImageUsageFlags, const pvrvk::ImageUsageFlags& dsImageUsageFlags, vma::Allocator* dsImageAllocator,
	vma::AllocationCreateFlags dsImageAllocationCreateFlags)
{
	outSwapchain = createSwapchain(device, surface, displayAttributes, preferredColorFormats, numColorFormats, swapchainImageUsageFlags);
	pvrvk::Format dsFormat;
	return createDepthStencilImageAndViewsHelper(device, displayAttributes, preferredDepthFormats, numDepthFormats, outSwapchain->getDimension(), outDepthStencil, dsFormat,
		dsImageUsageFlags, pvrvk::SampleCountFlags::e_1_BIT, dsImageAllocator, dsImageAllocationCreateFlags);
}

Swapchain createSwapchain(Device& device, const Surface& surface, pvr::DisplayAttributes& displayAttributes, pvrvk::Format* preferredColorFormats, uint32_t numColorFormats,
	pvrvk::ImageUsageFlags swapchainImageUsageFlags)
{
	return createSwapchainHelper(device, surface, displayAttributes, swapchainImageUsageFlags, preferredColorFormats, numColorFormats);
}

Swapchain createSwapchain(Device& device, const Surface& surface, pvr::DisplayAttributes& displayAttributes, pvrvk::ImageUsageFlags swapchainImageUsageFlags)
{
	pvrvk::Format formats[1];
	return createSwapchainHelper(device, surface, displayAttributes, swapchainImageUsageFlags, formats, 0);
}

void createDepthStencilImages(Device device, pvr::DisplayAttributes& displayAttributes, const pvrvk::Extent2D& imageExtent, Multi<ImageView>& depthStencilImages,
	pvrvk::Format& outFormat, const pvrvk::ImageUsageFlags& swapchainImageUsageFlags, pvrvk::SampleCountFlags sampleCount, vma::Allocator* dsImageAllocator,
	vma::AllocationCreateFlags dsImageAllocationCreateFlags)
{
	pvrvk::Format formats[1];
	createDepthStencilImageAndViewsHelper(
		device, displayAttributes, formats, 0, imageExtent, depthStencilImages, outFormat, swapchainImageUsageFlags, sampleCount, dsImageAllocator, dsImageAllocationCreateFlags);
}

void createDepthStencilImages(Device device, pvr::DisplayAttributes& displayAttributes, pvrvk::Format* preferredDepthFormats, uint32_t numDepthFormats,
	const pvrvk::Extent2D& imageExtent, Multi<ImageView>& depthStencilImages, pvrvk::Format& outFormat, const pvrvk::ImageUsageFlags& swapchainImageUsageFlags,
	pvrvk::SampleCountFlags sampleCount, vma::Allocator* dsImageAllocator, vma::AllocationCreateFlags dsImageAllocationCreateFlags)
{
	createDepthStencilImageAndViewsHelper(device, displayAttributes, preferredDepthFormats, numDepthFormats, imageExtent, depthStencilImages, outFormat, swapchainImageUsageFlags,
		sampleCount, dsImageAllocator, dsImageAllocationCreateFlags);
}

namespace {
void screenCaptureRegion(Device device, Image swapChainImage, CommandPool& cmdPool, Queue& queue, uint32_t x, uint32_t y, uint32_t w, uint32_t h, char* outBuffer,
	uint32_t strideInBytes, pvrvk::Format requestedImageFormat, pvrvk::ImageLayout initialLayout, pvrvk::ImageLayout finalLayout, vma::Allocator* bufferAllocator,
	vma::Allocator* imageAllocator)
{
	CommandBuffer cmdBuffer = cmdPool->allocateCommandBuffer();
	const uint16_t width = static_cast<uint16_t>(w - x);
	const uint16_t height = static_cast<uint16_t>(h - y);
	const uint32_t dataSize = strideInBytes * width * height;
	// create the destination texture which does the format conversion

	const pvrvk::FormatProperties& formatProps = device->getPhysicalDevice()->getFormatProperties(requestedImageFormat);
	if ((formatProps.getOptimalTilingFeatures() & pvrvk::FormatFeatureFlags::e_BLIT_DST_BIT) == 0)
	{
		throw ErrorValidationFailedEXT("Screen Capture requested Image format is not supported");
	}

	// Create the intermediate image which will be used as the format conversion
	// when copying from swapchain image and then copied into the buffer
	Image dstImage = createImage(device, pvrvk::ImageType::e_2D, requestedImageFormat, pvrvk::Extent3D(width, height, 1u),
		pvrvk::ImageUsageFlags::e_TRANSFER_DST_BIT | pvrvk::ImageUsageFlags::e_TRANSFER_SRC_BIT, pvrvk::ImageCreateFlags::e_NONE, pvrvk::ImageLayersSize(),
		pvrvk::SampleCountFlags::e_1_BIT, pvrvk::MemoryPropertyFlags::e_DEVICE_LOCAL_BIT, pvrvk::MemoryPropertyFlags::e_DEVICE_LOCAL_BIT, imageAllocator);

	const pvrvk::Offset3D srcOffsets[2] = { pvrvk::Offset3D(static_cast<uint16_t>(x), static_cast<uint16_t>(y), 0),
		pvrvk::Offset3D(static_cast<uint16_t>(w), static_cast<uint16_t>(h), 1) };

	const pvrvk::Offset3D dstOffsets[2] = { pvrvk::Offset3D(static_cast<uint16_t>(x), static_cast<uint16_t>(h), 0),
		pvrvk::Offset3D(static_cast<uint16_t>(w), static_cast<uint16_t>(y), 1) };

	// create the final destination buffer for reading
	Buffer buffer = createBuffer(device, dataSize, pvrvk::BufferUsageFlags::e_TRANSFER_DST_BIT, pvrvk::MemoryPropertyFlags::e_HOST_VISIBLE_BIT,
		pvrvk::MemoryPropertyFlags::e_HOST_VISIBLE_BIT | pvrvk::MemoryPropertyFlags::e_DEVICE_LOCAL_BIT, bufferAllocator, pvr::utils::vma::AllocationCreateFlags::e_MAPPED_BIT);
	buffer->setObjectName("PVRUtilsVk::screenCaptureRegion::Temporary Screen Capture Buffer");

	cmdBuffer->begin(pvrvk::CommandBufferUsageFlags::e_ONE_TIME_SUBMIT_BIT);
	cmdBuffer->debugMarkerBeginEXT("PVRUtilsVk::screenCaptureRegion");
	ImageBlit copyRange(pvrvk::ImageSubresourceLayers(), srcOffsets, pvrvk::ImageSubresourceLayers(), dstOffsets);

	// transform the layout from the color attachment to transfer src
	setImageLayout(swapChainImage, initialLayout, pvrvk::ImageLayout::e_TRANSFER_SRC_OPTIMAL, cmdBuffer);
	setImageLayout(dstImage, pvrvk::ImageLayout::e_UNDEFINED, pvrvk::ImageLayout::e_TRANSFER_DST_OPTIMAL, cmdBuffer);

	cmdBuffer->blitImage(swapChainImage, dstImage, &copyRange, 1, pvrvk::Filter::e_LINEAR, pvrvk::ImageLayout::e_TRANSFER_SRC_OPTIMAL, pvrvk::ImageLayout::e_TRANSFER_DST_OPTIMAL);

	pvrvk::ImageSubresourceLayers subResource;
	subResource.setAspectMask(pvrvk::ImageAspectFlags::e_COLOR_BIT);
	BufferImageCopy region(0, 0, 0, subResource, pvrvk::Offset3D(x, y, 0), pvrvk::Extent3D(w, h, 1));

	setImageLayout(swapChainImage, pvrvk::ImageLayout::e_TRANSFER_SRC_OPTIMAL, finalLayout, cmdBuffer);
	setImageLayout(dstImage, pvrvk::ImageLayout::e_TRANSFER_DST_OPTIMAL, pvrvk::ImageLayout::e_TRANSFER_SRC_OPTIMAL, cmdBuffer);

	cmdBuffer->copyImageToBuffer(dstImage, pvrvk::ImageLayout::e_TRANSFER_SRC_OPTIMAL, buffer, &region, 1);
	cmdBuffer->debugMarkerEndEXT();
	cmdBuffer->end();
	// create a fence for wait.
	Fence fenceWait = device->createFence(pvrvk::FenceCreateFlags(0));
	SubmitInfo submitInfo;
	submitInfo.commandBuffers = &cmdBuffer;
	submitInfo.numCommandBuffers = 1;
	queue->submit(&submitInfo, 1, fenceWait);
	fenceWait->wait(); // wait for the submit to finish so that the command buffer get destroyed properly

	// map the buffer and copy the data
	void* memory = 0;
	unsigned char* data = nullptr;
	bool unmap = false;
	if (!buffer->getDeviceMemory()->isMapped())
	{
		buffer->getDeviceMemory()->map(&memory, 0, dataSize);
		unmap = true;
	}
	else
	{
		memory = buffer->getDeviceMemory()->getMappedData();
	}
	data = static_cast<unsigned char*>(memory);
	memcpy(outBuffer, data, dataSize);
	buffer->getDeviceMemory()->invalidateRange(0, dataSize);
	if (unmap)
	{
		buffer->getDeviceMemory()->unmap();
	}
}
} // namespace

bool takeScreenshot(Swapchain& swapChain, const uint32_t swapIndex, CommandPool& cmdPool, Queue& queue, const std::string& screenshotFileName, vma::Allocator* bufferAllocator,
	vma::Allocator* imageAllocator, const uint32_t screenshotScale)
{
	if (!swapChain->supportsUsage(pvrvk::ImageUsageFlags::e_TRANSFER_SRC_BIT))
	{
		Log(LogLevel::Warning, "Could not take screenshot as the swapchain does not support TRANSFER_SRC_BIT");
		return false;
	}
	// force the queue to wait idle prior to taking a copy of the swap chain image
	queue->waitIdle();

	saveImage(swapChain->getImage(swapIndex), pvrvk::ImageLayout::e_PRESENT_SRC_KHR, pvrvk::ImageLayout::e_PRESENT_SRC_KHR, cmdPool, queue, screenshotFileName, bufferAllocator,
		imageAllocator, screenshotScale);
	return true;
}

void saveImage(pvrvk::Image image, const pvrvk::ImageLayout imageInitialLayout, const pvrvk::ImageLayout imageFinalLayout, pvrvk::CommandPool& pool, pvrvk::Queue& queue,
	const std::string& filename, vma::Allocator* bufferAllocator, vma::Allocator* imageAllocator, const uint32_t screenshotScale)
{
	const Extent2D dim(image->getWidth(), image->getHeight());
	const uint32_t stride = 4;
	std::vector<char> buffer(dim.width * dim.height * stride);
	screenCaptureRegion(image->getDevice(), image, pool, queue, 0, 0, dim.width, dim.height, buffer.data(), stride, image->getFormat(), imageInitialLayout, imageFinalLayout,
		bufferAllocator, imageAllocator);
	Log(LogLevel::Information, "Writing TGA screenshot, filename %s.", filename.c_str());
	writeTGA(filename.c_str(), dim.width, dim.height, reinterpret_cast<const unsigned char*>(buffer.data()), 4, screenshotScale);
}

void updateImage(Device& device, CommandBufferBase cbuffTransfer, ImageUpdateInfo* updateInfos, uint32_t numUpdateInfos, pvrvk::Format format, pvrvk::ImageLayout layout,
	bool isCubeMap, Image& image, vma::Allocator* bufferAllocator)
{
	using namespace vma;
	if (!(cbuffTransfer.isValid() && cbuffTransfer->isRecording()))
	{
		throw ErrorValidationFailedEXT("updateImage - Commandbuffer must be valid and in recording state");
	}

	uint32_t numFace = (isCubeMap ? 6 : 1);

	uint32_t hwSlice;
	std::vector<Buffer> stagingBuffers;

	{
		cbuffTransfer->debugMarkerBeginEXT("PVRUtilsVk::updateImage");

		stagingBuffers.resize(numUpdateInfos);
		BufferImageCopy imgcp = {};

		for (uint32_t i = 0; i < numUpdateInfos; ++i)
		{
			const ImageUpdateInfo& mipLevelUpdate = updateInfos[i];
			assertion(mipLevelUpdate.data && mipLevelUpdate.dataSize, "Data and Data size must be valid");

			hwSlice = mipLevelUpdate.arrayIndex * numFace + mipLevelUpdate.cubeFace;

			// Will write the switch layout commands from the universal queue to the transfer queue to both the
			// transfer command buffer and the universal command buffer
			setImageLayoutAndQueueFamilyOwnership(CommandBufferBase(), cbuffTransfer, static_cast<uint32_t>(-1), static_cast<uint32_t>(-1), pvrvk::ImageLayout::e_UNDEFINED,
				pvrvk::ImageLayout::e_TRANSFER_DST_OPTIMAL, image, mipLevelUpdate.mipLevel, 1, hwSlice, 1, inferAspectFromFormat(format));

			// Create a staging buffer to use as the source of a copyBufferToImage
			stagingBuffers[i] = createBuffer(device, mipLevelUpdate.dataSize, pvrvk::BufferUsageFlags::e_TRANSFER_SRC_BIT, pvrvk::MemoryPropertyFlags::e_HOST_VISIBLE_BIT,
				pvrvk::MemoryPropertyFlags::e_HOST_VISIBLE_BIT, bufferAllocator, vma::AllocationCreateFlags::e_MAPPED_BIT);

			stagingBuffers[i]->setObjectName("PVRUtilsVk::updateImage::Temporary Image Upload Buffer");
			imgcp.setImageOffset(pvrvk::Offset3D(mipLevelUpdate.offsetX, mipLevelUpdate.offsetY, mipLevelUpdate.offsetZ));
			imgcp.setImageExtent(pvrvk::Extent3D(mipLevelUpdate.imageWidth, mipLevelUpdate.imageHeight, 1));

			imgcp.setImageSubresource(pvrvk::ImageSubresourceLayers(inferAspectFromFormat(format), updateInfos[i].mipLevel, hwSlice, 1));
			imgcp.setBufferRowLength(mipLevelUpdate.dataWidth);
			imgcp.setBufferImageHeight(mipLevelUpdate.dataHeight);

			const uint8_t* srcData;
			uint32_t srcDataSize;
			srcData = static_cast<const uint8_t*>(mipLevelUpdate.data);
			srcDataSize = mipLevelUpdate.dataSize;

			updateHostVisibleBuffer(stagingBuffers[i], srcData, 0, srcDataSize, true);

			cbuffTransfer->copyBufferToImage(stagingBuffers[i], image, pvrvk::ImageLayout::e_TRANSFER_DST_OPTIMAL, 1, &imgcp);

			// CAUTION: We swapped src and dst queue families as, if there was no ownership transfer, no problem - queue families
			// will be ignored.
			// Will write the switch layout commands from the transfer queue to the universal queue to both the
			// transfer command buffer and the universal command buffer
			setImageLayoutAndQueueFamilyOwnership(cbuffTransfer, CommandBufferBase(), static_cast<uint32_t>(-1), static_cast<uint32_t>(-1),
				pvrvk::ImageLayout::e_TRANSFER_DST_OPTIMAL, layout, image, mipLevelUpdate.mipLevel, 1, hwSlice, 1, inferAspectFromFormat(format));
		}
		cbuffTransfer->debugMarkerEndEXT();
	}
}

void create3dPlaneMesh(uint32_t width, uint32_t depth, bool generateTexCoords, bool generateNormalCoords, assets::Mesh& outMesh)
{
	const float halfWidth = width * .5f;
	const float halfDepth = depth * .5f;

	glm::vec3 normal[4] = { glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f) };

	glm::vec2 texCoord[4] = {
		glm::vec2(0.0f, 1.0f),
		glm::vec2(0.0f, 0.0f),
		glm::vec2(1.0f, 0.0f),
		glm::vec2(1.0f, 1.0f),
	};

	glm::vec3 pos[4] = { glm::vec3(-halfWidth, 0.0f, -halfDepth), glm::vec3(-halfWidth, 0.0f, halfDepth), glm::vec3(halfWidth, 0.0f, halfDepth),
		glm::vec3(halfWidth, 0.0f, -halfDepth) };

	uint32_t indexData[] = { 0, 1, 2, 0, 2, 3 };

	float vertData[32];
	uint32_t offset = 0;

	for (uint32_t i = 0; i < 4; ++i)
	{
		memcpy(&vertData[offset], &pos[i], sizeof(pos[i]));
		offset += 3;
		if (generateNormalCoords)
		{
			memcpy(&vertData[offset], &normal[i], sizeof(normal[i]));
			offset += 3;
		}
		if (generateTexCoords)
		{
			memcpy(&vertData[offset], &texCoord[i], sizeof(texCoord[i]));
			offset += 2;
		}
	}

	uint32_t stride = sizeof(glm::vec3) + (generateNormalCoords ? sizeof(glm::vec3) : 0) + (generateTexCoords ? sizeof(glm::vec2) : 0);

	outMesh.addData(reinterpret_cast<const uint8_t*>(vertData), sizeof(vertData), stride, 0);
	outMesh.addFaces(reinterpret_cast<const uint8_t*>(indexData), sizeof(indexData), IndexType::IndexType32Bit);
	offset = 0;
	outMesh.addVertexAttribute("POSITION", DataType::Float32, 3, offset, 0);
	offset += sizeof(float) * 3;
	if (generateNormalCoords)
	{
		outMesh.addVertexAttribute("NORMAL", DataType::Float32, 3, offset, 0);
		offset += sizeof(float) * 2;
	}
	if (generateTexCoords)
	{
		outMesh.addVertexAttribute("UV0", DataType::Float32, 2, offset, 0);
	}
	outMesh.setPrimitiveType(PrimitiveTopology::TriangleList);
	outMesh.setStride(0, stride);
	outMesh.setNumFaces(ARRAY_SIZE(indexData) / 3);
	outMesh.setNumVertices(ARRAY_SIZE(pos));
}
namespace {
inline static bool areQueueFamiliesSameOrInvalid(uint32_t lhs, uint32_t rhs)
{
	debug_assertion((lhs != -1 && rhs != -1) || (lhs == rhs),
		"ImageUtilsVK(areQueueFamiliesSameOrInvalid): Only one queue family was valid. "
		"Either both must be valid, or both must be ignored (-1)"); // Don't pass one non-null only...
	return lhs == rhs || lhs == uint32_t(-1) || rhs == uint32_t(-1);
}
inline static bool isMultiQueue(uint32_t queueFamilySrc, uint32_t queueFamilyDst)
{
	return !areQueueFamiliesSameOrInvalid(queueFamilySrc, queueFamilyDst);
}

inline pvrvk::AccessFlags getAccesFlagsFromLayout(pvrvk::ImageLayout layout)
{
	switch (layout)
	{
	case pvrvk::ImageLayout::e_GENERAL:
		return pvrvk::AccessFlags::e_SHADER_READ_BIT | pvrvk::AccessFlags::e_SHADER_WRITE_BIT | pvrvk::AccessFlags::e_COLOR_ATTACHMENT_READ_BIT |
			pvrvk::AccessFlags::e_COLOR_ATTACHMENT_WRITE_BIT;
	case pvrvk::ImageLayout::e_COLOR_ATTACHMENT_OPTIMAL:
		return pvrvk::AccessFlags::e_COLOR_ATTACHMENT_READ_BIT | pvrvk::AccessFlags::e_COLOR_ATTACHMENT_WRITE_BIT;
	case pvrvk::ImageLayout::e_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		return pvrvk::AccessFlags::e_DEPTH_STENCIL_ATTACHMENT_READ_BIT | pvrvk::AccessFlags::e_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	case pvrvk::ImageLayout::e_TRANSFER_DST_OPTIMAL:
		return pvrvk::AccessFlags::e_TRANSFER_WRITE_BIT;
	case pvrvk::ImageLayout::e_TRANSFER_SRC_OPTIMAL:
		return pvrvk::AccessFlags::e_TRANSFER_READ_BIT;
	case pvrvk::ImageLayout::e_SHADER_READ_ONLY_OPTIMAL:
		return pvrvk::AccessFlags::e_SHADER_READ_BIT;
	case pvrvk::ImageLayout::e_PRESENT_SRC_KHR:
		return pvrvk::AccessFlags::e_MEMORY_READ_BIT;
	case pvrvk::ImageLayout::e_PREINITIALIZED:
		return pvrvk::AccessFlags::e_HOST_WRITE_BIT;
	default:
		return (pvrvk::AccessFlags)0;
	}
}
} // namespace

void setImageLayoutAndQueueFamilyOwnership(CommandBufferBase srccmd, CommandBufferBase dstcmd, uint32_t srcQueueFamily, uint32_t dstQueueFamily, pvrvk::ImageLayout oldLayout,
	pvrvk::ImageLayout newLayout, Image image, uint32_t baseMipLevel, uint32_t numMipLevels, uint32_t baseArrayLayer, uint32_t numArrayLayers, pvrvk::ImageAspectFlags aspect)
{
	bool multiQueue = isMultiQueue(srcQueueFamily, dstQueueFamily);

	// No operation required: We don't have a layout transition, and we don't have a queue family change.
	if (newLayout == oldLayout && !multiQueue)
	{
		return;
	} // No transition required

	if (multiQueue)
	{
		assertion(srccmd.isValid() && dstcmd.isValid(),
			"Vulkan Utils setImageLayoutAndQueueOwnership: An ownership change was required, "
			"but at least one null command buffers was passed as parameters");
	}
	else
	{
		assertion(srccmd.isNull() || dstcmd.isNull(),
			"Vulkan Utils setImageLayoutAndQueueOwnership: An ownership change was not required, "
			"but two non-null command buffers were passed as parameters");
	}
	MemoryBarrierSet barriers;

	ImageMemoryBarrier imageMemBarrier;
	imageMemBarrier.setOldLayout(oldLayout);
	imageMemBarrier.setNewLayout(newLayout);
	imageMemBarrier.setImage(image);
	imageMemBarrier.setSubresourceRange(pvrvk::ImageSubresourceRange(aspect, baseMipLevel, numMipLevels, baseArrayLayer, numArrayLayers));
	imageMemBarrier.setSrcQueueFamilyIndex(static_cast<uint32_t>(-1));
	imageMemBarrier.setDstQueueFamilyIndex(static_cast<uint32_t>(-1));
	imageMemBarrier.setSrcAccessMask(getAccesFlagsFromLayout(oldLayout));
	imageMemBarrier.setDstAccessMask(getAccesFlagsFromLayout(newLayout));

	if (multiQueue)
	{
		imageMemBarrier.setSrcQueueFamilyIndex(srcQueueFamily);
		imageMemBarrier.setDstQueueFamilyIndex(dstQueueFamily);
	}
	barriers.clearAllBarriers();
	// Support any one of the command buffers being NOT null - either first or second is fine.
	if (srccmd.isValid())
	{
		barriers.addBarrier(imageMemBarrier);
		srccmd->pipelineBarrier(pvrvk::PipelineStageFlags::e_ALL_COMMANDS_BIT, pvrvk::PipelineStageFlags::e_ALL_COMMANDS_BIT, barriers, true);
	}
	if (dstcmd.isValid())
	{
		barriers.addBarrier(imageMemBarrier);
		dstcmd->pipelineBarrier(pvrvk::PipelineStageFlags::e_ALL_COMMANDS_BIT, pvrvk::PipelineStageFlags::e_ALL_COMMANDS_BIT, barriers, true);
	}
} // namespace utils

// The application defined callback used as the callback function specified in as pfnCallback in the
// create info VkDebugReportCallbackCreateInfoEXT used when creating the debug report callback vkCreateDebugReportCallbackEXT
VKAPI_ATTR VkBool32 VKAPI_CALL throwOnErrorDebugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location,
	int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
	(void)object;
	(void)location;
	(void)messageCode;
	(void)pLayerPrefix;
	(void)pUserData;

	// throw an exception if the type of VkDebugReportFlagsEXT contains the ERROR_BIT
	if ((static_cast<pvrvk::DebugReportFlagsEXT>(flags) & (pvrvk::DebugReportFlagsEXT::e_ERROR_BIT_EXT)) != pvrvk::DebugReportFlagsEXT(0))
	{
		throw pvrvk::ErrorValidationFailedEXT(
			std::string(pvrvk::to_string(static_cast<pvrvk::DebugReportObjectTypeEXT>(objectType)) + std::string(". VULKAN_LAYER_VALIDATION: ") + pMessage));
	}
	return VK_FALSE;
}

// The application defined callback used as the callback function specified in as pfnCallback in the
// create info VkDebugReportCallbackCreateInfoEXT used when creating the debug report callback vkCreateDebugReportCallbackEXT
VKAPI_ATTR VkBool32 VKAPI_CALL logMessageDebugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location,
	int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
	(void)object;
	(void)location;
	(void)messageCode;
	(void)pLayerPrefix;
	(void)pUserData;
	// map the VkDebugReportFlagsEXT to a suitable log type
	// map the VkDebugReportObjectTypeEXT to a stringified representation
	// Log the message generated by a lower layer
	Log(mapValidationTypeToLogLevel(static_cast<pvrvk::DebugReportFlagsEXT>(flags)),
		std::string(pvrvk::to_string(static_cast<pvrvk::DebugReportObjectTypeEXT>(objectType)) + std::string(". VULKAN_LAYER_VALIDATION: %s")).c_str(), pMessage);

	return VK_FALSE;
}

pvrvk::DebugReportCallback createDebugReportCallback(pvrvk::Instance& instance, pvrvk::DebugReportFlagsEXT flags, PFN_vkDebugReportCallbackEXT callback, void* userData)
{
	DebugReportCallbackCreateInfo createInfo(flags, callback, userData);
	return instance->createDebugReportCallback(createInfo);
}

pvrvk::Instance createInstance(const std::string& applicationName, VulkanVersion version, const InstanceExtensions& instanceExtensions, const InstanceLayers& instanceLayers)
{
	InstanceCreateInfo instanceInfo;
	ApplicationInfo appInfo;
	instanceInfo.setApplicationInfo(&appInfo);
	appInfo.setApplicationName(applicationName);
	appInfo.setApplicationVersion(1);
	appInfo.setEngineName("PVRVk");
	appInfo.setEngineVersion(0);

	// Retrieve the vulkan bindings
	VkBindings vkBindings;
	if (!initVkBindings(&vkBindings))
	{
		throw pvrvk::ErrorInitializationFailed("We were unable to retrieve Vulkan bindings");
	}

	uint32_t major = -1;
	uint32_t minor = -1;
	uint32_t patch = -1;

	// If a valid function pointer for vkEnumerateInstanceVersion cannot be retrieved then Vulkan only 1.0 is supported by the implementation otherwise we can use
	// vkEnumerateInstanceVersion to determine the api version supported.
	if (vkBindings.vkEnumerateInstanceVersion)
	{
		uint32_t supportedApiVersion;
		vkBindings.vkEnumerateInstanceVersion(&supportedApiVersion);

		major = VK_VERSION_MAJOR(supportedApiVersion);
		minor = VK_VERSION_MINOR(supportedApiVersion);
		patch = VK_VERSION_PATCH(supportedApiVersion);

		Log(LogLevel::Information, "The function pointer for 'vkEnumerateInstanceVersion' was valid. Supported instance version: ([%d].[%d].[%d]).", major, minor, patch);
	}
	else
	{
		major = 1;
		minor = 0;
		patch = 0;
		Log(LogLevel::Information, "Could not find a function pointer for 'vkEnumerateInstanceVersion'. Setting instance version to: ([%d].[%d].[%d]).", major, minor, patch);
	}

	if (instanceExtensions.extensionStrings.size())
	{
		instanceInfo.setEnabledExtensions(
			pvrvk::Extensions::filterInstanceExtensions(instanceExtensions.extensionStrings.data(), static_cast<uint32_t>(instanceExtensions.extensionStrings.size())));

		Log(LogLevel::Information, "Supported Instance Extensions:");
		for (uint32_t i = 0; i < instanceInfo.getNumEnabledExtensionNames(); ++i)
		{
			Log(LogLevel::Information, "\t%s", instanceInfo.getEnabledExtensionName(i).c_str());
		}
	}

	std::vector<LayerProperties> layerProperties;
	pvrvk::Layers::Instance::enumerateInstanceLayers(layerProperties);

	if (instanceLayers.layersStrings.size())
	{
		std::vector<std::string> supportedLayers =
			pvrvk::Layers::filterLayers(layerProperties, instanceLayers.layersStrings.data(), static_cast<uint32_t>(instanceLayers.layersStrings.size()));

		bool requestedStdValidation = false;
		bool supportsStdValidation = false;
		int stdValidationRequiredIndex = -1;

		for (uint32_t i = 0; i < static_cast<uint32_t>(instanceLayers.layersStrings.size()); ++i)
		{
			if (!strcmp(instanceLayers.layersStrings[i].c_str(), "VK_LAYER_LUNARG_standard_validation"))
			{
				requestedStdValidation = true;
				break;
			}
		}

		if (requestedStdValidation)
		{
			// This code is to cover cases where VK_LAYER_LUNARG_standard_validation is requested but is not supported, where on some platforms the
			// component layers enabled via VK_LAYER_LUNARG_standard_validation may still be supported even though VK_LAYER_LUNARG_standard_validation is not.
			for (auto it = layerProperties.begin(); !supportsStdValidation && it != layerProperties.end(); ++it)
			{
				supportsStdValidation = !strcmp(it->getLayerName(), "VK_LAYER_LUNARG_standard_validation");
			}

			if (!supportsStdValidation)
			{
				for (uint32_t i = 0; stdValidationRequiredIndex == -1 && i < layerProperties.size(); ++i)
				{
					if (!strcmp(instanceLayers.layersStrings[i].c_str(), "VK_LAYER_LUNARG_standard_validation"))
					{
						stdValidationRequiredIndex = i;
					}
				}

				for (uint32_t j = 0; j < instanceLayers.layersStrings.size(); ++j)
				{
					if (stdValidationRequiredIndex == j && !supportsStdValidation)
					{
						const char* stdValComponents[] = { "VK_LAYER_GOOGLE_threading", "VK_LAYER_LUNARG_parameter_validation", "VK_LAYER_LUNARG_object_tracker",
							"VK_LAYER_LUNARG_core_validation", "VK_LAYER_GOOGLE_unique_objects" };
						for (uint32_t k = 0; k < sizeof(stdValComponents) / sizeof(stdValComponents[0]); ++k)
						{
							for (uint32_t i = 0; i < layerProperties.size(); ++i)
							{
								if (!strcmp(stdValComponents[k], layerProperties[i].getLayerName()))
								{
									supportedLayers.push_back(std::string(stdValComponents[k]));
									break;
								}
							}
						}
					}
				}

				// filter the layers again checking for support for the component layers enabled via VK_LAYER_LUNARG_standard_validation
				supportedLayers = pvrvk::Layers::filterLayers(layerProperties, supportedLayers.data(), static_cast<uint32_t>(supportedLayers.size()));
			}
		}

		instanceInfo.setEnabledLayers(supportedLayers);

		Log(LogLevel::Information, "Supported Instance Layers:");
		for (uint32_t i = 0; i < instanceInfo.getNumEnabledLayerNames(); ++i)
		{
			Log(LogLevel::Information, "\t%s", instanceInfo.getEnabledLayerName(i).c_str());
		}
	}

	version = VulkanVersion(major, minor, patch);
	appInfo.setApiVersion(version.toVulkanVersion());

	pvrvk::Instance outInstance = pvrvk::createInstance(instanceInfo);

	const ApplicationInfo* instanceAppInfo = outInstance->getInfo().getApplicationInfo();
	Log(LogLevel::Information, "Created Vulkan Instance:");
	Log(LogLevel::Information, "	Application Name: %s.", instanceAppInfo->getApplicationName().c_str());
	Log(LogLevel::Information, "	Application Version: %d.", instanceAppInfo->getApplicationVersion());
	Log(LogLevel::Information, "	Engine Name: %s.", instanceAppInfo->getEngineName().c_str());
	Log(LogLevel::Information, "	Engine Version: %d.", instanceAppInfo->getEngineVersion());
	Log(LogLevel::Information, "	Version: %d / ([%d].[%d].[%d]).", instanceAppInfo->getApiVersion(), major, minor, patch);

	const std::vector<pvrvk::PhysicalDevice>& physicalDevices = outInstance->getPhysicalDevices();

	Log(LogLevel::Information, "Supported Vulkan Physical devices:");

	for (uint32_t i = 0; i < physicalDevices.size(); ++i)
	{
		pvrvk::PhysicalDeviceProperties physicalDeviceProperties = physicalDevices[i]->getProperties();

		uint32_t deviceMajor = VK_VERSION_MAJOR(physicalDeviceProperties.getApiVersion());
		uint32_t deviceMinor = VK_VERSION_MINOR(physicalDeviceProperties.getApiVersion());
		uint32_t devicePatch = VK_VERSION_PATCH(physicalDeviceProperties.getApiVersion());

		Log(LogLevel::Information, "	Device Name: %s.", physicalDeviceProperties.getDeviceName());
		Log(LogLevel::Information, "	Device ID: %d.", physicalDeviceProperties.getDeviceID());
		Log(LogLevel::Information, "	Api Version Supported: %d / ([%d].[%d].[%d]).", physicalDeviceProperties.getApiVersion(), deviceMajor, deviceMinor, devicePatch);
		Log(LogLevel::Information, "	Device Type: %s.", pvrvk::to_string(physicalDeviceProperties.getDeviceType()).c_str());
		Log(LogLevel::Information, "	Driver version: %d.", physicalDeviceProperties.getDriverVersion());
		Log(LogLevel::Information, "	Vendor ID: %d.", physicalDeviceProperties.getVendorID());

		Log(LogLevel::Information, "	Memory Configuration:");
		auto memprop = physicalDevices[i]->getMemoryProperties();

		for (int heapIdx = 0; heapIdx < memprop.getMemoryHeapCount(); ++heapIdx)
		{
			auto heap = memprop.getMemoryHeaps()[heapIdx];
			Log(LogLevel::Information, "		Heap:[%d] Size:[%d] Flags: [%d (%s) ]", heapIdx, heap.getSize(), heap.getFlags(), to_string(heap.getFlags()).c_str());
			for (int typeIdx = 0; typeIdx < memprop.getMemoryTypeCount(); ++typeIdx)
			{
				auto type = memprop.getMemoryTypes()[typeIdx];
				if (type.getHeapIndex() == heapIdx)
					Log(LogLevel::Information, "			Memory Type: [%d] Flags: [%d (%s) ] ", typeIdx, type.getPropertyFlags(), to_string(type.getPropertyFlags()).c_str());
			}
		}
	}
	return outInstance;
} // namespace utils

pvrvk::Surface createSurface(pvrvk::Instance& instance, pvrvk::PhysicalDevice& physicalDevice, void* window, void* display)
{
	(void)physicalDevice; // hide warning
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	display;
	return pvrvk::Surface(instance->createAndroidSurface(reinterpret_cast<ANativeWindow*>(window)));
#elif defined VK_USE_PLATFORM_WIN32_KHR
	return pvrvk::Surface(instance->createWin32Surface(GetModuleHandle(NULL), static_cast<HWND>(window)));
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	typedef xcb_connection_t* (*PFN_XGetXCBConnection)(::Display*);
	void* dlHandle = dlopen("libX11-xcb.so", RTLD_LAZY);

	if (dlHandle)
	{
		PFN_XGetXCBConnection fn_XGetXCBConnection = (PFN_XGetXCBConnection)dlsym(dlHandle, "XGetXCBConnection");
		xcb_connection_t* connection = fn_XGetXCBConnection(static_cast< ::Display*>(display));
		dlclose(dlHandle);
		return pvrvk::Surface(instance->createXcbSurface(connection, reinterpret_cast<Window>(window)));
	}

	Log("Failed to dlopen libX11-xcb. Please check your libX11-xcb installation on the target system");

#if defined(VK_USE_PLATFORM_XLIB_KHR)
	if (instance->isInstanceExtensionEnabled(VK_KHR_XLIB_SURFACE_EXTENSION_NAME))
	{
		Log(LogLevel::Information, "Falling to xlib protocol");
		return pvrvk::Surface(instance->createXlibSurface(static_cast< ::Display*>(display), reinterpret_cast<Window>(window)));
	}
#endif
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
	if (instance->isInstanceExtensionEnabled(VK_KHR_XLIB_SURFACE_EXTENSION_NAME))
	{
		return pvrvk::Surface(instance->createXlibSurface(static_cast< ::Display*>(display), reinterpret_cast<Window>(window)));
	}
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
	if (instance->isInstanceExtensionEnabled(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME))
	{
		return pvrvk::Surface(instance->createWaylandSurface(reinterpret_cast<wl_display*>(display), reinterpret_cast<wl_surface*>(window)));
	}
#else // NullWS
	Log("%u Displays supported by the physical device", physicalDevice->getNumDisplays());
	Log("Display properties:");

	for (uint32_t i = 0; i < physicalDevice->getNumDisplays(); ++i)
	{
		const Display& display = physicalDevice->getDisplay(i);
		Log("Properties for Display [%u]:", i);
		Log("	Display Name: '%s':", display->getDisplayName());
		Log("	Supports Persistent Content: %u", display->getPersistentContent());
		Log("	Physical Dimensions: (%u, %u)", display->getPhysicalDimensions().getWidth(), display->getPhysicalDimensions().getHeight());
		Log("	Physical Resolution: (%u, %u)", display->getPhysicalResolution().getWidth(), display->getPhysicalResolution().getHeight());
		Log("	Supported Transforms: %s", pvrvk::to_string(display->getSupportedTransforms()).c_str());
		Log("	Supports Plane Reorder: %u", display->getPlaneReorderPossible());

		Log("	Display supports [%u] display modes:", display->getNumDisplayModes());
		for (uint32_t j = 0; j < display->getNumDisplayModes(); ++j)
		{
			Log("	Properties for Display Mode [%u]:", j);
			const DisplayMode& displayMode = display->getDisplayMode(j);
			Log("		Refresh Rate: %f", displayMode->getParameters().getRefreshRate());
			Log("		Visible Region: (%u, %u)", displayMode->getParameters().getVisibleRegion().getWidth(), displayMode->getParameters().getVisibleRegion().getHeight());
		}
	}

	if (physicalDevice->getNumDisplays() == 0)
	{
		throw pvrvk::ErrorInitializationFailed("Could not find a suitable Vulkan Display.");
	}

	// We simply loop through the display planes and find a supported display and display mode
	for (uint32_t i = 0; i < physicalDevice->getNumDisplayPlanes(); ++i)
	{
		uint32_t currentStackIndex = -1;
		pvrvk::Display display = physicalDevice->getDisplayPlaneProperties(i, currentStackIndex);
		std::vector<pvrvk::Display> supportedDisplaysForPlane = physicalDevice->getDisplayPlaneSupportedDisplays(i);
		DisplayMode displayMode;

		// if a valid display can be found and its supported then make use of it
		if (display.isValid() && std::find(supportedDisplaysForPlane.begin(), supportedDisplaysForPlane.end(), display) != supportedDisplaysForPlane.end())
		{
			displayMode = display->getDisplayMode(0);
		}
		// else find the first supported display and grab its first display mode
		else if (supportedDisplaysForPlane.size())
		{
			pvrvk::Display& currentDisplay = supportedDisplaysForPlane[0];
			displayMode = currentDisplay->getDisplayMode(0);
		}

		if (displayMode.isValid())
		{
			DisplayPlaneCapabilitiesKHR capabilities = physicalDevice->getDisplayPlaneCapabilities(displayMode, i);
			Log("Capabilities for the chosen display mode for Display Plane [%u]:", i);
			Log("	Supported Alpha Flags: %s", pvrvk::to_string(capabilities.getSupportedAlpha()).c_str());
			Log("	Supported Min Src Position: (%u, %u)", capabilities.getMinSrcPosition().getX(), capabilities.getMinSrcPosition().getY());
			Log("	Supported Max Src Position: (%u, %u)", capabilities.getMaxSrcPosition().getX(), capabilities.getMaxSrcPosition().getY());
			Log("	Supported Min Src Extent: (%u, %u)", capabilities.getMinSrcExtent().getWidth(), capabilities.getMinSrcExtent().getHeight());
			Log("	Supported Max Src Extent: (%u, %u)", capabilities.getMaxSrcExtent().getWidth(), capabilities.getMaxSrcExtent().getHeight());
			Log("	Supported Min Dst Position: (%u, %u)", capabilities.getMinDstPosition().getX(), capabilities.getMinDstPosition().getY());
			Log("	Supported Max Dst Position: (%u, %u)", capabilities.getMaxDstPosition().getX(), capabilities.getMaxDstPosition().getY());
			Log("	Supported Min Dst Extent: (%u, %u)", capabilities.getMinDstExtent().getWidth(), capabilities.getMinDstExtent().getHeight());
			Log("	Supported Max Dst Extent: (%u, %u)", capabilities.getMaxDstExtent().getWidth(), capabilities.getMaxDstExtent().getHeight());

			return pvrvk::Surface(
				instance->createDisplayPlaneSurface(displayMode, displayMode->getParameters().getVisibleRegion(), pvrvk::DisplaySurfaceCreateFlagsKHR::e_NONE, i, currentStackIndex));
		}
	}
#endif

	throw pvrvk::ErrorInitializationFailed("We were unable to create a suitable Surface for the given physical device.");
} // namespace pvr

uint32_t numberOfSetBits(uint32_t bits)
{
	bits = bits - ((bits >> 1) & 0x55555555);
	bits = (bits & 0x33333333) + ((bits >> 2) & 0x33333333);
	return (((bits + (bits >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

void getMemoryTypeIndex(const pvrvk::PhysicalDevice& physicalDevice, const uint32_t allowedMemoryTypeBits, const pvrvk::MemoryPropertyFlags requiredMemoryProperties,
	const pvrvk::MemoryPropertyFlags optimalMemoryProperties, uint32_t& outMemoryTypeIndex, pvrvk::MemoryPropertyFlags& outMemoryPropertyFlags)
{
	// attempt to find a memory type index which supports the optimal set of memory property flags
	pvrvk::MemoryPropertyFlags memoryPropertyFlags = optimalMemoryProperties;

	// ensure that the optimal set of memory property flags is a superset of the required set of memory property flags.
	// This also handles cases where the optimal set of memory property flags hasn't been set but the required set has
	memoryPropertyFlags |= requiredMemoryProperties;

	uint32_t minCost = std::numeric_limits<unsigned int>::max();

	// iterate through each memory type supported by the physical device and attempt to find the best possible memory type supporting as many of the optimal bits as possible
	for (uint32_t memoryIndex = 0; memoryIndex < physicalDevice->getMemoryProperties().getMemoryTypeCount(); ++memoryIndex)
	{
		const uint32_t memoryTypeBits = (1 << memoryIndex);
		// ensure the memory type is compatible with the require memory for the given allocation
		const bool isRequiredMemoryType = static_cast<uint32_t>(allowedMemoryTypeBits & memoryTypeBits) != 0;

		if (isRequiredMemoryType)
		{
			const pvrvk::MemoryPropertyFlags currentMemoryPropertyFlags = physicalDevice->getMemoryProperties().getMemoryTypes()[memoryIndex].getPropertyFlags();
			// ensure the memory property flags for the current memory type supports the required set of memory property flags
			const bool hasRequiredProperties = static_cast<uint32_t>(currentMemoryPropertyFlags & requiredMemoryProperties) == requiredMemoryProperties;
			if (hasRequiredProperties)
			{
				// calculate a cost value based on the number of bits from the optimal set of bits which are not present in the current memory type
				uint32_t currentCost = numberOfSetBits(static_cast<uint32_t>(memoryPropertyFlags & ~currentMemoryPropertyFlags));

				// update the return values if the current cost is less than the current maximum cost value
				if (currentCost < minCost)
				{
					outMemoryTypeIndex = static_cast<uint32_t>(memoryIndex);
					outMemoryPropertyFlags = currentMemoryPropertyFlags;

					// early return if we have a perfect match
					if (currentCost == 0)
					{
						return;
					}
					// keep track of the current minimum cost
					minCost = currentCost;
				}
			}
		}
	}
}

InstanceExtensions::InstanceExtensions()
{
#ifdef VK_KHR_surface
	extensionStrings.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#endif
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	extensionStrings.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined VK_USE_PLATFORM_WIN32_KHR
	extensionStrings.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	extensionStrings.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
	extensionStrings.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
	extensionStrings.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#elif defined(VK_KHR_display) // NullWS
	extensionStrings.push_back(VK_KHR_DISPLAY_EXTENSION_NAME);
#endif
#ifdef VK_KHR_get_physical_device_properties2
	extensionStrings.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif

	// if the build is Debug then enable the VK_EXT_debug_report extension to aid with debugging
#ifdef DEBUG
#ifdef VK_EXT_debug_report
	extensionStrings.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif
#endif
}

} // namespace utils
} // namespace pvr
  //!\endcond
