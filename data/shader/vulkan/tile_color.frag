#version 450
#extension GL_ARB_separate_shader_objects : enable

#ifdef TW_TILE_TEXTURED
layout(binding = 0) uniform sampler2DArray gTextureSampler;
#endif

#ifdef TW_TILE_TEXTURED
layout (location = 0) noperspective in vec3 TexCoord;
#endif
layout (location = 1) in vec4 inColor;

layout (location = 0) out vec4 FragClr;
void main()
{
#ifdef TW_TILE_TEXTURED
	vec4 TexColor = texture(gTextureSampler, TexCoord.xyz);
	FragClr = TexColor * inColor; 
#else
	FragClr = inColor;
#endif
}
