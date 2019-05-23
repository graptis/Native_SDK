/*!
\brief The Device Memory class, a class representing a memory block managed by Vulkan
\file PVRVk/DeviceMemoryVk.h
\author PowerVR by Imagination, Developer Technology Team
\copyright Copyright (c) Imagination Technologies Limited.
*/

#pragma once
#include "PVRVk/DeviceVk.h"
namespace pvrvk {

/// <summary> Implementation of the VkExportMemoryAllocateInfoKHR  class used by the VK_KHR_external_memory extension</summary>
struct ExportMemoryAllocateInfoKHR
{
	ExternalMemoryHandleTypeFlags handleTypes;//!< <summary> HandleTypes member </summary>
	/// <summary> Constructor. handleTypes are initialized to zero. </summary>
	ExportMemoryAllocateInfoKHR() : handleTypes(ExternalMemoryHandleTypeFlags::e_NONE) {}
	/// <summary> Constructor. handleTypes are initialized to the parameter passed. </summary>
	/// <param name="handleTypes"> The ExternalMemoryHandleTypeFlags used to initialise handleTypes. </param>
	ExportMemoryAllocateInfoKHR(ExternalMemoryHandleTypeFlags handleTypes) : handleTypes(handleTypes) {}
};

/// <summary> Parameter object used to create a Memory Allocation using the VMA</summary>
struct MemoryAllocationInfo
{
public:
	/// <summary> Constructor. Initialized to unknown values (size 0, index -1)</summary>
	MemoryAllocationInfo() : _allocationSize(0), _memoryTypeIndex(uint32_t(-1)) {}
	/// <summary> Constructor. Initialized by user=provided values</summary>
	/// <param name="allocationSize"> The total size of the allocation, in bytes.</param>
	/// <param name="memoryTypeIndex"> The index of the Vulkan memory type required. (retrieve using the various get memory property functions)</param>
	MemoryAllocationInfo(DeviceSize allocationSize, uint32_t memoryTypeIndex) : _allocationSize(allocationSize), _memoryTypeIndex(memoryTypeIndex) {}

	/// <summary> Retrieve the size of the allocation.</summary>
	/// <returns> The size of the allocation)</summary>
	DeviceSize getAllocationSize() const
	{
		return _allocationSize;
	}
	/// <summary> Set the size of the allocation.</summary>
	/// <param name="sizeInBytes"> The total size of the allocation.</param>
	void setAllocationSize(DeviceSize sizeInBytes)
	{
		_allocationSize = sizeInBytes;
	}

	/// <summary> Retrieve the Memory Index of the  memory type of the allocation.</summary>
	/// <returns> The index of the memory type of the allocation)</summary>
	uint32_t getMemoryTypeIndex() const
	{
		return _memoryTypeIndex;
	}

	/// <summary> Set the memory index of the allocation.</summary>
	/// <param name="memoryTypeIndex"> The memory index of the allocation.</param>
	void setMemoryTypeIndex(uint32_t memoryTypeIndex)
	{
		this->_memoryTypeIndex = memoryTypeIndex;
	}

	/// <summary> Set this field in order to use the VK_KHR_external_memory extension.</summary>
	/// <param name="info"> The ExportMemoryAllocateInfoKHR object describing the source of the allocation.</param>
	void setExportMemoryAllocationInfoKHR(const ExportMemoryAllocateInfoKHR& info)
	{
		_exportMemoryAllocateInfoKHR = info;
	}

	/// <summary> Get the ExportMemoryAllocateInfoKHR object used.</summary>
	/// <returns> The ExportMemoryAllocateInfoKHR object contained.</returns>
	const ExportMemoryAllocateInfoKHR& getExportMemoryAllocateInfoKHR() const
	{
		return _exportMemoryAllocateInfoKHR;
	}
	/// <summary> Get the ExportMemoryAllocateInfoKHR object used.</summary>
	/// <returns> The ExportMemoryAllocateInfoKHR object contained.</returns>
	ExportMemoryAllocateInfoKHR& getExportMemoryAllocateInfoKHR()
	{
		return _exportMemoryAllocateInfoKHR;
	}

private:
	DeviceSize _allocationSize;
	uint32_t _memoryTypeIndex;
	ExportMemoryAllocateInfoKHR _exportMemoryAllocateInfoKHR;
};

namespace impl {
/// <summary>VkDeviceMemory wrapper</summary>
class IDeviceMemory_ : public DeviceObjectHandle<VkDeviceMemory>
{
public:
	/// <summary>Constructor.</summary>
	IDeviceMemory_() : DeviceObjectHandle() {}

	/// <summary>Constructor.</summary>
	/// <param name="device">The device to use for allocating the device memory</param>
	explicit IDeviceMemory_(DeviceWeakPtr device) : DeviceObjectHandle(device) {}

