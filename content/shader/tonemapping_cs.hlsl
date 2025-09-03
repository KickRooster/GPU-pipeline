// Tone mapping compute shader
// Based on UE5's compute-based post-processing approach

Texture2D<float4> InputTexture : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);

cbuffer ViewportConstants : register(b0)
{
    float2 InputSize;   // Input texture size (as float for C++ compatibility)
    float2 OutputSize;  // Output texture size (as float for C++ compatibility)
    float Exposure;     // Exposure adjustment (default: 1.0)
    float Contrast;     // Contrast adjustment (default: 1.0)  
};

// Improved ACES Filmic (from UE4/UE5)
float3 ACESFilmic(float3 x)
{
    // Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// Accurate sRGB gamma correction (better than simple pow 2.2)
float3 LinearToSRGB(float3 color)
{
    color = saturate(color);
    
    // Accurate sRGB curve
    return lerp(
        12.92 * color,                                    // Linear part
        1.055 * pow(color, 1.0 / 2.4) - 0.055,          // Gamma part  
        step(0.0031308, color)                            // Threshold
    );
}

// Simple contrast adjustment
float3 ApplyContrast(float3 color, float contrast)
{
    return saturate((color - 0.5) * contrast + 0.5);
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    // Early exit if outside texture bounds (InputSize == OutputSize for 1:1 mapping)
    if (id.x >= (uint)InputSize.x || id.y >= (uint)InputSize.y)
        return;
    
    // Sample HDR input from input texture
    float4 originalColor = InputTexture[id.xy];
    float3 hdrColor = originalColor.rgb;
    
    // Apply exposure adjustment first
    hdrColor *= Exposure;
    
    // Apply ACES Filmic tone mapping (industry standard, used by UE5)
    float3 toneMappedColor = ACESFilmic(hdrColor);
    
    // Apply contrast adjustment
    toneMappedColor = ApplyContrast(toneMappedColor, Contrast);
    
    // Apply gamma correction
    float3 finalColor = LinearToSRGB(toneMappedColor);
    
    // Write to output texture
    OutputTexture[id.xy] = float4(finalColor, originalColor.a);
}