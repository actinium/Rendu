#version 400

#include "common_parallax.glsl"

#define MATERIAL_ID 2

in INTERFACE {
    mat3 tbn; ///< Normal to view matrix.
	vec3 tangentSpacePosition; ///< Tangent space position.
	vec3 viewSpacePosition; ///< View space position.
	vec2 uv; ///< UV coordinates.
} In ;

layout(binding = 0) uniform sampler2D texture0; ///< Albedo.
layout(binding = 1) uniform sampler2D texture1; ///< Normal map.
layout(binding = 2) uniform sampler2D texture2; ///< Effects map.
layout(binding = 3) uniform sampler2D texture3; ///< Local depth map.
uniform mat4 p; ///< Projection matrix.

// Output: the fragment color
layout (location = 0) out vec4 fragColor; ///< Color.
layout (location = 1) out vec3 fragNormal; ///< View space normal.
layout (location = 2) out vec3 fragEffects; ///< Effects.

/** Transfer albedo and effects along with the material ID, and output the final normal 
	(combining geometry normal and normal map) in view space. Apply parallax mapping effect. */
void main(){
	
	vec2 localUV = In.uv;
	vec2 positionShift;
	
	// Compute the new uvs, and use them for the remaining steps.
	vec3 vTangentDir = normalize(- In.tangentSpacePosition);
	localUV = parallax(localUV, vTangentDir, texture3, positionShift);
	// If UV are outside the texture ([0,1]), we discard the fragment.
	if(localUV.x > 1.0 || localUV.y  > 1.0 || localUV.x < 0.0 || localUV.y < 0.0){
		discard;
	}
	
	// Store values.
	vec4 color = texture(texture0, localUV);
	if(color.a <= 0.01){
		discard;
	}
	
	// Flip the up of the local frame for back facing fragments.
	mat3 tbn = In.tbn;
	tbn[2] *= (gl_FrontFacing ? 1.0 : -1.0);
	// Compute the normal at the fragment using the tangent space matrix and the normal read in the normal map.
	vec3 n = texture(texture1,localUV).rgb;
	n = normalize(n * 2.0 - 1.0);
	n = normalize(tbn * n);
	
	fragColor.rgb = color.rgb;
	fragColor.a = float(MATERIAL_ID)/255.0;
	fragNormal.rgb = n * 0.5 + 0.5;
	fragEffects.rgb = texture(texture2,localUV).rgb;
	
	updateFragmentPosition(localUV, positionShift, In.viewSpacePosition, p, tbn, texture3);
	
}
