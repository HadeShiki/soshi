#include "texture_bank.h"

#include "handles.h"

namespace soshi {

std::uint64_t TextureBank::load(SDL_Renderer* renderer, const char* path) {
    if (renderer == nullptr or path == nullptr) return 0;

    SDL_Surface* surface = SDL_LoadSurface(path);
    if (surface == nullptr) return 0;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (texture == nullptr) return 0;

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);

    for (std::uint32_t index = kFirstHandleIndex; index < static_cast<std::uint32_t>(kCapacity); ++index) {
        Slot& slot = slots_[index];
        if (slot.texture != nullptr) continue;

        slot.texture = texture;
        return handle_of(index, slot.generation);
    }

    SDL_DestroyTexture(texture);
    return 0;
}

void TextureBank::reset() {
    // SDL_DestroyRenderer already released the textures; only the slots need clearing.
    for (Slot& slot : slots_) {
        slot.texture = nullptr;
        ++slot.generation;
    }
}

TextureBank::Slot* TextureBank::find(std::uint64_t handle) {
    const std::uint32_t index = handle_index(handle);
    if (index < kFirstHandleIndex or index >= static_cast<std::uint32_t>(kCapacity)) return nullptr;

    Slot& slot = slots_[index];
    if (slot.texture == nullptr or slot.generation != handle_generation(handle)) return nullptr;
    return &slot;
}

void TextureBank::destroy(std::uint64_t handle) {
    Slot* slot = find(handle);
    if (slot == nullptr) return;

    SDL_DestroyTexture(slot->texture);
    slot->texture = nullptr;
    ++slot->generation;
}

bool TextureBank::size_of(std::uint64_t handle, float& width, float& height) {
    Slot* slot = find(handle);
    if (slot == nullptr) return false;
    return SDL_GetTextureSize(slot->texture, &width, &height);
}

SDL_Texture* TextureBank::raw(std::uint32_t index) const {
    if (index < kFirstHandleIndex or index >= static_cast<std::uint32_t>(kCapacity)) return nullptr;
    return slots_[index].texture;
}

}  // namespace soshi
