/*!
\brief Function definitions for Event class
\file PVRVk/EventVk.cpp
\author PowerVR by Imagination, Developer Technology Team
\copyright Copyright (c) Imagination Technologies Limited.
*/
#include "PVRVk/EventVk.h"
#include "PVRVk/BufferVk.h"
#include "PVRVk/ImageVk.h"

namespace pvrvk {
namespace impl {
Event_::Event_(const DeviceWeakPtr& device) : DeviceObjectHandle(device), DeviceObjectDebugMarker(DebugReportObjectTypeEXT::e_EVENT_EXT)
{
	VkEventCreateInfo nfo;
	nfo.sType = static_cast<VkStructureType>(StructureType::e_EVENT_CREATE_INFO);
	nfo.pNext = 0;
	nfo.flags = static_cast<VkEventCreateFlags>(EventCreateFlags::e_NONE);
	vkThrowIfFailed(_device->getVkBindings().vkCreateEvent(_device->getVkHandle(), &nfo, NULL, &_vkHandle), "EventVk_::init: Failed to create Semaphore object");
}

Event_::~Event_()
{
	if (getVkHandle() != VK_NULL_HANDLE)
	{
		if (_device.isValid())
		{
			_device->getVkBindings().vkDestroyEvent(_device->getVkHandle(), getVkHandle(), NULL);
			_vkHandle = VK_NULL_HANDLE;
			_device.reset();
		}
		else
		{
			reportDestroyedAfterDevice("Event");
		}
	}
}

void Event_::set()
{
	vkThrowIfFailed(_device->getVkBindings().vkSetEvent(_device->getVkHandle(), getVkHandle()), "Event::set returned an error");
}

void Event_::reset()
{
	vkThrowIfFailed(_device->getVkBindings().vkResetEvent(_device->getVkHandle(), getVkHandle()), "Event::reset returned an error");
}

bool Event_::isSet()
{
	Result res;
	vkThrowIfError(res = static_cast<pvrvk::Result>(_device->getVkBindings().vkGetEventStatus(_device->getVkHandle(), getVkHandle())), "Event::isSet returned an error");
	debug_assertion(res == Result::e_EVENT_SET || res == Result::e_EVENT_RESET, "Event::isSet returned a success code that was neither EVENT_SET or EVENT_RESET");
	return res == Result::e_EVENT_RESET;
}
} // namespace impl
} // namespace pvrvk
