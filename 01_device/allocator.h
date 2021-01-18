#ifndef VULKAN_ALLOCATOR_H
#define VULKAN_ALLOCATOR_H

/**
 * allocator.h
 *  - Manages all created vulkan objects and memory, and deallocates them automatically before device is destroyed
 */


#include "../00_base/vulkan_base.h"

#define vector(type, name) vector<Vk##type> m_##name##s;
#define function(type, name) Vk##type create##type(const Vk##type##CreateInfo& create_info);

//List of all simple vulkan types, these follow a simple pattern that can be automatically generated by the preprocessor
#define functionForAllTypes(func) \
func(DescriptorPool, descriptor_pool)\
func(DescriptorSetLayout, descriptor_set_layout)\
func(Buffer, buffer)\
func(Sampler, sampler)\
func(Image, image)\
func(BufferView, buffer_view)\
func(ImageView, image_view)\
func(RenderPass, render_pass)\
func(Framebuffer, framebuffer)\
func(ShaderModule, shader_module)\
func(PipelineLayout, pipeline_layout)\
func(Semaphore, semaphore)\
func(Fence, fence)\
func(CommandPool, command_pool)


class Device;

/**
 * VulkanAllocator
 *  - Manages all created vulkan objects
 *  - Holds information about device relevant to creating objects
 */
class VulkanAllocator
{
    VkDevice m_device;
    //Two different variables, each one holds some properties of the device
    VkPhysicalDeviceMemoryProperties m_memory_properties;
    VkPhysicalDeviceLimits m_device_limits;

    //define vectors for all simple types, e.g. for 'func(DescriptorPool, descriptor_pool)' this translates to 'vector<VkDescriptorPool> m_descriptor_pools;'
    functionForAllTypes(vector)
    //The following types don't follow the simple creation and destruction rules specified above
    //Pipelines need special creation functions based on their type
    vector<VkPipeline> m_pipelines;
    vector<VkSwapchainKHR> m_swapchains;
    //All allocated memory visible to the GPU
    vector<VkDeviceMemory> m_allocated_memory;
    //Mapped memory is visible to both GPU and CPU
    vector<VkDeviceMemory> m_mapped_memory;
public:
    //create a new allocator and initiate m_memory_properties and m_device_limits
    VulkanAllocator(VkDevice device, VkPhysicalDevice physical_device);

    //declare create functions for all simple types
    functionForAllTypes(function)
    
    /**
     * Allocate memory visible to the GPU
     * @param size memory size in bytes
     * @param type_bits what type does the memory need to have, these are generated automatically by the classes reserving memory for buffers / images
     * @param properties most common VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, valid values VK_MEMORY_PROPERTY_***
     */
    VkDeviceMemory allocateMemory(VkDeviceSize size, uint32_t type_bits, VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    /**
     * Map memory - get a pointer to CPU/GPU shared memory
     * @param memory the memory to get pointer to
     * @param offset offset from memory start
     * @param size how many bytes have to be mapped
     */
    void* mapMemory(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size);
    
    //Create graphics pipeline
    VkPipeline createPipeline(const VkGraphicsPipelineCreateInfo& info);

    //Create compute pipeline
    VkPipeline createPipeline(const VkComputePipelineCreateInfo& info);

    //Create swapchain
    VkSwapchainKHR createSwapchain(const VkSwapchainCreateInfoKHR& info);

    //Get handle to device
    VkDevice getDevice() const;

    //Get a reference to device limits
    const VkPhysicalDeviceLimits& getLimits() const;

    //Destroy all the objects associated with this allocator
    void destroy();
};


/**
 * AllocatorWrapper
 *  - This ensures that there is only one copy of VulkanAllocator created during runtime
 *  - This copy is then available as a global variable
 */
class AllocatorWrapper
{
    VulkanAllocator* m_allocator = nullptr;
public:
    //Set the pointer to the newly created allocator, this function works only once when m_allocator is nullptr, on every other throw it prints an error
    void set(VulkanAllocator& allocator);
    operator VulkanAllocator&();
    VulkanAllocator& get();
};

//the global allocator variable, this limits application to using only one vulkan device. I decided that the simplicity gained from making this variable global far outweighs the rare cases in which one would want to use mor than one device.
extern AllocatorWrapper g_allocator;

//global define, returns current device
#define g_device g_allocator.get().getDevice()

#undef vector
#undef function

#endif