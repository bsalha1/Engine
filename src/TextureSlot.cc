#include "TextureSlot.h"

static uint8_t _next_texture_slot = 0;

uint8_t Engine::TextureSlot::next_texture_slot()
{
    return _next_texture_slot++;
}