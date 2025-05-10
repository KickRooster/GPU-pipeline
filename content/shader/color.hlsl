//***************************************************************************************
// color.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Transforms and colors geometry.
//***************************************************************************************

cbuffer cbCamera : register(b0)
{
	float4x4 gViewProj; 
};

cbuffer cbStaticMeshActor : register(b1)
{
	float4x4 gWorld;
};

struct VertexIn
{
	float3 PosL  : POSITION;
	float3 Normal  : NORMAL;
    float4 Color : COLOR;
	float2 UV : TEXCOORD;
};

struct VertexOut
{
	float4 PosH  : SV_POSITION;
	float3 NormalW : NORMAL;
    float4 Color : COLOR;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	
	float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
	
	float3x3 worldInvTranspose = transpose((float3x3)gWorld);
	vout.NormalW = normalize(mul(vin.Normal, worldInvTranspose));
	
	vout.PosH = mul(posW, gViewProj);
	
	vout.Color.xyz = abs(vout.NormalW);
	vout.Color.w = 1.0;
	
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return pin.Color;
}
