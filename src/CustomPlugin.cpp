#include "CustomPlugin.hpp"

CustomPluginInstance::CustomPluginInstance(OfxImageEffectHandle handle) : OFX::ImageEffect(handle) {
    dstClip_ = fetchClip(kOfxImageEffectOutputClipName);
    srcClip_ = fetchClip(kOfxImageEffectSimpleSourceClipName);

    intensityParam_ = fetchDoubleParam("intensity");
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
    // 1. Lock memory buffers
    std::unique_ptr<OFX::Image> srcImg(srcClip_->fetchImage(args.time));
    std::unique_ptr<OFX::Image> dstImg(dstClip_->fetchImage(args.time));
    if (!srcImg || !dstImg) return;

    // 2. Fetch raw pointers and memory layout
    char* srcData = static_cast<char*>(srcImg->getPixelData());
    char* dstData = static_cast<char*>(dstImg->getPixelData());
    int srcRowBytes = srcImg->getRowBytes();
    int dstRowBytes = dstImg->getRowBytes();
    OfxRectI srcBounds = srcImg->getBounds();
    OfxRectI dstBounds = dstImg->getBounds();
    OfxRectI renderWin = args.renderWindow;

    // 3. Algorithm Parameters & UI Fetch
    float muR = 1.5f; // Average grain radius in pixels (keep small for performance)
    int N = 4;        // Monte Carlo iterations
    
    // Fetch the slider value (assuming you bound intensityParam_ in the constructor)
    double uiIntensity;
    intensityParam_->getValueAtTime(args.time, uiIntensity);
    float blendIntensity = static_cast<float>(uiIntensity);

    // Cell size logic derived from the IPOL paper
    float cellSize = 1.0f / std::ceil(1.0f / muR);
    uint32_t frameTime = static_cast<uint32_t>(args.time);
    
    // 4. Precompute Gaussian offsets for the N Monte Carlo iterations
    std::vector<float> xi_x(N), xi_y(N);
    std::mt19937 mcGen(frameTime * 1000); 
    std::normal_distribution<float> mcDist(0.0f, 0.8f); // 0.8 is the paper's default sigma
    for(int k = 0; k < N; ++k) {
        xi_x[k] = mcDist(mcGen);
        xi_y[k] = mcDist(mcGen);
    }

    // 5. Main Pixel Processing Loop
    for (int y = renderWin.y1; y < renderWin.y2; ++y) {
        char* dstRow = dstData + (y - dstBounds.y1) * dstRowBytes;
        
        for (int x = renderWin.x1; x < renderWin.x2; ++x) {
            float* dstPixel = reinterpret_cast<float*>(dstRow + (x - dstBounds.x1) * 4 * sizeof(float));
            float hitCount = 0.0f;

            // N Monte Carlo Sample Evaluation
            for (int k = 0; k < N; ++k) {
                float px = static_cast<float>(x) + xi_x[k];
                float py = static_cast<float>(y) + xi_y[k];

                // Determine overlapping cells
                int minCx = std::floor((px - muR) / cellSize);
                int maxCx = std::floor((px + muR) / cellSize);
                int minCy = std::floor((py - muR) / cellSize);
                int maxCy = std::floor((py + muR) / cellSize);

                bool ptCovered = false;

                // Evaluate overlapping cells
                for (int cy = minCy; cy <= maxCy && !ptCovered; ++cy) {
                    for (int cx = minCx; cx <= maxCx && !ptCovered; ++cx) {
                        
                        // Clamp coordinates to fetch input intensity safely
                        int fetchX = std::clamp(static_cast<int>(cx * cellSize), srcBounds.x1, srcBounds.x2 - 1);
                        int fetchY = std::clamp(static_cast<int>(cy * cellSize), srcBounds.y1, srcBounds.y2 - 1);
                        
                        char* srcRow = srcData + (fetchY - srcBounds.y1) * srcRowBytes;
                        float* srcPixel = reinterpret_cast<float*>(srcRow + (fetchX - srcBounds.x1) * 4 * sizeof(float));
                        
                        // Calculate Rec709 Luminance and clamp to prevent log(0) issues
                        float u = 0.2126f * srcPixel[0] + 0.7152f * srcPixel[1] + 0.0722f * srcPixel[2];
                        u = std::clamp(u, 0.0f, 0.99f); 
                        
                        // Poisson parameter math from the IPOL Boolean model
                        float lambda = -((cellSize * cellSize) / (3.14159f * (muR * muR))) * std::log(1.0f - u);
                        
                        // CRITICAL FIX: Prevent MSVC crash on pure black cells
                        if (lambda <= 0.00001f) {
                            continue; 
                        }
                        
                        // Deterministic Cell PRNG using std::mt19937
                        uint32_t cellSeed = static_cast<uint32_t>(cx) * 73856093 ^ static_cast<uint32_t>(cy) * 19349663 ^ frameTime;
                        std::mt19937 cellGen(cellSeed);
                        
                        std::poisson_distribution<int> poisson(lambda);
                        std::uniform_real_distribution<float> uniform(0.0f, cellSize);
                        
                        int nGrains = poisson(cellGen);
                        
                        // Evaluate grains in this cell
                        for (int g = 0; g < nGrains; ++g) {
                            // Grain coordinates relative to the output image
                            float gx = (cx * cellSize) + uniform(cellGen);
                            float gy = (cy * cellSize) + uniform(cellGen);
                            
                            float dx = gx - px;
                            float dy = gy - py;
                            
                            // Euclidean distance check
                            if ((dx*dx + dy*dy) < (muR * muR)) {
                                hitCount += 1.0f;
                                ptCovered = true;
                                break;
                            }
                        }
                    }
                }
            }
            
            // 6. Color Compositing & Output
            
            // Fetch original pixel for this exact (x, y) location
            char* currentSrcRow = srcData + (y - srcBounds.y1) * srcRowBytes;
            float* currentSrcPixel = reinterpret_cast<float*>(currentSrcRow + (x - srcBounds.x1) * 4 * sizeof(float));
            
            // Calculate simulated physical luma
            float simLuma = hitCount / static_cast<float>(N);
            
            // Calculate original continuous luma
            float origLuma = 0.2126f * currentSrcPixel[0] + 0.7152f * currentSrcPixel[1] + 0.0722f * currentSrcPixel[2];
            
            // Determine the ratio between the simulated film density and the original digital density.
            // (Add a small epsilon to prevent division by zero in absolute black)
            float densityRatio = simLuma / (origLuma + 0.0001f);
            
            // Modulate the density ratio by the UI slider (Intensity)
            // If blendIntensity = 0.0, ratio is forced to 1.0 (no change).
            // If blendIntensity = 1.0, ratio is fully applied.
            float finalModulation = 1.0f + ((densityRatio - 1.0f) * blendIntensity);
            
            // Apply physical grain mask to the original color channels and write to output
            dstPixel[0] = std::clamp(currentSrcPixel[0] * finalModulation, 0.0f, 1.0f);
            dstPixel[1] = std::clamp(currentSrcPixel[1] * finalModulation, 0.0f, 1.0f);
            dstPixel[2] = std::clamp(currentSrcPixel[2] * finalModulation, 0.0f, 1.0f);
            dstPixel[3] = currentSrcPixel[3]; // Pass alpha untouched
        }
    }
}


CustomPluginFactory::CustomPluginFactory() 
    : OFX::PluginFactoryHelper<CustomPluginFactory>("com.yourdomain.CustomPlugin", 1, 0) {}

void CustomPluginFactory::describe(OFX::ImageEffectDescriptor &desc) {
    desc.setPluginGrouping("Custom Plugins");
    desc.setLabels("Cinematic Grain", "CineGrain", "Cinematic Film Grain Generator");
    
    desc.setPluginDescription("A basic CPU OpenFX film grain plugin.");
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
    intensity->setDefault(0.15);
    intensity->setRange(0.0, 10.0);       // Absolute hard limits (user cannot type values outside this)
    intensity->setDisplayRange(0.0, 1.0); // The visual limits of the slider UI
    intensity->setHint("Controls the opacity of the procedural grain.");
}

OFX::ImageEffect* CustomPluginFactory::createInstance(OfxImageEffectHandle handle, OFX::ContextEnum context) {
    return new CustomPluginInstance(handle);
}

void OFX::Plugin::getPluginIDs(OFX::PluginFactoryArray &ids) {
    static CustomPluginFactory p;
    ids.push_back(&p);
}