	/// <summary>Constructor.</summary>
	/// <param name="device">The device to use for allocating the device memory</param>
	/// <param name="memory">The vulkan device memory object</param>
	IDeviceMemory_(DeviceWeakPtr device, VkDeviceMemory memory) : DeviceObjectHandle(device, memory) {}

	virtual ~IDeviceMemory_() {}

	/// <summary>Return true if this memory block is mappable by the host (const).</summary>
	/// <returns>True is this memory block can be mapped, otherwise false.</returns>
	virtual bool isMappable() const = 0;

	/// <summary>.</summary>
	/// <returns>.</returns>
	virtual uint32_t getMemoryType() const = 0;

	/// <summary>Return the memory flags(const)</summary>
	/// <returns>The flags of the memory</returns>
	virtual pvrvk::MemoryPropertyFlags getMemoryFlags() const = 0;

	/// <summary>Indicates whether the device memory's property flags includes the given flag</summary>
	/// <param name="flags">A device memory property flag</param>
	/// <returns>True if the device memory's property flags includes the given flag</returns>
	bool hasPropertyFlag(pvrvk::MemoryPropertyFlags flags) const
	{
		return (getMemoryFlags() & flags) == flags;
	}

	/// <summary>Return this mapped memory offset (const)</summary>
	/// <returns>The offset of the mapped range, if any.</returns
	virtual VkDeviceSize getMappedOffset() const = 0;

	/// <summary>Return a pointer to the mapped memory</summary>
	/// <returns>Mapped memory</returns
	virtual void* getMappedData() = 0;

	/// <summary>Return this mapped memory size</summary>
	/// <returns>The size of the mapped range, if any.</returns
	virtual VkDeviceSize getMappedSize() const = 0;

	/// <summary>Return this memory size</summary>
	/// <returns>The memory size</returns>
	virtual VkDeviceSize getSize() const = 0;

	/// <summary>Return true if this memory is being mapped by the host</summary>
	/// <returns>True if memory is already mapped, otherwise false.</returns
	virtual bool isMapped() const = 0;

	/// <summary>
	/// map this memory. NOTE: Only memory created with HostVisible flag can be mapped and unmapped
	/// </summary>
	/// <param name="mappedMemory">Out mapped memory</param>
	/// <param name="offset">Zero-based byte offset from the beginning of the memory object.</param>
	/// <param name="size">Size of the memory range to map, or VK_WHOLE_SIZE to map from offset to the end of the allocation</param>
	virtual void map(void** mappedMemory, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) = 0;

	/// <summary>Unmap this memory block</summary>
	virtual void unmap() = 0;

	/// <summary>Flush ranges of non-coherent memory from the host caches:</summary>
	/// <param name="offset">Zero-based byte offset from the beginning of the memory object</param>
	/// <param name="size">Either the size of range, or VK_WHOLE_SIZE to affect the range from offset to the end of the current mapping of the allocation.</param>
	virtual void flushRange(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) = 0;

	/// <summary>To invalidate ranges of non-coherent memory from the host caches</summary>
	/// <param name="offset">Zero-based byte offset from the beginning of the memory object.</param>
	/// <param name="size">Either the size of range, or VK_WHOLE_SIZE to affect the range from offset to the end of the current mapping of the allocation.</param>
	virtual void invalidateRange(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) = 0;
};

/// <summary>VkDeviceMemory wrapper</summary>
class DeviceMemory_ : public IDeviceMemory_, public DeviceObjectDebugMarker<DeviceMemory_>
{
public:
	DECLARE_NO_COPY_SEMANTICS(DeviceMemory_)

	/// <summary>Return true if this memory block is mappable by the host (const).</summary>
	/// <returns>bool</returns>
	bool isMappable() const
	{
		return (static_cast<uint32_t>(_flags & pvrvk::MemoryPropertyFlags::e_HOST_VISIBLE_BIT) != 0) ||
			(static_cast<uint32_t>(_flags & pvrvk::MemoryPropertyFlags::e_HOST_COHERENT_BIT) != 0);
	}

	/// <summary>Return the memory flags(const)</summary>
	/// <returns>pvrvk::MemoryPropertyFlags</returns>
	pvrvk::MemoryPropertyFlags getMemoryFlags() const
	{
		return _flags;
	}

	/// <summary>Return this mapped memory offset (const)</summary>
	/// <returns>VkDeviceSize</returns
	VkDeviceSize getMappedOffset() const
	{
		return _mappedOffset;
	}

	/// <summary>Return a pointer to the mapped memory</summary>
	/// <returns>Mapped memory</returns
	virtual void* getMappedData()
	{
		return _mappedMemory;
	}

