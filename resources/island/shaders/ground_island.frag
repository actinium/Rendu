#version 400
in INTERFACE {
	vec3 pos; ///< World position
	vec2 uv; ///< Texture coordinates
} In ;

uniform vec3 lightDirection; ///< Sun light direction.
uniform bool debugCol; ///< Use debug color instead of shading.
uniform vec3 camPos; ///< Camera world position.

layout(binding=0) uniform sampler2D heightMap; ///< Terrain height map, height in R, normals in GBA.
layout(binding=1) uniform sampler2D shadowMap; ///<Terrain shadowing factor, ground level in R, water level in G.
layout(binding=2) uniform sampler2D surfaceNoise; ///< Noise surface normal map.
layout(binding=3) uniform sampler2D glitterNoise; ///< Noise specular map.
layout(binding=4) uniform sampler2D sandMapSteep; ///< Normal map for steep dunes.
layout(binding=5) uniform sampler2D sandMapFlat; ///< Normal map for flat regions.

layout (location = 0) out vec3 fragColor; ///< Terrain appearance.
layout (location = 1) out vec3 fragWorldPos; ///< Terrain world position.

/** Shade the terrain by simulating sand appearance on dunes. */
void main(){

	fragWorldPos = In.pos;

	// Get clean normal and height.
	vec4 heightAndNor = textureLod(heightMap, In.uv, 0.0);
	vec3 n = normalize(heightAndNor.yzw);
	vec3 v = normalize(camPos - fragWorldPos);

	// Transition weight between planar and steep regions, for both X and Z orientations.
	float wFlat = pow(abs(n.y), 8.0);
	float wXdir = abs(n.y) > 0.99 ? 0.0 : (abs(n.x)/sqrt(1.0-n.y*n.y));
	vec3 blend = vec3(wXdir*(1.0-wFlat), wFlat, (1.0-wXdir)*(1.0-wFlat));

	// Sand normal map, blended.
	vec3 mapsUV = 3.0*fragWorldPos;
	vec2 mapsUvX = mapsUV.yz;
	vec2 mapsUvY = mapsUV.zx;
	vec2 mapsUvZ = mapsUV.yx;

	vec3 nSandX = normalize(texture(sandMapSteep, mapsUvX).rgb * 2.0 - 1.0);
	vec3 nSandZ = normalize(texture(sandMapSteep, mapsUvZ).rgb * 2.0 - 1.0);
	vec3 nSandY = normalize(texture(sandMapFlat, mapsUvY).rgb * 2.0 - 1.0);

	nSandX = vec3(nSandX.xy + n.zy, abs(nSandX.z) * n.x);
	nSandY = vec3(nSandY.xy + n.xz, abs(nSandY.z) * n.y);
	nSandZ = vec3(nSandZ.xy + n.xy, abs(nSandZ.z) * n.z);

	vec3 nMap = normalize(nSandX.zyx * blend.x + nSandY.xzy * blend.y + nSandZ.xyz * blend.z);

	// Add extra perturbation.
	vec3 surfN = normalize(texture(surfaceNoise, 200.0*In.uv).rgb);
	vec3 finalN = normalize(vec3(nMap.xy + surfN.xy, nMap.z*surfN.z));

	// Shadow
	float shadow = textureLod(shadowMap, In.uv, 0.0).r;
	
	// Colors.
	float colorBlend = texture(surfaceNoise, 150.0*(In.uv + vec2(0.73, 0.19))).a;
	vec3 sandColorDark = vec3(0.3, 0.22, 0.15);
	vec3 sandColorLight = vec3(1.0, 0.516, 0.188);
	vec3 sandColor = mix(sandColorLight, sandColorDark, colorBlend);
	vec3 shadowColor = 0.2 * sandColor;
	vec3 specColor = vec3(1.0, 0.642, 0.378);

	// Tweaked diffuse.
	float diffuse = clamp(2.0 * dot(vec3(1.0,0.3,1.0) * finalN, lightDirection), 0.0, 1.0);
	vec3 color = mix(shadowColor, sandColor, shadow * diffuse);

	// Fresnel.
	float F = pow(1.0 - max(dot(finalN, v), 0.0), 5.0);
	// Specular lobe.
	vec3 h = normalize(v + lightDirection);
	float lobe = pow(max(dot(finalN, h), 0.0), 64.0);
	// Extra glitter
	vec2 glitterUV = 50.0 * In.uv + vec2(0.71, 0.23);
	vec3 glitterN2 = normalize(texture(glitterNoise, glitterUV).rgb);
	vec3 lightBounce = reflect(-lightDirection, glitterN2);
	float glit = smoothstep(0.9, 1.0, dot(lightBounce, v));
	// Total specular.
	vec3 spec = (shadow * 0.9 + 0.1) * (F * specColor + lobe * specColor + glit * specColor);
	
	color += spec;
	
	if(debugCol){
		color = vec3(0.9,0.9,0.9);
	}
	fragColor = color;
}
