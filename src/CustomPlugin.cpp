#include "ofxsImageEffect.h"
#include "ofxsMultiThread.h"
#include "ofxsProcessing.h"

class CustomPluginInstance : public OFX::ImageEffect {
public:
    CustomPluginInstance(OfxImageEffectHandle handle) : OFX::ImageEffect(handle) {
        dstClip_ = fetchClip(kOfxImageEffectOutputClipName);
        srcClip_ = fetchClip(kOfxImageEffectSimpleSourceClipName);
    }

    void render(const OFX::RenderArguments &args) override { //TODO
    }

private:
    OFX::Clip *srcClip_;
    OFX::Clip *dstClip_;
};

class MyPluginFactory : public OFX::PluginFactoryHelper<MyPluginFactory> {
public:
    MyPluginFactory() : OFX::PluginFactoryHelper<MyPluginFactory>("com.yourdomain.CustomPlugin", 1, 0) {}

    void describe(OFX::ImageEffectDescriptor &desc) override {
        desc.setPluginGrouping("CustomPlugins");
        desc.setPluginDescription("CustomPlugin");
        desc.addSupportedContext(OFX::eContextFilter);
        desc.addSupportedBitDepth(OFX::eBitDepthFloat);
    }

    void describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context) override {
        OFX::ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
        srcClip->addSupportedComponent(OFX::ePixelComponentRGBA);

        OFX::ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
        dstClip->addSupportedComponent(OFX::ePixelComponentRGBA);
    }

    OFX::ImageEffect* createInstance(OfxImageEffectHandle handle, OFX::ContextEnum context) override {
        return new CustomPluginInstance(handle);
    }
};

void OFX::Plugin::getPluginIDs(OFX::PluginFactoryArray &ids) {
    static MyPluginFactory p;
    ids.push_back(&p);
}