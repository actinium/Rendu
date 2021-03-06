#version 400

in INTERFACE {
	vec3 pos; ///< Position.
} In;

layout(binding = 0) uniform samplerCubeArray screenTexture; ///< Image to blur.

uniform float invHalfSize;  ///< Inverse half size of the sampled texture.
uniform int layer; 			///< The layer to process.
uniform vec3 up; 			///< Face vertical vector.
uniform vec3 right; 		///< Face horizontal vector.

layout(location = 0) out vec4 fragColor; ///< Color.

/** Approximate box blur for cubemap arrays, with a similar effect to the 2D versions.
  This is incorrect as the texel footprint is not taken into account when weighting.
 */
void main(){
	// We have to unroll the box blur loop manually.
	// We shift the fetch direction by moving on the face plane, which is at distance invHalfSize.
	// To do this we use the right and up vectors of the face as displacement directions.
	vec4 color;
	color  = textureLod(screenTexture, vec4(In.pos + invHalfSize * (-2 * right + -2 * up), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * (-2 * right          ), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * (-2 * right +  2 * up), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * (-1 * right + -1 * up), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * (-1 * right +  1 * up), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * (             -2 * up), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos                                       , layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * (              2 * up), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * ( 1 * right + -1 * up), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * ( 1 * right +  1 * up), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * ( 2 * right + -2 * up), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * ( 2 * right          ), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * ( 2 * right +  2 * up), layer), 0.0);

	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * (-2 * right + -1 * up), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * (-2 * right +  1 * up), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * (-1 * right + -2 * up), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * (-1 * right +  0 * up), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * (-1 * right +  2 * up), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * (             -1 * up), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * (              1 * up), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * ( 1 * right + -2 * up), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * ( 1 * right +  0 * up), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * ( 1 * right +  2 * up), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * ( 2 * right + -1 * up), layer), 0.0);
	color += textureLod(screenTexture, vec4(In.pos + invHalfSize * ( 2 * right +  1 * up), layer), 0.0);

	fragColor = color / 25.0;
}
