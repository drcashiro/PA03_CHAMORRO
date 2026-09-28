#pragma once
#include "GlResource.h"
namespace pa3 {
class Texture {
public:
    Texture();
    void bind() const { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texture_.get()); }
private:
    GlResource texture_{GlResource::Kind::Texture};
};
}
