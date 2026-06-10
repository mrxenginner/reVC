uniform sampler2D tex0;	// base water texture
uniform sampler2D tex1;	// reflection (EnvMapTex)

FSIN vec4 v_color;
FSIN vec2 v_tex0;
FSIN vec2 v_tex1;
FSIN float v_fresnel;
FSIN float v_fog;

void
main(void)
{
	vec4 base = v_color * texture(tex0, vec2(v_tex0.x, 1.0 - v_tex0.y));
	vec3 refl = texture(tex1, vec2(v_tex1.x, 1.0 - v_tex1.y)).rgb;

	vec3 col = mix(base.rgb, refl, clamp(v_fresnel, 0.0, 1.0));
	col = mix(u_fogColor.rgb, col, v_fog);

	vec4 color;
	color.rgb = col;
	color.a = base.a;

	DoAlphaTest(color.a);

	FRAGCOLOR(color);
}
