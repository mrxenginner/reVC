#include "standardConstants.h"

struct VS_in
{
	float4 Position		: POSITION;
	float3 Normal		: NORMAL;
	float2 TexCoord		: TEXCOORD0;
	float4 Prelight		: COLOR0;
};

struct VS_out {
	float4 Position		: POSITION;
	float4 TexCoord0	: TEXCOORD0;	// xy = base uv, z = fog, w = fresnel
	float2 TexCoord1	: TEXCOORD1;	// reflection uv
	float4 Color		: COLOR0;
};

float3 eye			: register(c41);
float4 waveParams	: register(c42);	// x=angle, y=windFactor, z=waveK, w=xOffset
float4 waveParams2	: register(c43);	// x=detail, y=reflStrength, z=fresnelBias, w=time
float4 waterScroll	: register(c44);	// x=scrollU, y=scrollV, z=texScale
float4 waterColor	: register(c45);	// rgb=tint, a=alpha

VS_out main(in VS_in input)
{
	VS_out output;

	// the water frame is a pure translation, so world == object + offset
	float3 wpos = mul(worldMat, input.Position).xyz;

	// macro wave - MUST match CWaterLevel::GetWaterLevel so buoyancy stays in sync
	float phase = (wpos.x + waveParams.w + wpos.y) * waveParams.z + waveParams.x;
	float dz = waveParams.y * sin(phase);
	wpos.z += dz;

	// analytic normal of the macro wave
	float slope = waveParams.y * waveParams.z * cos(phase);
	float3 Normal = float3(-slope, -slope, 1.0);

	// cosmetic high-frequency ripple - normal only, no height
	float dphase = (wpos.x - wpos.y) * (waveParams.z * 3.0) + waveParams2.w * 2.0;
	Normal.xy += float2(cos(dphase), -cos(dphase)) * waveParams2.x;
	Normal = normalize(Normal);

	// translation-only world matrix -> apply the same dz in object space for clip pos
	float4 dispPos = input.Position;
	dispPos.z += dz;
	output.Position = mul(combinedMat, dispPos);

	float3 viewVec = normalize(eye - wpos);

	// scrolling base texture, tiled from world position
	output.TexCoord0.xy = wpos.xy * waterScroll.z + waterScroll.xy;
	output.TexCoord0.z = clamp((output.Position.w - fogEnd)*fogRange, fogDisable, 1.0);

	// reflect the view vector about the normal -> env map uv (same as the vehicle pipe)
	float3 r = Normal*dot(viewVec, Normal)*2.0 - viewVec;
	output.TexCoord1 = r.xy*0.5 + 0.5;

	// Fresnel: little reflection looking straight down, lots at grazing angles
	float f = 1.0 - saturate(dot(viewVec, Normal));
	output.TexCoord0.w = lerp(waveParams2.z, 1.0, f*f*f*f*f) * waveParams2.y;

	output.Color = waterColor;

	return output;
}
