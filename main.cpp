#include <windows.h>
#include <iostream>
#include <vector>
#include <string>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "vulkan_include_all.h"

using std::vector;
using std::string;
    
uint32_t screen_width = 1400;
uint32_t screen_height = 1400;
string app_name = "Vulkan PBR renderer";



int main(){
    //load vulkan library
    VulkanLibrary library;

    //create instance and window
    const vector<string> instance_extensions {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
    VulkanInstance& instance = library.createInstance(VulkanInstanceCreateInfo().appName(app_name).requestExtensions(instance_extensions));
    Window window = instance.createWindow(screen_width, screen_height, app_name);

    //choose physical device and create logical one, get 2 queues, one for presentation, other for rendering
    PhysicalDevice physical_device = PhysicalDevices(instance).choose();
    Device& device = physical_device.requestExtensions({VK_KHR_SWAPCHAIN_EXTENSION_NAME})\
        .requestScreenSupportQueues({{2, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT}}, window)\
        .createLogicalDevice(instance);
    Queue& queue = device.getQueue(0, 0);
    Queue& present_queue = device.getQueue(0, 1);
    
    //create a swapchain
    Swapchain swapchain = SwapchainInfo(physical_device, window).setUsages(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT).create();
    
    //create a command buffer for rendering
    CommandPool command_pool = CommandPoolInfo(queue.getFamilyIndex()).create();
    CommandBuffer draw_command_buffer = command_pool.allocateBuffer();

    //texture resolution
    uint32_t image_width = 1024;
    uint32_t image_height = 1024;
    //instantiate local object creator with staging buffer large enough to contain any texture
    LocalObjectCreator local_object_creator{queue, 4*image_width*image_height};

    //create color and depth images for rendering, assign them memory. VK_IMAGE_USAGE_SAMPLED_BIT is present so image can be post-processed into final result to be displayed
    Image color_image = ImageInfo(screen_width, screen_height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT).create();
    Image depth_image = ImageInfo(screen_width, screen_height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT).create();
    ImageMemoryObject image_memory({color_image, depth_image}, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    //create image view for color image defined above
    ImageView color_image_view = color_image.createView();
    
    //postprocessing sampler
    VkSampler postprocess_sampler = SamplerInfo().create();

   

    //create pipeline contexts and reserve descriptor sets for each used one
    DirectoryPipelinesContext pipelines_context{"shaders"};
    PipelineContext& PBR_context(pipelines_context.getContext("PBR"));
    PBR_context.reserveDescriptorSets(1, 1);
    PipelineContext& postprocess_context(pipelines_context.getContext("postprocess"));
    postprocess_context.reserveDescriptorSets(1);
    //create descriptor pool
    pipelines_context.createDescriptorPool();
    //reserve given descriptor set counts
    DescriptorSet set_textures, set_uniform_buffer, set_postprocess;
    PBR_context.allocateDescriptorSets(set_textures, set_uniform_buffer); //PBR context has two sets
    postprocess_context.allocateDescriptorSets(set_postprocess);
    //define vectors of sets, used later for binding sets
    vector<VkDescriptorSet> copper_descriptor_sets{set_textures, set_uniform_buffer};
    vector<VkDescriptorSet> postprocess_descriptor_sets{set_postprocess};

    //light positions and colors. Are vec4 due to problems with layouts in shaders (STD430 is not supported by shaders, and library doesn't support padding inside arrays for STD140)
    vector<float> light_positions{
        -10.f, -10.f, 10.f, 0.f,
         10.f, -10.f, 10.f, 0.f,
         10.f, 10.f, 10.f, 0.f,
        -10.f, 10.f, 10.f, 0.f
    };
    vector<float> light_colors{
        1.f, 1.f, 1.f, 0.f,
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f
    };
    //write above data into uniform buffer data
    MixedBufferData copper_uniform_buffer_data = PBR_context.createUniformBufferData(1, "u_light_data");
    copper_uniform_buffer_data.write("light_positions", light_positions).write("light_colors", light_colors);

    //create buffers
    vector<Buffer> buffers = local_object_creator.createBuffers(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VertexCreator::createPlane(15, 15, -15, -15, 30, 30),
        VertexCreator::screenQuadTexCoords(),
        BufferUsageInfo(copper_uniform_buffer_data, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
    );
    enum BufferNames{PLANE_BUFFER, SCREEN_QUAD_BUFFER, COPPER_UNIFORM_BUFFER};

     //load copper plate textures
    ImageSet copper_plate_images{
        local_object_creator,
        ImageSetOptions("textures/Metal_Plate_044_SD/Metal_Plate_044").addDiffuse().addAO().addHeightMap().addNormalMap().addRoughness().addMetallic()
    };
    //sampler for textures
    VkSampler texture_sampler = SamplerInfo().setFilters(VK_FILTER_LINEAR, VK_FILTER_LINEAR).create();
    
    //update all descriptors in textures set
    set_textures.updateDescriptors(
        TextureUpdateInfo{"u_diffuse",   copper_plate_images.getImageView(0), texture_sampler,  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        TextureUpdateInfo{"u_AO",        copper_plate_images.getImageView(1), texture_sampler,  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},          
        TextureUpdateInfo{"u_height",    copper_plate_images.getImageView(2), texture_sampler,  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        TextureUpdateInfo{"u_normal",    copper_plate_images.getImageView(3), texture_sampler,  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        TextureUpdateInfo{"u_roughness", copper_plate_images.getImageView(4), texture_sampler,  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        TextureUpdateInfo{"u_metallic",  copper_plate_images.getImageView(5), texture_sampler,  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
    );
    //update uniform buffer
    set_uniform_buffer.updateDescriptors(
        UniformBufferUpdateInfo{"u_light_data", buffers[COPPER_UNIFORM_BUFFER]}
    );
    //update descriptors in postprocess set
    set_postprocess.updateDescriptors(
        TextureUpdateInfo{"u_render", color_image_view, postprocess_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
    );

    //create push constant data for PBR shader
    PushConstantData copper_push_constants = PBR_context.createPushConstantData();

    //create rendering render pass
    VkRenderPass render_pass = SimpleRenderPassInfo{color_image.getFormat(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, depth_image.getFormat()}.create();
    RenderPassSettings render_pass_settings{screen_width, screen_height, {ClearValue(0.3f, 0.3f, 0.3f), ClearDepthValue(1.f)}};
    //create render framebuffer - uses color and depth images specified above
    VkFramebuffer render_framebuffer = FramebufferInfo(screen_width, screen_height, {color_image_view, depth_image.createView()}, render_pass).create();

    //create postprocess render pass
    VkRenderPass postprocess_render_pass = SimpleRenderPassInfo{swapchain.getFormat(), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR}.create();
    RenderPassSettings postprocess_render_pass_settings{screen_width, screen_height, {ClearValue(0.f, 0.f, 0.f)}};
    //create framebuffer with postprocess render pass
    swapchain.createFramebuffers(postprocess_render_pass);

    //create PBR pipeline - 2 coords per vertex, depth testing enabled
    PipelineInfo pipeline_info{screen_width, screen_height, 1};
    pipeline_info.getVertexInputInfo().addFloatBuffer({2});
    pipeline_info.getDepthStencilInfo().enableDepthTest().enableDepthWrite();
    Pipeline PBR_pipeline = PBR_context.createPipeline(pipeline_info, render_pass);
    
    //create postprocessing pipeline - no depth testing, 2 coords and 2 tex coords per vertexs
    PipelineInfo pipeline_info_postprocess{screen_width, screen_height, 1};
    pipeline_info_postprocess.getVertexInputInfo().addFloatBuffer({2, 2});
    Pipeline postprocess_pipeline = postprocess_context.createPipeline(pipeline_info_postprocess, postprocess_render_pass);

    //create camera, vectors- position, direction, up
    Camera camera{{0.f, 0.f, 1.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, window};
    //matrix responsible for flipping y coordinate before displaying to screen, vulkan has inverted y axis compared to OpenGL and GLM
    glm::mat4 invert_y_mat(1.0);
    invert_y_mat[1][1] = -1;
    //projection matrix - 45 degree angle, ratio width / height, near plane, far plane. Multiply by invert y mat to invert y axis
    glm::mat4 projection = glm::perspective(glm::radians(45.f), 1.f*window.getWidth() / window.getHeight(), 0.1f, 200.f) * invert_y_mat;
    //model view projection matrix (in this case only view projection, because no model matrices are present in this renderer)
    glm::mat4 MVP;

    //synchronization for frame rendering
    SubmitSynchronization frame_synchronization;
    frame_synchronization.setEndFence(Fence());
    
    while (window.running()){
        window.update();

        //estimate framerate to 100 frames per second, one frame lasts 0.01 secs
        camera.update(0.01f);

        //record command buffer
        draw_command_buffer.startRecordPrimary();
        //transition render and depth images to rendering layout and access
        draw_command_buffer.cmdBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,\
            {color_image.createMemoryBarrier(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
             depth_image.createMemoryBarrier(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)});
        
        //begin render pass
        draw_command_buffer.cmdBeginRenderPass(render_pass_settings, render_pass, render_framebuffer);
        //bind pipeline and given descriptor sets
        draw_command_buffer.cmdBindPipeline(PBR_pipeline, copper_descriptor_sets);
        
        //compute MVP matrix
        MVP = projection * camera.view_matrix;
        //write MVP and camera_pos into push constant data
        copper_push_constants.write("MVP", glm::value_ptr(MVP), 16).write("camera_pos", glm::value_ptr(camera.getPosition()), 3);
        //send push constants to the pipeline
        draw_command_buffer.cmdPushConstants(PBR_pipeline, copper_push_constants);
        //bind plane vertex buffer
        draw_command_buffer.cmdBindVertexBuffers({buffers[PLANE_BUFFER]});
        //draw all plane vertices, count = size_in_bytes / sizeof(float) / 2; 2 - element count per vertex
        draw_command_buffer.cmdDrawVertices(buffers[PLANE_BUFFER].getSize() / sizeof(float) / 2);

        //end render pass
        draw_command_buffer.cmdEndRenderPass();

        //get current swapchain image
        SwapchainImage swapchain_image = swapchain.acquireImage();

        //transition render color image to be sampled, and swapchain image to be rendered into
        draw_command_buffer.cmdBarrier(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            {color_image    .createMemoryBarrier(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT),
             swapchain_image.createMemoryBarrier(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)});
        
        //begin postprocess render pass
        draw_command_buffer.cmdBeginRenderPass(postprocess_render_pass_settings, postprocess_render_pass, swapchain_image.getFramebuffer());
        //bind postprocess pipeline and sets
        draw_command_buffer.cmdBindPipeline(postprocess_pipeline, postprocess_descriptor_sets);
        
        //bind postprocess vertex buffer and draw a screen quad
        draw_command_buffer.cmdBindVertexBuffers({buffers[SCREEN_QUAD_BUFFER]});
        draw_command_buffer.cmdDrawVertices(6);

        //end render pass
        draw_command_buffer.cmdEndRenderPass();

        //end command buffer recording
        draw_command_buffer.endRecordPrimary();
        
        //submit command buffer to the queue, wait for it to finish
        queue.submit(draw_command_buffer, frame_synchronization);
        //if it didn't finish for a long time, print error
        if (!frame_synchronization.waitFor(A_SHORT_WHILE)){
            PRINT_ERROR("Waiting for queue failed")
        }

        //reset end fence and present image
        frame_synchronization.getEndFence().reset();
        swapchain.presentImage(swapchain_image, present_queue);

        //reset command buffer and record it once again
        draw_command_buffer.resetBuffer(false);
    }
    //wait for all commands on device to finish
    device.waitFor();
    return 0;
}
