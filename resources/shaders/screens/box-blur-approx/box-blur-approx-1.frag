#version 330

// Input: UV coordinates
in INTERFACE {
	vec2 uv;
} In ;

// Uniforms: the texture, inverse of the screen size.
layout(binding = 0) uniform sampler2D screenTexture;

// Output: the fragment color
out float fragColor;


void main(){
	
	// We have to unroll the box blur loop manually.
	
	float color = textureOffset(screenTexture, In.uv, ivec2(-2,-2)).r;
	color += textureOffset(screenTexture, In.uv, ivec2(-2,0)).r;
	color += textureOffset(screenTexture, In.uv, ivec2(-2,2)).r;
	
	color += textureOffset(screenTexture, In.uv, ivec2(-1,-1)).r;
	color += textureOffset(screenTexture, In.uv, ivec2(-1,1)).r;
	
	color += textureOffset(screenTexture, In.uv, ivec2(0,-2)).r;
	color += textureOffset(screenTexture, In.uv, ivec2(0,0)).r;
	color += textureOffset(screenTexture, In.uv, ivec2(0,2)).r;
	
	color += textureOffset(screenTexture, In.uv, ivec2(1,-1)).r;
	color += textureOffset(screenTexture, In.uv, ivec2(1,1)).r;
	
	color += textureOffset(screenTexture, In.uv, ivec2(2,-2)).r;
	color += textureOffset(screenTexture, In.uv, ivec2(2,0)).r;
	color += textureOffset(screenTexture, In.uv, ivec2(2,2)).r;
	
	fragColor = color / 13.0;
}