	/// <summary>Return this mapped memory size (const)</summary>
	/// <returns>VkDeviceSize</returns
	VkDeviceSize getMappedSize() const
	{
		return _mappedSize;
	}

	/// <summary>Return this memory size (const)</summary>
	/// <returns>uint64_t</returns>
	VkDeviceSize getSize() const
	{
		return _allocationInfo.getAllocationSize();
	}

	/// <summary>Return true if this memory is being mapped by the host (const)</summary>
	/// <returns>VkDeviceSize</returns
	bool isMapped() const
	{
		return _mappedSize > 0;
	}

	/// <summary>
	/// map this memory. NOTE: Only memory created with HostVisible flag can be mapped and unmapped
	/// </summary>
	/// <param name="mappedMemory">Out mapped memory</param>
	/// <param name="offset">Zero-based byte offset from the beginning of the memory object.</param>
	/// <param name="size">Size of the memory range to map, or VK_WHOLE_SIZE to map from offset to the end of the allocation</param>
	virtual void map(void** mappedMemory, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE)
	{
		assertion(mappedMemory != nullptr, "DeviceMemory::map - mappedMemory must be a valid pointer");
		if (!isMappable())
		{
			throw ErrorMemoryMapFailed("Cannot map memory block as the memory was created without "
									   "HOST_VISIBLE_BIT or HOST_COHERENT_BIT memory flags");
		}
		if (_mappedSize)
		{
			throw ErrorMemoryMapFailed("Cannot map memory block as the memory is already mapped");
		}
		if (size != VK_WHOLE_SIZE)
		{
			if (offset + size > getSize())
			{
				// throw VkErrorValidationFailedEXT("Cannot map map memory block 0x%ullx"
				//    " - Attempting to map offset (0x%ullx) + size (0x%ullx) range greater than the memory block size", getVkHandle(), offset, size);
				throw ErrorMemoryMapFailed("Cannot map map memory block : offset + size range greater than the memory block size");
			}
		}

		vkThrowIfFailed(
			_device->getVkBindings().vkMapMemory(_device->getVkHandle(), getVkHandle(), offset, size, static_cast<VkMemoryMapFlags>(pvrvk::MemoryMapFlags::e_NONE), &_mappedMemory),
			"Failed to map memory block");

		if (_mappedMemory == nullptr)
		{
			throw ErrorMemoryMapFailed("Failed to map memory block");
		}

		// store the mapped offset and mapped size
		*mappedMemory = _mappedMemory;
		_mappedOffset = offset;
		_mappedSize = size;
	}

	/// <summary>Unmap this memory block</summary>
	virtual void unmap()
	{
		if (!_mappedSize)
		{
			throw ErrorMemoryMapFailed("Cannot unmap memory block as the memory is not mapped");
		}

		_mappedSize = 0;
		_mappedOffset = 0;

		_device->getVkBindings().vkUnmapMemory(_device->getVkHandle(), getVkHandle());
		_mappedMemory = nullptr;
	}

	/// <summary>Flush ranges of non-coherent memory from the host caches:</summary>
	/// <param name="offset">Zero-based byte offset from the beginning of the memory object</param>
	/// <param name="size">Either the size of range, or VK_WHOLE_SIZE to affect the range from offset to the end of the current mapping of the allocation.</param>
	void flushRange(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE)
	{
		if (static_cast<uint32_t>(_flags & pvrvk::MemoryPropertyFlags::e_HOST_COHERENT_BIT) != 0)
		{
			Log(LogLevel::Warning,
				"Flushing memory block 0x%ullx"
				" created using HOST_COHERENT_BIT memory flags - this is unnecessary.",
				getVkHandle());
		}

		VkMappedMemoryRange range = {};
		range.sType = static_cast<VkStructureType>(StructureType::e_MAPPED_MEMORY_RANGE);
		range.memory = getVkHandle();
		range.offset = offset;
		range.size = size;
		vkThrowIfFailed(_device->getVkBindings().vkFlushMappedMemoryRanges(_device->getVkHandle(), 1, &range), "Failed to flush range of memory block");
	}

	/// <summary>To invalidate ranges of non-coherent memory from the host caches</summary>
	/// <param name="offset">Zero-based byte offset from the beginning of the memory object.</param>
	/// <param name="size">Either the size of range, or VK_WHOLE_SIZE to affect the range from offset to the end of the current mapping of the allocation.</param>
	/// <returns>VkResult</returns>
	void invalidateRange(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE)
	{
		if (static_cast<uint32_t>(_flags & MemoryPropertyFlags::e_HOST_COHERENT_BIT) != 0)
		{
			Log(LogLevel::Warning,
				"Invalidating range of memory block 0x%ullx"
				" created using HOST_COHERENT_BIT memory flags - this is unnecessary.",
				getVkHandle());
		}

		VkMappedMemoryRange range = {};
		range.sType = static_cast<VkStructureType>(StructureType::e_MAPPED_MEMORY_RANGE);
		range.memory = getVkHandle();
		range.offset = offset;
		range.size = size;
		vkThrowIfFailed(_device->getVkBindings().vkInvalidateMappedMemoryRanges(_device->getVkHandle(), 1, &range), "Failed to invalidate range of memory block");
	}

