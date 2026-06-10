uniform vec3 u_eye;
uniform vec4 u_waveParams;	// x=angle, y=windFactor, z=waveK, w=xOffset
uniform vec4 u_waveParams2;	// x=detail, y=reflStrength, z=fresnelBias, w=time
uniform vec4 u_waterScroll;	// x=scrollU, y=scrollV, z=texScale, w=unused
uniform vec4 u_waterColor;	// rgb=timecycle tint, a=alpha

#define waveAngle    (u_waveParams.x)
#define windFactor   (u_waveParams.y)
#define waveK        (u_waveParams.z)
#define xOffset      (u_waveParams.w)
#define detailAmpl   (u_waveParams2.x)
#define reflStrength (u_waveParams2.y)
#define fresnelBias  (u_waveParams2.z)
#define waveTime     (u_waveParams2.w)

VSIN(ATTRIB_POS)	vec3 in_pos;

VSOUT vec4 v_color;
VSOUT vec2 v_tex0;
VSOUT vec2 v_tex1;
VSOUT float v_fresnel;
VSOUT float v_fog;

void
main(void)
{
	// the water frame is a pure translation, so world == object + offset
	vec4 world = u_world * vec4(in_pos, 1.0);

	// macro wave - MUST match CWaterLevel::GetWaterLevel so buoyancy stays in sync
	float phase = (world.x + xOffset + world.y) * waveK + waveAngle;
	world.z += windFactor * sin(phase);

	// analytic normal of the macro wave
	float slope = windFactor * waveK * cos(phase);
	vec3 Normal = vec3(-slope, -slope, 1.0);

	// cosmetic high-frequency ripple - perturbs the normal only (no height),
	// so it adds sparkle without desyncing the surface from physics
	float dphase = (world.x - world.y) * (waveK * 3.0) + waveTime * 2.0;
	Normal.xy += vec2(cos(dphase), -cos(dphase)) * detailAmpl;
	Normal = normalize(Normal);

	gl_Position = u_proj * u_view * world;

	vec3 viewVec = normalize(u_eye - world.xyz);

	// scrolling base texture, tiled from world position
	v_tex0 = world.xy * u_waterScroll.z + u_waterScroll.xy;

	v_color = u_waterColor;

	// reflect the view vector about the normal -> env map uv (same as the vehicle pipe)
	vec3 r = Normal * dot(viewVec, Normal) * 2.0 - viewVec;
	v_tex1 = r.xy * 0.5 + 0.5;

	// Fresnel: little reflection looking straight down, lots at grazing angles
	float f = 1.0 - clamp(dot(viewVec, Normal), 0.0, 1.0);
	v_fresnel = mix(fresnelBias, 1.0, f*f*f*f*f) * reflStrength;

	v_fog = DoFog(gl_Position.w);
}
