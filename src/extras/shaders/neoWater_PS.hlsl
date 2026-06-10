struct VS_out {
	float4 Position		: POSITION;
	float4 TexCoord0	: TEXCOORD0;	// xy = base uv, z = fog, w = fresnel
	float2 TexCoord1	: TEXCOORD1;	// reflection uv
	float4 Color		: COLOR0;
};

sampler2D tex0 : register(s0);	// base water texture
sampler2D tex1 : register(s1);	// reflection (EnvMapTex)

float4 fogColor : register(c0);

float4 main(VS_out input) : COLOR
{
	float4 base = input.Color * tex2D(tex0, input.TexCoord0.xy);
	float3 refl = tex2D(tex1, input.TexCoord1).rgb;

	float3 col = lerp(base.rgb, refl, saturate(input.TexCoord0.w));
	col = lerp(fogColor.rgb, col, input.TexCoord0.z);

	float4 color;
	color.rgb = col;
	color.a = base.a;

	return color;
}