	uint32_t getMemoryType() const
	{
		return _allocationInfo.getMemoryTypeIndex();
	}

protected:
	template<typename>
	friend struct ::pvrvk::RefCountEntryIntrusive;
	friend class ::pvrvk::impl::Device_;

	virtual ~DeviceMemory_()
	{
		if (getVkHandle() != VK_NULL_HANDLE)
		{
			_device->getVkBindings().vkFreeMemory(_device->getVkHandle(), getVkHandle(), nullptr);
			_vkHandle = VK_NULL_HANDLE;
		}
	}

	/// <summary>Constructor. Allocates device memory with the given properties</summary>
	/// <param name="device">The device from which the memory allocation will be made.</param>
	/// <param name="allocationInfo">The memory allocation info</param>
	/// <param name="memPropFlags">The memory property flags the memory type index supports.</param>
	/// <param name="vkMemoryHandle">A Vulkan device memory object indicating whether an allocation has already been made.</param>
	DeviceMemory_(DeviceWeakPtr device, const MemoryAllocationInfo& allocationInfo, pvrvk::MemoryPropertyFlags memPropFlags, VkDeviceMemory vkMemoryHandle = VK_NULL_HANDLE)
		: IDeviceMemory_(device), DeviceObjectDebugMarker(pvrvk::DebugReportObjectTypeEXT::e_DEVICE_MEMORY_EXT), _flags(memPropFlags), _mappedOffset(0), _mappedSize(0),
		  _mappedMemory(nullptr)
	{
		_vkHandle = vkMemoryHandle;
		_allocationInfo = allocationInfo;
		if (vkMemoryHandle == VK_NULL_HANDLE)
		{
			allocateDeviceMemory(_device, allocationInfo, _vkHandle);
		}
	}

private:
	/// <summary>Allocates device memory with the given properties</summary>
	/// <param name="device">The device from which the memory allocation will be made.</param>
	/// <param name="allocationInfo">The memory  memory allocation info.</param>
	/// <param name="outMemory">The resulting Vulkan device memory object.</param>
	void allocateDeviceMemory(Device device, const MemoryAllocationInfo& allocationInfo, VkDeviceMemory& outMemory)
	{
		// allocate the memory
		VkMemoryAllocateInfo memAllocInfo = {};
		memAllocInfo.sType = static_cast<VkStructureType>(StructureType::e_MEMORY_ALLOCATE_INFO);
		memAllocInfo.pNext = nullptr;
		memAllocInfo.allocationSize = allocationInfo.getAllocationSize();
		memAllocInfo.memoryTypeIndex = allocationInfo.getMemoryTypeIndex();

		// handle extension
		VkExportMemoryAllocateInfoKHR memAllocateInfoKHR = {};
		if (allocationInfo.getExportMemoryAllocateInfoKHR().handleTypes != ExternalMemoryHandleTypeFlags::e_NONE)
		{
			memAllocateInfoKHR.sType = static_cast<VkStructureType>(StructureType::e_EXPORT_MEMORY_ALLOCATE_INFO_KHR);
			memAllocateInfoKHR.handleTypes = static_cast<VkExternalMemoryHandleTypeFlags>(allocationInfo.getExportMemoryAllocateInfoKHR().handleTypes);
			memAllocInfo.pNext = &memAllocateInfoKHR;
		}

		if (memAllocInfo.memoryTypeIndex == static_cast<uint32_t>(-1))
		{
			throw ErrorValidationFailedEXT("Device Memory allocation failed: Could not get a Memory Type Index for the specified combination specified memory bits, properties and "
										   "flags");
		}
		vkThrowIfFailed(_device->getVkBindings().vkAllocateMemory(device->getVkHandle(), &memAllocInfo, nullptr, &outMemory), "Failed to allocate device memory");
	}

	/// <summary>The memory property flags for the device memory</summary>
	pvrvk::MemoryPropertyFlags _flags;

	/// <summary>The offset into the memory currently being pointed to</summary>
	VkDeviceSize _mappedOffset;

	/// <summary>The size of the memory pointed to</summary>
	VkDeviceSize _mappedSize;

	MemoryAllocationInfo _allocationInfo;

	/// <summary>A pointer to the currently mapped memory</summary>
	void* _mappedMemory;
};
} // namespace impl
} // namespace pvrvk
