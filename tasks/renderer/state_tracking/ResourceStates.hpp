#ifndef RESOURCESTATES_HPP
#define RESOURCESTATES_HPP

#include "etna/Vulkan.hpp"
#include "etna/BarrierBehavoir.hpp"

#include <variant>
#include <unordered_map>

namespace my_etna
{

class ResourceStates
{
  using HandleType = uint64_t;
  struct TextureState
  {
    vk::PipelineStageFlags2 piplineStageFlags = {};
    vk::AccessFlags2 accessFlags = {};
    vk::ImageLayout layout = vk::ImageLayout::eUndefined;
    vk::CommandBuffer owner = {};
    bool operator==(const TextureState& other) const = default;
  };
  struct BufferState
  {
    vk::PipelineStageFlags2 piplineStageFlags = {};
    vk::AccessFlags2 accessFlags = {};
    vk::CommandBuffer owner = {};
    bool operator==(const BufferState& other) const = default;
  };

  using State = std::variant<TextureState, BufferState>;
  std::unordered_map<HandleType, State> currentStates;
  std::vector<vk::ImageMemoryBarrier2> imgBarriersToFlush;
  std::vector<vk::BufferMemoryBarrier2> bufBarriersToFlush;

public:
  void setBufferState(
    vk::CommandBuffer com_buffer,
    vk::Buffer buffer,
    vk::PipelineStageFlags2 pipeline_stage_flag,
    vk::AccessFlags2 access_flags,
    ForceSetState force = ForceSetState::eFalse);

  void setExternalTextureState(
    vk::Image image,
    vk::PipelineStageFlags2 pipeline_stage_flag,
    vk::AccessFlags2 access_flags,
    vk::ImageLayout layout);

  void setTextureState(
    vk::CommandBuffer com_buffer,
    vk::Image image,
    vk::PipelineStageFlags2 pipeline_stage_flag,
    vk::AccessFlags2 access_flags,
    vk::ImageLayout layout,
    vk::ImageAspectFlags aspect_flags,
    ForceSetState force = ForceSetState::eFalse);

  void setColorTarget(
    vk::CommandBuffer com_buffer,
    vk::Image image,
    BarrierBehavoir behavoir = BarrierBehavoir::eDefault);
  void setDepthStencilTarget(
    vk::CommandBuffer com_buffer,
    vk::Image image,
    vk::ImageAspectFlags aspect_flags,
    BarrierBehavoir behavoir = BarrierBehavoir::eDefault);
  void setResolveTarget(
    vk::CommandBuffer com_buffer,
    vk::Image image,
    vk::ImageAspectFlags aspect_flags,
    BarrierBehavoir behavoir = BarrierBehavoir::eDefault);

  void flushBarriers(vk::CommandBuffer com_buf);
};

ResourceStates& get_resource_tracker();

void set_state(
  vk::CommandBuffer com_buffer,
  vk::Image image,
  vk::PipelineStageFlags2 pipeline_stage_flags,
  vk::AccessFlags2 access_flags,
  vk::ImageLayout layout,
  vk::ImageAspectFlags aspect_flags,
  ForceSetState force = ForceSetState::eFalse);

void set_state(
  vk::CommandBuffer com_buffer,
  vk::Buffer buffer,
  vk::PipelineStageFlags2 pipeline_stage_flags,
  vk::AccessFlags2 access_flags,
  ForceSetState force = ForceSetState::eFalse);

void flush_barriers(vk::CommandBuffer com_buffer);

} // namespace my_etna

#endif // RESOURCESTATES_HPP
