
#include "BindingBridge.hlsli"

NRI_RESOURCE( cbuffer, Constants, b, 0, 0 )
{
    float3 color;
    float scale;
};

struct PushConstants
{
    float transparency;
};

NRI_PUSH_CONSTANTS( PushConstants, pushConstants, 1 );
NRI_RESOURCE( Texture2D, diffuseTexture, t, 0, 1 );
NRI_RESOURCE( SamplerState, linearSampler, s, 0, 1 );

struct outputVS
{
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
};

float4 main( in outputVS input ) : SV_Target
{
    float4 output;
    output.xyz = diffuseTexture.Sample(linearSampler, input.texCoord ).xyz * color;
    output.w = pushConstants.transparency;

    return output;
}
