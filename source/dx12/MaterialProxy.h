#pragma once

constexpr unsigned int InvalidDescriptorIndex = 0xFFFFFFFF;

struct MaterialProxy
{
    unsigned int AlbedoTextureIndex = InvalidDescriptorIndex;
    unsigned int NormalTextureIndex = InvalidDescriptorIndex;
    unsigned int MetallicTextureIndex = InvalidDescriptorIndex;
    unsigned int RoughnessTextureIndex = InvalidDescriptorIndex;
};