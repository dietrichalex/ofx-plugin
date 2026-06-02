#include "CustomPlugin.hpp"
#include <cmath>

CustomPluginInstance::CustomPluginInstance(OfxImageEffectHandle handle) : OFX::ImageEffect(handle) {
    dstClip_ = fetchClip(kOfxImageEffectOutputClipName);
    srcClip_ = fetchClip(kOfxImageEffectSimpleSourceClipName);
}

inline float fastHash(uint32_t x, uint32_t y, uint32_t frame) {
    uint32_t seed = x + (y << 16) + (frame << 8);
    seed ^= seed >> 16;
    seed *= 0x85ebca6b;
    seed ^= seed >> 13;
    seed *= 0xc2b2ae35;
    seed ^= seed >> 16;
    // Normalize to 0.0 -> 1.0
    return static_cast<float>(seed) / static_cast<float>(0xFFFFFFFF);
}

void CustomPluginInstance::render(const OFX::RenderArguments &args) {
    std::unique_ptr<OFX::Image> srcImg(srcClip_->fetchImage(args.time));
    std::unique_ptr<OFX::Image> dstImg(dstClip_->fetchImage(args.time));

    if (!srcImg || !dstImg) return; 

    char* srcData = static_cast<char*>(srcImg->getPixelData());
    char* dstData = static_cast<char*>(dstImg->getPixelData());
    
    int srcRowBytes = srcImg->getRowBytes();
    int dstRowBytes = dstImg->getRowBytes();
    
    OfxRectI srcBounds = srcImg->getBounds();
    OfxRectI dstBounds = dstImg->getBounds();
    OfxRectI renderWindow = args.renderWindow;

    uint32_t frame = static_cast<uint32_t>(args.time);
    float grainIntensity = 0.15f;

    for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
        char* srcRowStart = srcData + (y - srcBounds.y1) * srcRowBytes;
        char* dstRowStart = dstData + (y - dstBounds.y1) * dstRowBytes;

        for (int x = renderWindow.x1; x < renderWindow.x2; ++x) {
            float* srcPixel = reinterpret_cast<float*>(srcRowStart + (x - srcBounds.x1) * 4 * sizeof(float));
            float* dstPixel = reinterpret_cast<float*>(dstRowStart + (x - dstBounds.x1) * 4 * sizeof(float));

            float r = srcPixel[0];
            float g = srcPixel[1];
            float b = srcPixel[2];
            float a = srcPixel[3];

            float noise = fastHash(x, y, frame) - 0.5f;

            float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;

            float midtoneMask = 1.0f - std::abs(lum - 0.5f) * 2.0f;
            midtoneMask = std::clamp(midtoneMask, 0.0f, 1.0f);

            float finalNoise = noise * grainIntensity * midtoneMask;

            dstPixel[0] = std::max(0.0f, r + finalNoise);
            dstPixel[1] = std::max(0.0f, g + finalNoise);
            dstPixel[2] = std::max(0.0f, b + finalNoise);
            dstPixel[3] = a;
        }
    }
}


CustomPluginFactory::CustomPluginFactory() 
    : OFX::PluginFactoryHelper<CustomPluginFactory>("com.yourdomain.CustomPlugin", 1, 0) {}

void CustomPluginFactory::describe(OFX::ImageEffectDescriptor &desc) {
    desc.setPluginGrouping("Custom Plugins");
    desc.setPluginDescription("A basic CPU OpenFX film grain plugin.");
    desc.addSupportedContext(OFX::eContextFilter);
    desc.addSupportedBitDepth(OFX::eBitDepthFloat);
}

void CustomPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context) {
    OFX::ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(OFX::ePixelComponentRGBA);

    OFX::ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(OFX::ePixelComponentRGBA);
}

OFX::ImageEffect* CustomPluginFactory::createInstance(OfxImageEffectHandle handle, OFX::ContextEnum context) {
    return new CustomPluginInstance(handle);
}

void OFX::Plugin::getPluginIDs(OFX::PluginFactoryArray &ids) {
    static CustomPluginFactory p;
    ids.push_back(&p);
}