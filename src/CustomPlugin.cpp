#include "CustomPlugin.hpp"
#include <memory>
#include <cuda_runtime.h>

CustomPluginInstance::CustomPluginInstance(OfxImageEffectHandle handle) : OFX::ImageEffect(handle) {
    dstClip_ = fetchClip(kOfxImageEffectOutputClipName);
    srcClip_ = fetchClip(kOfxImageEffectSimpleSourceClipName);
    intensityParam_ = fetchDoubleParam("intensity");
}

void CustomPluginInstance::render(const OFX::RenderArguments &args) {
    std::unique_ptr<OFX::Image> srcImg(srcClip_->fetchImage(args.time));
    std::unique_ptr<OFX::Image> dstImg(dstClip_->fetchImage(args.time));
    if (!srcImg || !dstImg) return;

    OfxRectI renderWin = args.renderWindow;
    int width = renderWin.x2 - renderWin.x1;
    int height = renderWin.y2 - renderWin.y1;
    size_t byteSize = width * height * 4 * sizeof(float);

    // Get Host Pointers
    float* h_src = reinterpret_cast<float*>(srcImg->getPixelData());
    float* h_dst = reinterpret_cast<float*>(dstImg->getPixelData());

    // Fetch UI parameters
    double uiIntensity;
    intensityParam_->getValueAtTime(args.time, uiIntensity);
    
    // Allocate Device Pointers
    float *d_src, *d_dst;
    cudaMalloc(&d_src, byteSize);
    cudaMalloc(&d_dst, byteSize);

    // Copy Host to Device
    cudaMemcpy(d_src, h_src, byteSize, cudaMemcpyHostToDevice);

    // Launch CUDA Kernel
    RunCudaGrainKernel(d_src, d_dst, width, height, 1.5f, 4, static_cast<float>(uiIntensity), static_cast<uint32_t>(args.time));

    // Copy Device to Host
    cudaMemcpy(h_dst, d_dst, byteSize, cudaMemcpyDeviceToHost);

    // Free Device Memory
    cudaFree(d_src);
    cudaFree(d_dst);
}

CustomPluginFactory::CustomPluginFactory() 
    : OFX::PluginFactoryHelper<CustomPluginFactory>("com.yourdomain.CustomPlugin", 1, 0) {}

void CustomPluginFactory::describe(OFX::ImageEffectDescriptor &desc) {
    desc.setPluginGrouping("Custom Plugins");
    desc.setLabels("Cinematic Grain (CUDA)", "CineGrainCU", "Cinematic Film Grain Generator");
    desc.setPluginDescription("GPU-accelerated physical film grain emulator.");
    desc.addSupportedContext(OFX::eContextFilter);
    desc.addSupportedBitDepth(OFX::eBitDepthFloat);
}

void CustomPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context) {
    OFX::ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(OFX::ePixelComponentRGBA);

    OFX::ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(OFX::ePixelComponentRGBA);

    OFX::DoubleParamDescriptor* intensity = desc.defineDoubleParam("intensity");
    intensity->setLabels("Grain Intensity", "Intensity", "Intensity");
    intensity->setDefault(0.5);
    intensity->setRange(0.0, 5.0);
    intensity->setDisplayRange(0.0, 1.0);
}

OFX::ImageEffect* CustomPluginFactory::createInstance(OfxImageEffectHandle handle, OFX::ContextEnum context) {
    return new CustomPluginInstance(handle);
}

void OFX::Plugin::getPluginIDs(OFX::PluginFactoryArray &ids) {
    static CustomPluginFactory p;
    ids.push_back(&p);
}