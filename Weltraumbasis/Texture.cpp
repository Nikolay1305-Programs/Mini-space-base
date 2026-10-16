#include "Texture.h"
#include <iostream>

#include <SDL.h>

#if __has_include(<SDL_image.h>)
#include <SDL_image.h>
#else
#include <SDL2/SDL_image.h>
#endif

static GLuint UploadRGBA(SDL_Surface* s)
{
    if (!s) return 0;

    SDL_Surface* rgba = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_RGBA32, 0);
    if (!rgba) {
        std::cout << "SDL_ConvertSurfaceFormat failed: " << SDL_GetError() << "\n";
        return 0;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // филтри (с mipmaps)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // wrap (по-чисто за ръбове)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // пазим/връщаме alignment, за да не чупим други upload-и
    GLint oldUnpack = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &oldUnpack);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    glPixelStorei(GL_UNPACK_ALIGNMENT, oldUnpack);

    glBindTexture(GL_TEXTURE_2D, 0);
    SDL_FreeSurface(rgba);
    return tex;
}

GLuint LoadTexture2D_RGBA(const std::string& path)
{
    SDL_Surface* s = IMG_Load(path.c_str());
    if (!s) {
        std::cout << "IMG_Load failed: " << path << " | " << IMG_GetError() << "\n";
        return 0;
    }

    GLuint t = UploadRGBA(s);
    SDL_FreeSurface(s);
    return t;
}

GLuint LoadTexture2D_FromMemory(const unsigned char* data, size_t sizeBytes)
{
    SDL_RWops* rw = SDL_RWFromConstMem(data, (int)sizeBytes);
    if (!rw) {
        std::cout << "SDL_RWFromConstMem failed: " << SDL_GetError() << "\n";
        return 0;
    }

    SDL_Surface* s = IMG_Load_RW(rw, 1);
    if (!s) {
        std::cout << "IMG_Load_RW failed: " << IMG_GetError() << "\n";
        return 0;
    }

    GLuint t = UploadRGBA(s);
    SDL_FreeSurface(s);
    return t;
}
