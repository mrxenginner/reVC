#ifndef __GTA_CUSTOMPIPES_H__
#define __GTA_CUSTOMPIPES_H__

#ifdef LIBRW
#ifdef EXTENDED_PIPELINES

namespace CustomPipes {


extern rw::TexDictionary *neoTxd;

struct CustomMatExt
{
	rw::Texture *glossTex;
	bool haveGloss;
};
extern rw::int32 CustomMatOffset;
inline CustomMatExt *GetCustomMatExt(rw::Material *mat) {
	return PLUGINOFFSET(CustomMatExt, mat, CustomMatOffset);
}


struct Color
{
	float r, g, b, a;
	Color(void) {}
	Color(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}
};

class InterpolatedValue
{
public:
	virtual void Read(char *s, int line, int field) = 0;
};

class InterpolatedFloat : public InterpolatedValue
{
public:
	float data[24][NUMWEATHERS];
	float curInterpolator;
	float curVal;

	InterpolatedFloat(float init);
	void Read(char *s, int line, int field);
	float Get(void);
};

class InterpolatedColor : public InterpolatedValue
{
public:
	Color data[24][NUMWEATHERS];
	float curInterpolator;
	Color curVal;

	InterpolatedColor(const Color &init);
	void Read(char *s, int line, int field);
	Color Get(void);
};

class InterpolatedLight : public InterpolatedColor
{
public:
	InterpolatedLight(const Color &init) : InterpolatedColor(init) {}
	void Read(char *s, int line, int field);
};

char *ReadTweakValueTable(char *fp, InterpolatedValue &interp);





void CustomPipeRegister(void);
void CustomPipeRegisterGL(void);
void CustomPipeInit(void);
void CustomPipeShutdown(void);
void SetTxdFindCallback(void);

extern bool bRenderingEnvMap;
extern int32 EnvMapSize;
extern rw::Camera *EnvMapCam;
extern rw::Texture *EnvMapTex;
extern rw::Texture *EnvMaskTex;
void EnvMapRender(void);

enum {
	VEHICLEPIPE_MATFX,
	VEHICLEPIPE_NEO
};
extern int32 VehiclePipeSwitch;
extern float VehicleShininess;
extern float VehicleSpecularity;
extern InterpolatedFloat Fresnel;
extern InterpolatedFloat Power;
extern InterpolatedLight DiffColor;
extern InterpolatedLight SpecColor;
extern rw::ObjPipeline *vehiclePipe;
void CreateVehiclePipe(void);
void DestroyVehiclePipe(void);
void AttachVehiclePipe(rw::Atomic *atomic);
void AttachVehiclePipe(rw::Clump *clump);

extern bool LightmapEnable;
extern float LightmapMult;
extern InterpolatedFloat WorldLightmapBlend;
extern rw::ObjPipeline *worldPipe;
void CreateWorldPipe(void);
void DestroyWorldPipe(void);
void AttachWorldPipe(rw::Atomic *atomic);
void AttachWorldPipe(rw::Clump *clump);

extern bool GlossEnable;
extern float GlossMult;
extern rw::ObjPipeline *glossPipe;
void CreateGlossPipe(void);
void DestroyGlossPipe(void);
void AttachGlossPipe(rw::Atomic *atomic);
void AttachGlossPipe(rw::Clump *clump);
rw::Texture *GetGlossTex(rw::Material *mat);

extern bool RimlightEnable;
extern float RimlightMult;
extern InterpolatedColor RampStart;
extern InterpolatedColor RampEnd;
extern InterpolatedFloat Offset;
extern InterpolatedFloat Scale;
extern InterpolatedFloat Scaling;
extern rw::ObjPipeline *rimPipe;
extern rw::ObjPipeline *rimSkinPipe;
void CreateRimLightPipes(void);
void DestroyRimLightPipes(void);
void AttachRimPipe(rw::Atomic *atomic);
void AttachRimPipe(rw::Clump *clump);

#ifdef CUSTOM_SHADER_WATER
// CUSTOM: GPU shader water pipe. Waves are displaced in the vertex shader and
// shaded (Fresnel + EnvMapTex reflection) in the pixel/fragment shader.
extern bool WaterPipeEnable;
extern float WaterReflectivity;	// reflection strength [0,1]
extern float WaterFresnelBias;	// minimum reflection at normal incidence [0,1]
extern float WaterDetail;		// cosmetic high-frequency normal ripple amount
extern rw::ObjPipeline *waterPipe;
void CreateWaterPipe(void);
void DestroyWaterPipe(void);
void AttachWaterPipe(rw::Atomic *atomic);
void DetachWaterPipe(rw::Atomic *atomic);
#endif

}

#endif

namespace WorldRender{
extern int numBlendInsts[3];
void AtomicFirstPass(RpAtomic *atomic, int pass);
void AtomicFullyTransparent(RpAtomic *atomic, int pass, int fadeAlpha);
void RenderBlendPass(int pass);
}

#endif

#endif // __GTA_CUSTOMPIPES_H__
