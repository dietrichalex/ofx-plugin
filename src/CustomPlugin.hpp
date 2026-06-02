#pragma once

#include "ofxsImageEffect.h"
#include "ofxsMultiThread.h"
#include "ofxsProcessing.h"

class CustomPluginInstance : public OFX::ImageEffect {
public:
    CustomPluginInstance(OfxImageEffectHandle handle);
    void render(const OFX::RenderArguments &args) override;

private:
    OFX::Clip *srcClip_;
    OFX::Clip *dstClip_;
    OFX::DoubleParam* intensityParam_;
};

class CustomPluginFactory : public OFX::PluginFactoryHelper<CustomPluginFactory> {
public:
    CustomPluginFactory();
    void describe(OFX::ImageEffectDescriptor &desc) override;
    void describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context) override;
    OFX::ImageEffect* createInstance(OfxImageEffectHandle handle, OFX::ContextEnum context) override;
};